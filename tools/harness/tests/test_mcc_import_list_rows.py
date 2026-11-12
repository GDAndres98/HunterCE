"""The MCC import leaves out the rows of the disc's menu lists that the disc's
map has not (Halo PC's SAVE CHECKPOINT and CHANGE SETTINGS of the campaign
pause menu, and the settings screens behind them), moving the rows after them
up, and leaves other widgets' children alone."""
import pytest
from tools.harness import build, constant, function, mutated, read, run, structure

IMPORTER = "port/linux/game/mcc_import.c"
CONSTANTS = ("V5_WIDGET_TYPE_COLUMN_LIST", "V5_WIDGET_CHILD_WIDGETS_OFFSET", "V5_WIDGET_CHILD_BYTES",
             "V5_WIDGET_CHILD_PLACE_OFFSET")


def code():
    return function(read(IMPORTER), "mcc_import_lists_disc_rows")


def execute(source, case):
    groups = read("source/tag_files/tag_groups.h")
    types = "\n".join([structure(groups, "tag_block"), structure(groups, "tag_reference"),
                       structure(read("port/linux/game/mcc_import.h"), "mcc_import_tag_instance")])
    constants = "\n".join(f"#define {name} {constant(read(IMPORTER), name)}" for name in CONSTANTS)
    return run(build("mcc_import_list_rows", (("types.inc", types), ("constants.inc", constants),
                                              ("under_test.inc", source))), case)


@pytest.mark.parametrize("case", ["list", "container"])
def test_list_rows(case):
    status, output = execute(code(), case)
    assert status == 0, output


@pytest.mark.parametrize("fault,case", [("type", "container"), ("group", "container"), ("place", "list"),
                                        ("disc", "list")])
def test_negative_control(fault, case):
    source = code()
    if fault == "type":
        source = mutated(source, "*(short const *)widget != V5_WIDGET_TYPE_COLUMN_LIST", "FALSE")
    elif fault == "group":
        source = mutated(source, "mcc_import_globals.out_groups[out_index] != 'DeLa' ||", "")
    elif fault == "place":
        source = mutated(source, "csmemcpy(place + V5_WIDGET_CHILD_PLACE_OFFSET, offsets, sizeof(offsets));", "")
    else:
        source = mutated(source, "mcc_import_globals.old_to_new[handle & 0xFFFF] == NONE", "FALSE")
    status, output = execute(source, case)
    assert status == 1 and "check failed" in output, output
