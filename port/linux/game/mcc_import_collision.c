/*
MCC_IMPORT_COLLISION.C

Collision models the import builds from a render model (M3b). Halo PC, and
so MCC, has no collision for the multiplayer maps' flag bases and
teleporter bases: their coll tags are empty, and a player walks through
them. The disc's are small convex shapes. The operations' collision_hull
lines name such a tag, the render model to take its shape from, and its
material ("<type>,<name>"); the import gives the tag one node, one region
and one convex BSP: the convex hull of the model's opaque parts (those of
environment and model shaders, not a teleporter's transparent cone), its
nearly equal planes made one, and no face of more corners than a collision
surface takes. The shape is MCC's, not the disc's: close to it, never the
same bytes.
*/

/* ---------- headers */

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "errors.h"
#include "tag_files/tag_groups.h"
#include "math/real_math.h"
#include "models/model_definitions.h"
#include "rasterizer/rasterizer_geometry.h"
#include "physics/bsp3d.h"
#include "physics/collision_bsp_definitions.h"
#include "physics/collision_model_definitions.h"
#include "mcc_import.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- constants */

enum
{
	MCC_COLLISION_MAXIMUM_POINTS = 256,
	MCC_COLLISION_MAXIMUM_PLANES = 128,
	MCC_COLLISION_MAXIMUM_VERTICES = 256,
	MCC_COLLISION_MAXIMUM_EDGES = 512,
	/* a face's corners while the hull is made, and a collision surface's */
	MCC_COLLISION_MAXIMUM_FACE_VERTICES = 64,
	MCC_COLLISION_SURFACE_VERTICES = MAXIMUM_VERTICES_PER_COLLISION_SURFACE,
	/* the merges that bring the faces to that many corners */
	MCC_COLLISION_MAXIMUM_MERGES = 64,
	/* (a leaf's or a 2d bsp's index: the high bit) */
	MCC_COLLISION_LEAF_BIT = (long)0x80000000,
};

/* how far a point may be from a plane and be on it, and how far planes may
be apart (in normal and in distance) and be one: world units (3.048 m). A
corner, where planes meet, is on a face's plane far more nearly, and is
one corner with another nearer still (CORNER). */
#define MCC_COLLISION_ON_PLANE 0.0005f
#define MCC_COLLISION_CORNER 0.00005f
#define MCC_COLLISION_SAME_NORMAL 0.9998f
#define MCC_COLLISION_SAME_DISTANCE 0.003f

/* ---------- structures */

/* the parts of a render model's geometry and its shaders, as loaded */
struct mcc_collision_geometry
{
	unsigned long flags;
	long unused[8];
	struct tag_block parts;
};

struct mcc_collision_part
{
	unsigned long flags;
	short shader_index;
	byte unknown006[0x54 - 0x06];
	struct vertex_buffer vertex_buffer;
};

struct mcc_collision_shader
{
	struct tag_reference shader;
	byte unknown010[0x20 - 0x10];
};

/* collision_bsp.c's and damage.c's own */
struct mcc_collision_leaf
{
	word flags;
	short bsp2d_reference_count;
	long first_bsp2d_reference_index;
};

struct mcc_collision_bsp2d_reference
{
	long plane_designator;
	long root_index;
};

struct mcc_collision_region
{
	char name[TAG_STRING_LENGTH + 1];
	unsigned long flags;
	long unused0;
	real damage_threshold;
	long unused1[3];
	struct tag_reference destroyed_effect;
	struct tag_block permutations;
};

struct mcc_collision_permutation
{
	char name[TAG_STRING_LENGTH + 1];
};

typedef char mcc_collision_geometry_size[sizeof(struct mcc_collision_geometry) == 0x30 ? 1 : -1];
typedef char mcc_collision_part_size[sizeof(struct mcc_collision_part) == 0x68 ? 1 : -1];
typedef char mcc_collision_shader_size[sizeof(struct mcc_collision_shader) == 0x20 ? 1 : -1];
typedef char mcc_collision_leaf_size[sizeof(struct mcc_collision_leaf) == 0x08 ? 1 : -1];
typedef char mcc_collision_region_size[sizeof(struct mcc_collision_region) == 0x54 ? 1 : -1];
typedef char mcc_collision_node_size[sizeof(struct collision_node) == 0x40 ? 1 : -1];
typedef char mcc_collision_bsp_size[sizeof(struct collision_bsp) == 0x60 ? 1 : -1];
typedef char mcc_collision_model_size[sizeof(struct collision_model) == 0x298 ? 1 : -1];

