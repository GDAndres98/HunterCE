"""The MCC import writes a bitmap's frames in the disc's order where the
operations give it (bitmap_frames): each of the disc's frames MCC's frame of
the same picture, or empty (no pixels) where MCC has none, the bitmap's one
sequence made as long. The menus draw a menus' map's frames by their place
in the disc's bitmap. Bitmaps with sprites, and lines naming frames MCC has
not got or none of MCC's, are left in MCC's order."""
import pytest
from tools.harness import build, function, mutated, read, run, structure

IMPORTER = "port/linux/game/mcc_import.c"


def code():
    return function(read(IMPORTER), "mcc_import_bitmap_frames")


def execute(source, case):
    groups = read("source/tag_files/tag_groups.h")
    bitmaps = read("source/bitmaps/bitmap_group.h")
    types = "\n".join([structure(groups, "tag_block"), structure(groups, "tag_data")] +
                      [structure(bitmaps, name) for name in ("bitmap_data", "bitmap_group_sprite",
                                                             "bitmap_group_sequence", "bitmap_group")] +
                      [structure(read("port/linux/game/mcc_import.h"), "mcc_import_tag_instance")])
    return run(build("mcc_import_bitmap_frames", (("types.inc", types), ("under_test.inc", source))), case)


@pytest.mark.parametrize("case", ["larger", "smaller", "sprites", "range", "empty", "none"])
def test_bitmap_frames(case):
    status, output = execute(code(), case)
    assert status == 0, output


@pytest.mark.parametrize("fault,case", [("sequence", "larger"), ("empty", "smaller"), ("sprites", "sprites"),
                                        ("range", "range")])
def test_negative_control(fault, case):
    source = code()
    if fault == "sequence":
        source = mutated(source, "sequence->bitmap_count = (short)count;", "")
    elif fault == "empty":
        source = mutated(source, "frames[index].pixels_offset = NONE;", ";")
    elif fault == "sprites":
        source = mutated(source, "sequence->sprites.count ||", "")
    else:
        source = mutated(source, "source >= group->bitmaps.count ||", "")
    status, output = execute(source, case)
    assert status == 1 and "check failed" in output, output
