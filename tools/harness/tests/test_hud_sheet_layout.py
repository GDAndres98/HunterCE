"""The real hud_hires_sprite_sheet_matches (hud_hires_tags.c) and hud_hires_asset_sheet_matches (hud_hires.c): with
display.high_res_hud_by_layout, a HUD sprite sheet of other pixels takes its texture only when its sprites on the
bitmap are those of layout.json's sheet_sprites, whose CRC tools/embed_assets.py embeds."""

import json
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, constant, enum_with, function, mutated, read, run  # noqa: E402
from embed_assets import sheet_crc  # noqa: E402

CASES = ["group-type", "disc-layout", "other-bitmaps", "moved-sprite", "extra-sprite", "outside", "no-sheet",
         "too-many"]

# a fault in hud_hires_sprite_sheet_matches and the case that must catch it
NEGATIVE_CONTROLS = {
    "no-dedupe": (("if (at < count && field == 4)", "if (0)"), "disc-layout"),
    "unsorted": (("rects[at][field] > rect[field]", "rects[at][field] < rect[field]"), "disc-layout"),
    "all-bitmaps": (("if (sprite->bitmap_index != bitmap_index)", "if (0)"), "other-bitmaps"),
    "no-type-check": (("if (group->type != HIRES_SPRITES_GROUP_TYPE)", "if (0)"), "no-sheet"),
}


def sheets():
    """Each layout.json entry with sheet_sprites, as C data, with the CRC embed_assets.py gives it."""
    assets = [asset for asset in json.loads(read("port/assets/hud/layout.json"))["assets"]
              if asset.get("sheet_sprites")]
    most = max(len(asset["sheet_sprites"]) for asset in assets)
    lines = [f"static const struct {{ int width, height; unsigned int crc; long count; long rects[{most}][4]; }} "
             "sheets[] = {"]
    for asset in assets:
        rects = ", ".join("{ %d, %d, %d, %d }" % tuple(rect) for rect in asset["sheet_sprites"])
        lines.append(f"\t{{ {asset['width']}, {asset['height']}, 0x{sheet_crc(asset):08x}u, "
                     f"{len(asset['sheet_sprites'])}, {{ {rects} }} }},")
    lines.append("};")
    lines.append(f"#define NUMBEROF_SHEETS {len(assets)}")
    return "\n".join(lines) + "\n"


def generated(fault=None):
    tags = read("port/linux/game/hud_hires_tags.c")
    config = (f"enum {{ HIRES_SPRITES_GROUP_TYPE = {constant(tags, 'HIRES_SPRITES_GROUP_TYPE')}, "
              f"MAXIMUM_HIRES_SHEET_SPRITES = {constant(tags, 'MAXIMUM_HIRES_SHEET_SPRITES')} }};\n"
              + enum_with(read("source/bitmaps/bitmap_extract.c"), "_bitmap_group_type_sprites") + "\n")
    code = function(tags, "hud_hires_sprite_sheet_matches")
    if fault:
        code = mutated(code, *fault)
    code = function(read("port/linux/src/hud_hires.c"), "hud_hires_asset_sheet_matches") + "\n\n" + code
    return (("config.inc", config), ("sheets.inc", sheets()), ("under_test.inc", code + "\n"))


@pytest.mark.parametrize("case", CASES)
def test_case(case):
    status, output = run(build("hud_sheet_layout", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", NEGATIVE_CONTROLS)
def test_negative_control(control):
    fault, case = NEGATIVE_CONTROLS[control]
    status, output = run(build("hud_sheet_layout", generated(fault)), case)
    assert status == CHECK_FAILED, f"the layout check with a fault ({control}) passed '{case}': the test cannot see it"