/* the hull: the points it holds, its planes (outward), its corners, each
face's corners (counter-clockwise seen from outside) and the BSP's edges */
struct mcc_collision_hull
{
	long point_count;
	real_point3d points[MCC_COLLISION_MAXIMUM_POINTS];
	long plane_count;
	real_plane3d planes[MCC_COLLISION_MAXIMUM_PLANES];
	long vertex_count;
	real_point3d vertices[MCC_COLLISION_MAXIMUM_VERTICES];
	short face_vertex_counts[MCC_COLLISION_MAXIMUM_PLANES];
	short face_vertices[MCC_COLLISION_MAXIMUM_PLANES][MCC_COLLISION_MAXIMUM_FACE_VERTICES];
	long edge_count;
	struct collision_edge edges[MCC_COLLISION_MAXIMUM_EDGES];
	long face_first_edges[MCC_COLLISION_MAXIMUM_PLANES];
	long vertex_first_edges[MCC_COLLISION_MAXIMUM_VERTICES];
};

/* ---------- globals */

static struct mcc_collision_hull mcc_collision_work;

/* ---------- private code: the hull */

static real mcc_collision_distance(
	real_plane3d const *plane,
	real_point3d const *point)
{
	return plane->n.n[0] * point->n[0] + plane->n.n[1] * point->n[1] + plane->n.n[2] * point->n[2] - plane->d;
}

/* the plane's distance made the farthest point's, so that every point is
behind it or on it */
static void mcc_collision_plane_hold(
	struct mcc_collision_hull const *hull,
	real_plane3d *plane)
{
	long index;
	real most = plane->n.n[0] * hull->points[0].n[0] + plane->n.n[1] * hull->points[0].n[1] +
		plane->n.n[2] * hull->points[0].n[2];

	for (index = 1; index < hull->point_count; index++)
	{
		real along = plane->n.n[0] * hull->points[index].n[0] + plane->n.n[1] * hull->points[index].n[1] +
			plane->n.n[2] * hull->points[index].n[2];

		if (along > most)
			most = along;
	}
	plane->d = most;
}

/* a point added, unless it is one already */
static boolean mcc_collision_point_add(
	struct mcc_collision_hull *hull,
	real_point3d const *point)
{
	long index;

	for (index = 0; index < hull->point_count; index++)
	{
		if (hull->points[index].n[0] == point->n[0] && hull->points[index].n[1] == point->n[1] &&
			hull->points[index].n[2] == point->n[2])
		{
			return TRUE;
		}
	}
	if (hull->point_count >= MCC_COLLISION_MAXIMUM_POINTS)
		return FALSE;
	hull->points[hull->point_count++] = *point;

	return TRUE;
}

/* every plane through three points with all the points behind it (or on
it), outward, the nearly equal ones once */
static boolean mcc_collision_planes(
	struct mcc_collision_hull *hull)
{
	long a, b, c, index;

