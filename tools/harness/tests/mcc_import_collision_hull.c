#include "harness.h"
#include <math.h>

/* (the game's points and vectors, not the harness's) */
#define real_point3d game_real_point3d
#define real_vector3d game_real_vector3d
typedef unsigned short word;
#include "types.inc"
#include "constants.inc"
#define square_root(x) ((real)sqrt(x))
#define csmemcpy(destination, source, size) memcpy(destination, source, size)

static void *mcc_import_operations_allocate(unsigned long size)
{
	return calloc(1, size);
}

#include "under_test.inc"

static struct mcc_collision_hull hull;

static void point(real x, real y, real z)
{
	real_point3d p;

	p.n[0] = x;
	p.n[1] = y;
	p.n[2] = z;
	CHECK(mcc_collision_point_add(&hull, &p), "a point added");
}

/* the hull is closed and convex: each face's corners on its plane and
counter-clockwise seen from outside (its normal outward), every corner and
point behind every plane, each surface's ring of edges going round its
corners, each edge between two faces, no face of more corners than a
surface takes */
static void hull_check(void)
{
	long face, index;

	for (face = 0; face < hull.plane_count; face++)
	{
		real_plane3d const *plane = &hull.planes[face];
		short count = hull.face_vertex_counts[face], i;
		long edge = hull.face_first_edges[face];
		real area[3] = { 0.0f, 0.0f, 0.0f };

		CHECK(count >= 3 && count <= MCC_COLLISION_SURFACE_VERTICES, "face %ld has %d corners", face, count);
		for (i = 0; i < count; i++)
		{
			real_point3d const *a = &hull.vertices[hull.face_vertices[face][i]];
			real_point3d const *b = &hull.vertices[hull.face_vertices[face][(i + 1) % count]];

			CHECK(fabs(mcc_collision_distance(plane, a)) < 0.003f, "face %ld's corner %d is on its plane", face, i);
			area[0] += a->n[1] * b->n[2] - a->n[2] * b->n[1];
			area[1] += a->n[2] * b->n[0] - a->n[0] * b->n[2];
			area[2] += a->n[0] * b->n[1] - a->n[1] * b->n[0];
		}
		CHECK(area[0] * plane->n.n[0] + area[1] * plane->n.n[1] + area[2] * plane->n.n[2] > 0.0f,
			"face %ld goes counter-clockwise round its outward normal", face);
		for (i = 0; i < count; i++)
		{
			struct collision_edge const *e = &hull.edges[edge];
			boolean reverse = e->surface_indices[1] == face;

			CHECK(e->surface_indices[reverse] == face, "face %ld's ring holds its own edges", face);
			CHECK(e->vertex_indices[reverse] == hull.face_vertices[face][i], "face %ld's ring goes round its corners", face);
			edge = e->edge_indices[reverse];
		}
		CHECK(edge == hull.face_first_edges[face], "face %ld's ring closes", face);
		for (index = 0; index < hull.vertex_count; index++)
			CHECK(mcc_collision_distance(plane, &hull.vertices[index]) < 0.003f, "corner %ld is behind face %ld", index, face);
		for (index = 0; index < hull.point_count; index++)
			CHECK(mcc_collision_distance(plane, &hull.points[index]) < 0.003f, "point %ld is behind face %ld", index, face);
	}
	for (index = 0; index < hull.edge_count; index++)
	{
		CHECK(hull.edges[index].surface_indices[0] != hull.edges[index].surface_indices[1] &&
			hull.edges[index].surface_indices[1] != NONE, "edge %ld is between two faces", index);
	}
	CHECK(hull.vertex_count - hull.edge_count + hull.plane_count == 2, "closed (corners - edges + faces = 2)");
}

/* the BSP as the game reads it: a point inside is in the solid (NONE), one
outside in a leaf whose surface's plane has it in front */
static long bsp_test(struct collision_bsp const *bsp, real x, real y, real z)
{
	long node = 0;
	real_point3d p;

	p.n[0] = x;
	p.n[1] = y;
	p.n[2] = z;
	do
	{
		struct bsp3d_node const *n = (struct bsp3d_node const *)bsp->bsp3d.nodes.address + node;
		real_plane3d const *plane = (real_plane3d const *)bsp->bsp3d.planes.address + n->plane_designator;

		node = n->children[mcc_collision_distance(plane, &p) >= 0.0f];
	}
	while (!(node & MCC_COLLISION_LEAF_BIT));
	if (node == NONE)
		return NONE;
	{
		struct mcc_collision_leaf const *leaf = (struct mcc_collision_leaf const *)bsp->leaves.address + (node & 0x7FFFFFFF);
		struct mcc_collision_bsp2d_reference const *reference =
			(struct mcc_collision_bsp2d_reference const *)bsp->bsp2d_references.address + leaf->first_bsp2d_reference_index;
		real_plane3d const *plane = (real_plane3d const *)bsp->bsp3d.planes.address + reference->plane_designator;

		CHECK(leaf->bsp2d_reference_count == 1, "a leaf has its face's reference");
		CHECK(mcc_collision_distance(plane, &p) >= 0.0f, "the leaf's face has the point in front");
		return node & 0x7FFFFFFF;
	}
}

