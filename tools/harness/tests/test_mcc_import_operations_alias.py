"""The MCC import's alias lines: the disc's tag of a group is MCC's tag of
another name of the same group (the campaign pause screen's box pieces), and
nothing else is. And the bytes the operations gave a data field (their arena)
are told from a map's, which the import never follows into a tag."""
import pytest
from tools.harness import build, function, mutated, read, run

PATCH = "port/linux/game/mcc_import_patch.c"
FUNCTIONS = ("mcc_patch_length", "mcc_patch_group", "mcc_import_operations_alias", "mcc_import_operations_made")


def code():
    source = read(PATCH)
    return "\n".join(function(source, name) for name in FUNCTIONS)


def execute(source, case):
    return run(build("mcc_import_operations_alias", (("under_test.inc", source),)), case)


@pytest.mark.parametrize("case", ["alias", "none", "made"])
def test_operations_alias(case):
    status, output = execute(code(), case)
    assert status == 0, output


@pytest.mark.parametrize("fault,case", [("group", "none"), ("target", "none"), ("used", "made")])
def test_negative_control(fault, case):
    source = code()
    if fault == "group":
        source = mutated(source, "mcc_patch_group(operation->tag) == group && ", "")
    elif fault == "target":
        source = mutated(source, " && mcc_patch_group(operation->a) == group)", ")")
    else:
        source = mutated(source, "size <= (unsigned long)(mcc_patch_globals.arena + mcc_patch_globals.arena_used - bytes)",
                         "TRUE")
    status, output = execute(source, case)
    assert status == 1 and "check failed" in output, output