	hull->plane_count = 0;
	for (a = 0; a < hull->point_count; a++)
	{
		for (b = a + 1; b < hull->point_count; b++)
		{
			for (c = b + 1; c < hull->point_count; c++)
			{
				real_point3d const *pa = &hull->points[a], *pb = &hull->points[b], *pc = &hull->points[c];
				real u[3], v[3], length;
				real_plane3d plane;
				boolean in_front = FALSE, behind = FALSE, known = FALSE;

				for (index = 0; index < 3; index++)
				{
					u[index] = pb->n[index] - pa->n[index];
					v[index] = pc->n[index] - pa->n[index];
				}
				plane.n.n[0] = u[1] * v[2] - u[2] * v[1];
				plane.n.n[1] = u[2] * v[0] - u[0] * v[2];
				plane.n.n[2] = u[0] * v[1] - u[1] * v[0];
				length = plane.n.n[0] * plane.n.n[0] + plane.n.n[1] * plane.n.n[1] + plane.n.n[2] * plane.n.n[2];
				if (length < 1e-12f)
					continue;
				length = square_root(length);
				for (index = 0; index < 3; index++)
					plane.n.n[index] /= length;
				plane.d = plane.n.n[0] * pa->n[0] + plane.n.n[1] * pa->n[1] + plane.n.n[2] * pa->n[2];
				for (index = 0; index < hull->point_count && !(in_front && behind); index++)
				{
					real distance = mcc_collision_distance(&plane, &hull->points[index]);

					in_front = in_front || distance > MCC_COLLISION_ON_PLANE;
					behind = behind || distance < -MCC_COLLISION_ON_PLANE;
				}
				if (in_front && behind)
					continue;
				if (in_front)
				{
					for (index = 0; index < 3; index++)
						plane.n.n[index] = -plane.n.n[index];
				}
				mcc_collision_plane_hold(hull, &plane);
				for (index = 0; index < hull->plane_count && !known; index++)
				{
					real_plane3d const *other = &hull->planes[index];

					known = plane.n.n[0] * other->n.n[0] + plane.n.n[1] * other->n.n[1] + plane.n.n[2] * other->n.n[2] >=
						MCC_COLLISION_SAME_NORMAL && fabs(plane.d - other->d) <= MCC_COLLISION_SAME_DISTANCE;
				}
				if (known)
					continue;
				if (hull->plane_count >= MCC_COLLISION_MAXIMUM_PLANES)
					return FALSE;
				hull->planes[hull->plane_count++] = plane;
			}
		}
	}

	return hull->plane_count >= 4;
}

/* the point where three planes meet, if they do */
static boolean mcc_collision_meet(
	real_plane3d const *p,
	real_plane3d const *q,
	real_plane3d const *r,
	real_point3d *point)
{
	real qr[3], rp[3], pq[3], determinant;
	short index;

	qr[0] = q->n.n[1] * r->n.n[2] - q->n.n[2] * r->n.n[1];
	qr[1] = q->n.n[2] * r->n.n[0] - q->n.n[0] * r->n.n[2];
	qr[2] = q->n.n[0] * r->n.n[1] - q->n.n[1] * r->n.n[0];
	rp[0] = r->n.n[1] * p->n.n[2] - r->n.n[2] * p->n.n[1];
	rp[1] = r->n.n[2] * p->n.n[0] - r->n.n[0] * p->n.n[2];
	rp[2] = r->n.n[0] * p->n.n[1] - r->n.n[1] * p->n.n[0];
	pq[0] = p->n.n[1] * q->n.n[2] - p->n.n[2] * q->n.n[1];
	pq[1] = p->n.n[2] * q->n.n[0] - p->n.n[0] * q->n.n[2];
	pq[2] = p->n.n[0] * q->n.n[1] - p->n.n[1] * q->n.n[0];
	determinant = p->n.n[0] * qr[0] + p->n.n[1] * qr[1] + p->n.n[2] * qr[2];
	if (fabs(determinant) < 1e-6f)
		return FALSE;
	for (index = 0; index < 3; index++)
		point->n[index] = (p->d * qr[index] + q->d * rp[index] + r->d * pq[index]) / determinant;

	return TRUE;
}

/* a pseudo-angle of (x, y): it grows as the angle does, from 0 to 4 */
static real mcc_collision_angle(
	real x,
	real y)
{
	real sum = fabs(x) + fabs(y);

	if (sum <= 0.0f)
		return 0.0f;
	return y >= 0.0f ? 1.0f - x / sum : 3.0f + x / sum;
}

/* the hull's corners (where three planes meet, behind every plane) and each
face's corners in order; a plane that holds fewer than three corners holds
no face, and goes */
static boolean mcc_collision_faces(
	struct mcc_collision_hull *hull)
{
	boolean dropped = TRUE;
	long a, b, c, index, face;