int main(int argc, char **argv)
{
	char const *mode = argv[1];
	long index;

	memset(&hull, 0, sizeof(hull));
	if (!strcmp(mode, "box"))
	{
		/* (a box of 8 corners and points inside it, and on its faces) */
		for (index = 0; index < 8; index++)
			point(index & 1 ? 1.0f : -1.0f, index & 2 ? 0.5f : -0.5f, index & 4 ? 0.25f : 0.0f);
		point(0.0f, 0.0f, 0.1f);
		point(1.0f, 0.0f, 0.1f);
		point(0.5f, 0.5f, 0.25f);
		CHECK(mcc_collision_hull_make(&hull), "made");
		CHECK(hull.plane_count == 6 && hull.vertex_count == 8 && hull.edge_count == 12, "a box: %ld faces, %ld corners, %ld edges",
			hull.plane_count, hull.vertex_count, hull.edge_count);
		hull_check();
		return 0;
	}
	if (!strcmp(mode, "nearly"))
	{
		/* (a box whose top is a little uneven: its four corners one face) */
		for (index = 0; index < 8; index++)
			point(index & 1 ? 1.0f : -1.0f, index & 2 ? 0.5f : -0.5f, index & 4 ? 0.25f + (index & 1) * 0.0004f : 0.0f);
		CHECK(mcc_collision_hull_make(&hull), "made");
		CHECK(hull.plane_count == 6, "six faces (%ld)", hull.plane_count);
		hull_check();
		return 0;
	}
	if (!strcmp(mode, "prism"))
	{
		/* (a twelve-sided prism: its ends hold more corners than a surface
		takes, so sides are made one until they do not) */
		for (index = 0; index < 12; index++)
		{
			real angle = 6.2831853f * (real)index / 12.0f;

			point((real)cos(angle), (real)sin(angle), 0.0f);
			point((real)cos(angle), (real)sin(angle), 0.1f);
		}
		CHECK(mcc_collision_hull_make(&hull), "made");
		hull_check();
		return 0;
	}
	if (!strcmp(mode, "bsp"))
	{
		struct collision_bsp bsp;

		for (index = 0; index < 8; index++)
			point(index & 1 ? 1.0f : -1.0f, index & 2 ? 0.5f : -0.5f, index & 4 ? 0.25f : 0.0f);
		CHECK(mcc_collision_hull_make(&hull), "made");
		memset(&bsp, 0, sizeof(bsp));
		CHECK(mcc_collision_bsp_write(&hull, &bsp, 0), "written");
		CHECK(bsp.bsp3d.nodes.count == 6 && bsp.surfaces.count == 6 && bsp.edges.count == 12 && bsp.vertices.count == 8,
			"the BSP's counts are the hull's");
		CHECK(bsp_test(&bsp, 0.0f, 0.0f, 0.1f) == NONE, "inside: solid");
		CHECK(bsp_test(&bsp, 0.9f, 0.4f, 0.2f) == NONE, "inside, near a corner: solid");
		CHECK(bsp_test(&bsp, 0.0f, 0.0f, 0.3f) != NONE, "above: outside");
		CHECK(bsp_test(&bsp, 0.0f, 0.0f, -0.1f) != NONE, "below: outside");
		CHECK(bsp_test(&bsp, 1.2f, 0.0f, 0.1f) != NONE, "beside: outside");
		for (index = 0; index < 8; index++)
		{
			struct collision_vertex const *vertex = (struct collision_vertex const *)bsp.vertices.address + index;
			struct collision_edge const *edge = (struct collision_edge const *)bsp.edges.address + vertex->first_edge_index;

			CHECK(edge->vertex_indices[0] == index || edge->vertex_indices[1] == index, "corner %ld's first edge is its own", index);
		}
		return 0;
	}
	if (!strcmp(mode, "flat"))
	{
		/* (points on one plane hold no volume: no hull) */
		for (index = 0; index < 6; index++)
			point((real)index, (real)(index * index % 5), 0.0f);
		CHECK(!mcc_collision_hull_make(&hull), "none made");
		return 0;
	}
	if (!strcmp(mode, "file"))
	{
		/* (points from a file, "x y z" a line: for looking at a model's) */
		FILE *file = fopen(argv[2], "r");
		float x, y, z;

		while (file && fscanf(file, "%f %f %f", &x, &y, &z) == 3)
			point(x, y, z);
		printf("%ld points: made %d, %ld faces, %ld corners, %ld edges\n", hull.point_count,
			mcc_collision_hull_make(&hull), hull.plane_count, hull.vertex_count, hull.edge_count);
		for (index = 0; index < hull.plane_count; index++)
		{
			printf("face %ld: (%.4f %.4f %.4f %.4f) %d corners\n", index, hull.planes[index].n.n[0], hull.planes[index].n.n[1],
				hull.planes[index].n.n[2], hull.planes[index].d, hull.face_vertex_counts[index]);
		}
		hull_check();
		return 0;
	}
	fprintf(stderr, "no case %s\n", mode);
	return 2;
}
