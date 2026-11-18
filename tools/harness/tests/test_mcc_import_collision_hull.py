"""The MCC import's collision models built from a render model's shape (the
multiplayer maps' flag and teleporter bases, which MCC has none of): the
convex hull of the points, its nearly equal planes one, no face of more
corners than a collision surface takes, its winged edges closed and going
round each face as the game walks them, and its BSP putting a point inside
in the solid and one outside in the leaf of a face it is in front of."""
import re

import pytest
from tools.harness import build, constant, enum_with, function, mutated, read, run, structure

COLLISION = "port/linux/game/mcc_import_collision.c"
FUNCTIONS = ("mcc_collision_distance", "mcc_collision_plane_hold", "mcc_collision_point_add", "mcc_collision_planes",
             "mcc_collision_meet", "mcc_collision_angle", "mcc_collision_faces", "mcc_collision_side",
             "mcc_collision_merge", "mcc_collision_edges", "mcc_collision_hull_make", "mcc_collision_block",
             "mcc_collision_bsp_write")


def union(source, name):
    """a named union's definition and its typedef"""
    match = re.search(r"^union\s+" + re.escape(name) + r"\s*\{.*?^\};\s*\ntypedef union " + re.escape(name) + r" \w+;",
                      source, re.M | re.S)
    if not match:
        raise LookupError(f"union not found in the sources: {name}")
    return match.group(0)


def code():
    source = read(COLLISION)
    return "\n".join(function(source, name) for name in FUNCTIONS)


def binary(source):
    """the test program, built"""
    maths = read("source/math/real_math.h")
    groups = read("source/tag_files/tag_groups.h")
    collision = read("source/physics/collision_bsp_definitions.h")
    own = read(COLLISION)
    plane = re.search(r"^struct real_plane3d\s*\{.*?^\};\s*\ntypedef struct real_plane3d real_plane3d;", maths,
                      re.M | re.S).group(0)
    types = "\n".join([
        union(maths, "real_point3d"), union(maths, "real_vector3d"), plane,
        structure(groups, "tag_block"),
        structure(read("source/physics/bsp3d.h"), "bsp3d"), structure(read("source/physics/bsp3d.h"), "bsp3d_node"),
        structure(read("source/physics/bsp2d.h"), "bsp2d"),
        structure(collision, "collision_surface"), structure(collision, "collision_edge"),
        structure(collision, "collision_vertex"), structure(collision, "collision_bsp"),
    ])
    constants = "\n".join(
        [f"#define MAXIMUM_VERTICES_PER_COLLISION_SURFACE {constant(collision, 'MAXIMUM_VERTICES_PER_COLLISION_SURFACE')}",
         enum_with(own, "MCC_COLLISION_MAXIMUM_POINTS")] +
        re.findall(r"^#define MCC_COLLISION_\w+ [^\n]+", own, re.M) +
        [structure(own, "mcc_collision_leaf"), structure(own, "mcc_collision_bsp2d_reference"),
         structure(own, "mcc_collision_hull")])
    return build("mcc_import_collision_hull", (("types.inc", types), ("constants.inc", constants),
                                                ("under_test.inc", source)))


def execute(source, case):
    return run(binary(source), case)


@pytest.mark.parametrize("case", ["box", "nearly", "prism", "bsp", "flat"])
def test_collision_hull(case):
    status, output = execute(code(), case)
    assert status == 0, output


@pytest.mark.parametrize("fault,case", [("merge", "nearly"), ("limit", "prism"), ("ring", "box"), ("leaf", "bsp")])
def test_negative_control(fault, case):
    source = code()
    if fault == "merge":
        source = mutated(source, "fabs(plane.d - other->d) <= MCC_COLLISION_SAME_DISTANCE", "FALSE")
    elif fault == "limit":
        source = mutated(source, "hull->face_vertex_counts[face] > MCC_COLLISION_SURFACE_VERTICES", "FALSE")
    elif fault == "ring":
        source = mutated(source, "edge->edge_indices[edge->surface_indices[1] == face] = ring[(i + 1) % count];",
                         "edge->edge_indices[edge->surface_indices[1] == face] = ring[i];")
    else:
        source = mutated(source, "nodes[index].children[1] = MCC_COLLISION_LEAF_BIT | face;",
                         "nodes[index].children[1] = MCC_COLLISION_LEAF_BIT | index;")
    status, output = execute(source, case)
    assert status == 1 and "check failed" in output, output