	while (dropped)
	{
		dropped = FALSE;
		hull->vertex_count = 0;
		for (a = 0; a < hull->plane_count; a++)
		{
			for (b = a + 1; b < hull->plane_count; b++)
			{
				for (c = b + 1; c < hull->plane_count; c++)
				{
					real_point3d point;
					boolean inside = TRUE, known = FALSE;

					if (!mcc_collision_meet(&hull->planes[a], &hull->planes[b], &hull->planes[c], &point))
						continue;
					for (index = 0; index < hull->plane_count && inside; index++)
						inside = mcc_collision_distance(&hull->planes[index], &point) <= MCC_COLLISION_CORNER;
					for (index = 0; index < hull->vertex_count && inside && !known; index++)
					{
						known = fabs(hull->vertices[index].n[0] - point.n[0]) <= MCC_COLLISION_CORNER &&
							fabs(hull->vertices[index].n[1] - point.n[1]) <= MCC_COLLISION_CORNER &&
							fabs(hull->vertices[index].n[2] - point.n[2]) <= MCC_COLLISION_CORNER;
					}
					if (!inside || known)
						continue;
					if (hull->vertex_count >= MCC_COLLISION_MAXIMUM_VERTICES)
						return FALSE;
					hull->vertices[hull->vertex_count++] = point;
				}
			}
		}
		for (face = 0; face < hull->plane_count; face++)
		{
			real_plane3d const *plane = &hull->planes[face];
			real center[3] = { 0.0f, 0.0f, 0.0f }, u[3], w[3], length;
			real angles[MCC_COLLISION_MAXIMUM_FACE_VERTICES];
			short count = 0, i, j;

			for (index = 0; index < hull->vertex_count; index++)
			{
				if (fabs(mcc_collision_distance(plane, &hull->vertices[index])) > MCC_COLLISION_CORNER)
					continue;
				if (count >= MCC_COLLISION_MAXIMUM_FACE_VERTICES)
					return FALSE;
				hull->face_vertices[face][count++] = (short)index;
			}
			hull->face_vertex_counts[face] = count;
			if (count < 3)
			{
				hull->planes[face] = hull->planes[--hull->plane_count];
				dropped = TRUE;
				break;
			}
			for (i = 0; i < count; i++)
			{
				for (index = 0; index < 3; index++)
					center[index] += hull->vertices[hull->face_vertices[face][i]].n[index] / count;
			}
			for (index = 0; index < 3; index++)
				u[index] = hull->vertices[hull->face_vertices[face][0]].n[index] - center[index];
			length = square_root(u[0] * u[0] + u[1] * u[1] + u[2] * u[2]);
			if (length <= 0.0f)
				return FALSE;
			for (index = 0; index < 3; index++)
				u[index] /= length;
			w[0] = plane->n.n[1] * u[2] - plane->n.n[2] * u[1];
			w[1] = plane->n.n[2] * u[0] - plane->n.n[0] * u[2];
			w[2] = plane->n.n[0] * u[1] - plane->n.n[1] * u[0];
			for (i = 0; i < count; i++)
			{
				real_point3d const *corner = &hull->vertices[hull->face_vertices[face][i]];
				real x = 0.0f, y = 0.0f;

				for (index = 0; index < 3; index++)
				{
					x += (corner->n[index] - center[index]) * u[index];
					y += (corner->n[index] - center[index]) * w[index];
				}
				angles[i] = mcc_collision_angle(x, y);
			}
			for (i = 1; i < count; i++)
			{
				for (j = i; j > 0 && angles[j - 1] > angles[j]; j--)
				{
					real angle = angles[j];
					short corner = hull->face_vertices[face][j];

					angles[j] = angles[j - 1];
					angles[j - 1] = angle;
					hull->face_vertices[face][j] = hull->face_vertices[face][j - 1];
					hull->face_vertices[face][j - 1] = corner;
				}
			}
		}
	}

	return hull->plane_count >= 4;
}

/* the plane other than `face` that holds both corners, else NONE */
static long mcc_collision_side(
	struct mcc_collision_hull const *hull,
	long face,
	short a,
	short b)
{
	long other;

	for (other = 0; other < hull->plane_count; other++)
	{
		short i, found = 0;

		if (other == face)
			continue;
		for (i = 0; i < hull->face_vertex_counts[other]; i++)
			found += hull->face_vertices[other][i] == a || hull->face_vertices[other][i] == b;
		if (found == 2)
			return other;
	}

	return NONE;
}

/* a face of more corners than a surface takes loses one: of the planes
beside it, the two neighbours most nearly alike become one, behind which
every point stays (the shape grows a little there) */
static boolean mcc_collision_merge(
	struct mcc_collision_hull *hull,
	long face)
{
	short count = hull->face_vertex_counts[face], i;
	long best_first = NONE, best_second = NONE;
	real best = -2.0f;

	for (i = 0; i < count; i++)
	{
		short a = hull->face_vertices[face][i], b = hull->face_vertices[face][(i + 1) % count],
			c = hull->face_vertices[face][(i + 2) % count];
		long first = mcc_collision_side(hull, face, a, b), second = mcc_collision_side(hull, face, b, c);

		if (first != NONE && second != NONE && first != second)
		{
			real_plane3d const *p = &hull->planes[first], *q = &hull->planes[second];
			real alike = p->n.n[0] * q->n.n[0] + p->n.n[1] * q->n.n[1] + p->n.n[2] * q->n.n[2];

			if (alike > best)
			{
				best = alike;
				best_first = first;
				best_second = second;
			}
		}
	}
	if (best_first == NONE)
		return FALSE;
	{
		real_plane3d *p = &hull->planes[best_first];
		real_plane3d const *q = &hull->planes[best_second];
		real length;
		short index;

		for (index = 0; index < 3; index++)
			p->n.n[index] += q->n.n[index];
		length = square_root(p->n.n[0] * p->n.n[0] + p->n.n[1] * p->n.n[1] + p->n.n[2] * p->n.n[2]);
		if (length <= 0.0f)
			return FALSE;
		for (index = 0; index < 3; index++)
			p->n.n[index] /= length;
		mcc_collision_plane_hold(hull, p);
		hull->planes[best_second] = hull->planes[--hull->plane_count];
	}

	return TRUE;
}

/* the winged edges: each face's corners in order make its ring of edges,
an edge shared by its two faces (the first going from its first corner to
its second, the other back); FALSE if the hull is not closed */
static boolean mcc_collision_edges(
	struct mcc_collision_hull *hull)
{
	long face, index;

	hull->edge_count = 0;
	for (index = 0; index < hull->vertex_count; index++)
		hull->vertex_first_edges[index] = NONE;
	for (face = 0; face < hull->plane_count; face++)
	{
		short count = hull->face_vertex_counts[face], i;
		long ring[MCC_COLLISION_MAXIMUM_FACE_VERTICES];

		for (i = 0; i < count; i++)
		{
			long a = hull->face_vertices[face][i], b = hull->face_vertices[face][(i + 1) % count], edge;

			for (edge = 0; edge < hull->edge_count; edge++)
			{
				if (hull->edges[edge].vertex_indices[0] == b && hull->edges[edge].vertex_indices[1] == a &&
					hull->edges[edge].surface_indices[1] == NONE)
				{
					break;
				}
			}
			if (edge == hull->edge_count)
			{
				if (hull->edge_count >= MCC_COLLISION_MAXIMUM_EDGES)
					return FALSE;
				hull->edges[edge].vertex_indices[0] = a;
				hull->edges[edge].vertex_indices[1] = b;
				hull->edges[edge].surface_indices[0] = face;
				hull->edges[edge].surface_indices[1] = NONE;
				hull->edges[edge].edge_indices[0] = NONE;
				hull->edges[edge].edge_indices[1] = NONE;
				hull->edge_count++;
			}
			else
				hull->edges[edge].surface_indices[1] = face;
			ring[i] = edge;
			if (hull->vertex_first_edges[a] == NONE)
				hull->vertex_first_edges[a] = edge;
		}
		for (i = 0; i < count; i++)
		{
			struct collision_edge *edge = &hull->edges[ring[i]];

			edge->edge_indices[edge->surface_indices[1] == face] = ring[(i + 1) % count];
		}
		hull->face_first_edges[face] = ring[0];
	}
	for (index = 0; index < hull->edge_count; index++)
	{
		if (hull->edges[index].surface_indices[1] == NONE || hull->edges[index].edge_indices[0] == NONE ||
			hull->edges[index].edge_indices[1] == NONE)
		{
			return FALSE;
		}
	}
	for (index = 0; index < hull->vertex_count; index++)
	{
		if (hull->vertex_first_edges[index] == NONE)
			return FALSE;
	}

	return hull->vertex_count - hull->edge_count + hull->plane_count == 2;
}

/* the hull of the points: its planes, corners, faces and edges, no face of
more corners than a collision surface takes */
static boolean mcc_collision_hull_make(
	struct mcc_collision_hull *hull)
{
	long merges = 0, face;

	if (hull->point_count < 4 || !mcc_collision_planes(hull))
		return FALSE;
	for (;;)
	{
		long largest = NONE;

		if (!mcc_collision_faces(hull))
			return FALSE;
		for (face = 0; face < hull->plane_count; face++)
		{
			if (hull->face_vertex_counts[face] > MCC_COLLISION_SURFACE_VERTICES)
				largest = face;
		}
		if (largest == NONE)
			break;
		if (merges++ >= MCC_COLLISION_MAXIMUM_MERGES || !mcc_collision_merge(hull, largest))
			return FALSE;
	}

	return mcc_collision_edges(hull);
}

/* ---------- private code: the collision model */

static void *mcc_collision_block(
	struct tag_block *block,
	long count,
	unsigned long size)
{
	void *elements = mcc_import_operations_allocate((unsigned long)count * size);

	block->count = elements ? count : 0;
	block->address = elements;
	block->definition = NULL;

	return elements;
}

/* the hull's convex BSP: a node for each face's plane in turn, in front of
which is that face's leaf and behind which the next node (behind the last,
the solid); a leaf's one 2d reference is its face's surface itself */
static boolean mcc_collision_bsp_write(
	struct mcc_collision_hull const *hull,
	struct collision_bsp *bsp,
	short material_index)
{
	long count = hull->plane_count, index;
	struct bsp3d_node *nodes = mcc_collision_block(&bsp->bsp3d.nodes, count, sizeof(struct bsp3d_node));
	real_plane3d *planes = mcc_collision_block(&bsp->bsp3d.planes, count, sizeof(real_plane3d));
	struct mcc_collision_leaf *leaves = mcc_collision_block(&bsp->leaves, count, sizeof(struct mcc_collision_leaf));
	struct mcc_collision_bsp2d_reference *references = mcc_collision_block(&bsp->bsp2d_references, count,
		sizeof(struct mcc_collision_bsp2d_reference));
	struct collision_surface *surfaces = mcc_collision_block(&bsp->surfaces, count, sizeof(struct collision_surface));
	struct collision_edge *edges = mcc_collision_block(&bsp->edges, hull->edge_count, sizeof(struct collision_edge));
	struct collision_vertex *vertices = mcc_collision_block(&bsp->vertices, hull->vertex_count,
		sizeof(struct collision_vertex));

	bsp->bsp2d.nodes.count = 0;
	bsp->bsp2d.nodes.address = NULL;
	bsp->bsp2d.nodes.definition = NULL;
	if (!nodes || !planes || !leaves || !references || !surfaces || !edges || !vertices)
		return FALSE;
	for (index = 0; index < count; index++)
	{
		long face = count - 1 - index;

		planes[index] = hull->planes[index];
		nodes[index].plane_designator = index;
		nodes[index].children[0] = index + 1 < count ? index + 1 : NONE;
		nodes[index].children[1] = MCC_COLLISION_LEAF_BIT | face;
		leaves[index].flags = 0;
		leaves[index].bsp2d_reference_count = 1;
		leaves[index].first_bsp2d_reference_index = index;
		references[index].plane_designator = face;
		references[index].root_index = MCC_COLLISION_LEAF_BIT | face;
		surfaces[index].plane_designator = index;
		surfaces[index].first_edge_index = hull->face_first_edges[index];
		surfaces[index].flags = 0;
		surfaces[index].breakable_surface_index = 0;
		surfaces[index].material_index = material_index;
	}
	for (index = 0; index < hull->edge_count; index++)
		edges[index] = hull->edges[index];
	for (index = 0; index < hull->vertex_count; index++)
	{
		vertices[index].point = hull->vertices[index];
		vertices[index].first_edge_index = hull->vertex_first_edges[index];
	}

	return TRUE;
}

/* a reference to no effect */
static void mcc_collision_no_effect(
	struct tag_reference *reference)
{
	reference->group_tag = 'effe';
	reference->name = NULL;
	reference->name_length = 0;
	reference->index = NONE;
}

/* a new collision model's references name no effect (its zeros would name
the table's first tag) */
static void mcc_collision_no_effects(
	struct damage_resistance *resistance)
{
	mcc_collision_no_effect(&resistance->localized_damage_effect);
	mcc_collision_no_effect(&resistance->area_damage_effect);
	mcc_collision_no_effect(&resistance->body_damaged_effect);
	mcc_collision_no_effect(&resistance->body_depleted_effect);
	mcc_collision_no_effect(&resistance->body_destroyed_effect);
	mcc_collision_no_effect(&resistance->shield_damaged_effect);
	mcc_collision_no_effect(&resistance->shield_depleted_effect);
	mcc_collision_no_effect(&resistance->shield_recharging_effect);
}

/* a tag of the table by group and name, else NONE */
static long mcc_collision_tag(
	struct mcc_import_tags const *tags,
	char const *text)
{
	unsigned long group = 0;
	short character;
	long index;

	if (strlen(text) < 6 || text[4] != ':')
		return NONE;
	for (character = 0; character < 4; character++)
		group = group << 8 | (unsigned char)(text[character] == '_' ? ' ' : text[character]);
	for (index = 0; index < tags->count; index++)
	{
		if ((unsigned long)tags->instances[index].group_tag == group && !strcmp(tags->instances[index].name, text + 5))
			return index;
	}

	return NONE;
}

/* the model's opaque parts' points (environment and model shaders) */
static boolean mcc_collision_model_points(
	struct model const *model,
	struct mcc_collision_hull *hull)
{
	long stride = rasterizer_geometry_get_vertex_size(_rasterizer_vertex_type_model_compressed);
	long geometry_index;

	hull->point_count = 0;
	for (geometry_index = 0; geometry_index < model->geometries.count && model->geometries.address; geometry_index++)
	{
		struct mcc_collision_geometry const *geometry =
			(struct mcc_collision_geometry const *)model->geometries.address + geometry_index;
		long part_index;

		for (part_index = 0; part_index < geometry->parts.count && geometry->parts.address; part_index++)
		{
			struct mcc_collision_part const *part = (struct mcc_collision_part const *)geometry->parts.address + part_index;
			struct mcc_collision_shader const *shader;
			long vertex;

			if (part->shader_index < 0 || part->shader_index >= model->shaders.count || !model->shaders.address)
				continue;
			shader = (struct mcc_collision_shader const *)model->shaders.address + part->shader_index;
			if (shader->shader.group_tag != 'senv' && shader->shader.group_tag != 'soso')
				continue;
			for (vertex = 0; vertex < part->vertex_buffer.count && part->vertex_buffer.base_address; vertex++)
			{
				real_point3d point;

				csmemcpy(&point, (byte const *)part->vertex_buffer.base_address + vertex * stride, sizeof(point));
				if (!mcc_collision_point_add(hull, &point))
					return FALSE;
			}
		}
	}

	return TRUE;
}

/* the collision model `coll` given the hull of the model `mode`'s shape:
one material ("<type>,<name>"), one region and permutation, one node (the
model's first node's) with the hull's BSP, and a pathfinding sphere about
it */
static boolean mcc_collision_build(
	struct mcc_import_tags *tags,
	char const *coll_name,
	char const *model_name,
	char const *material_text)
{
	struct mcc_collision_hull *hull = &mcc_collision_work;
	long model_index = mcc_collision_tag(tags, model_name), coll_index;
	struct collision_model *collision;
	struct model const *model;
	struct damage_resistance_material *material;
	struct mcc_collision_region *region;
	struct mcc_collision_permutation *permutation;
	struct collision_node *node;
	struct collision_bsp *bsp;
	struct pathfinding_sphere *sphere;
	char const *comma = material_text ? strchr(material_text, ',') : NULL;
	real radius = 0.0f;
	long index;

	if (model_index == NONE || !comma || !tags->instances[model_index].base_address)
		return FALSE;
	model = tags->instances[model_index].base_address;
	if (!mcc_collision_model_points(model, hull) || !mcc_collision_hull_make(hull))
		return FALSE;
	/* (MCC has none of these: the tag is added, of the disc's name; one
	MCC has with nodes is its own, and stays) */
	coll_index = mcc_import_operations_tag_add(tags, coll_name);
	if (coll_index == NONE || !tags->instances[coll_index].base_address)
		return FALSE;
	collision = tags->instances[coll_index].base_address;
	if (collision->nodes.count)
		return TRUE;
	mcc_collision_no_effects(&collision->resistance);
	material = mcc_collision_block(&collision->resistance.materials, 1, sizeof(*material));
	region = mcc_collision_block(&collision->resistance.regions, 1, sizeof(*region));
	permutation = region ? mcc_collision_block(&region->permutations, 1, sizeof(*permutation)) : NULL;
	node = mcc_collision_block(&collision->nodes, 1, sizeof(*node));
	bsp = node ? mcc_collision_block(&node->bsps, 1, sizeof(*bsp)) : NULL;
	sphere = mcc_collision_block(&collision->pathfinding_spheres, 1, sizeof(*sphere));
	if (!material || !permutation || !bsp || !sphere || !mcc_collision_bsp_write(hull, bsp, 0))
		return FALSE;
	collision->resistance.indirect_damage_material_index = NONE;
	material->material_type = (short)strtol(material_text, NULL, 10);
	strncpy(material->name, comma + 1, TAG_STRING_LENGTH);
	strncpy(region->name, "__unnamed", TAG_STRING_LENGTH);
	region->destroyed_effect.group_tag = 'effe';
	region->destroyed_effect.index = NONE;
	strncpy(permutation->name, "collision_volume", TAG_STRING_LENGTH);
	if (model->nodes.count > 0 && model->nodes.address)
		strncpy(node->name, ((struct model_node const *)model->nodes.address)->name, TAG_STRING_LENGTH);
	node->region_index = 0;
	node->parent_node_index = NONE;
	node->next_sibling_node_index = NONE;
	node->first_child_node_index = NONE;
	node->runtime_damage_part = NONE;
	for (index = 0; index < hull->vertex_count; index++)
	{
		real across = square_root(hull->vertices[index].n[0] * hull->vertices[index].n[0] +
			hull->vertices[index].n[1] * hull->vertices[index].n[1]);

		if (across > radius)
			radius = across;
	}
	sphere->node_index = 0;
	sphere->radius = radius;
	fprintf(stdout, "mcc import: %s: the hull of %s: %ld points, %ld faces, %ld corners, %ld edges\n", coll_name,
		model_name, hull->point_count, hull->plane_count, hull->vertex_count, hull->edge_count);

	return TRUE;
}

/* ---------- public code */

long mcc_import_collision_hulls(
	struct mcc_import_tags *tags)
{
	char const *tag, *a, *b;
	long cursor = 0, failed = 0;

	while (mcc_import_operations_next("collision_hull", &cursor, &tag, &a, &b))
	{
		if (!mcc_collision_build(tags, tag, a, b))
		{
			error(_error_silent, "mcc import: %s: no collision built from %s", tag, a ? a : "-");
			fprintf(stderr, "mcc import: %s: no collision built from %s\n", tag, a ? a : "-");
			failed++;
		}
	}

	return failed;
}
