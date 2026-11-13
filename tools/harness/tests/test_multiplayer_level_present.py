"""A new multiplayer game starts on a map this machine has: when its own (carousel,
or the map used last) is missing from the maps folder, on the first it has, of
the Xbox's, the Custom Edition maps, then MCC's; none when it has none. A map the
player chooses that is missing is refused, not loaded (which would be taken for a
damaged disc and restart the game)."""
import pytest
from tools.harness import build, function, mutated, read, run

SOURCE = "port/linux/game/custom_edition_maps.c"


def code():
    source = read(SOURCE)
    return "\n".join(function(source, name) for name in (
        "multiplayer_level_present", "multiplayer_level_chosen_present", "multiplayer_level_first_present"))


def execute(source, case):
    return run(build("multiplayer_level_present", (("under_test.inc", source),)), case)


@pytest.mark.parametrize("case", ["xbox", "custom", "mcc", "none"])
def test_first_present(case):
    status, output = execute(code(), case)
    assert status == 0, output


@pytest.mark.parametrize("fault,case", [("file", "xbox"), ("mcc", "mcc"), ("refuse", "none")])
def test_negative_control(fault, case):
    source = code()
    if fault == "file":
        source = mutated(source, "if (file == INVALID_HANDLE_VALUE)\n\t{\n\t\treturn FALSE;\n\t}",
                         "if (file == INVALID_HANDLE_VALUE && 0)\n\t{\n\t\treturn FALSE;\n\t}")
    elif fault == "mcc":
        source = mutated(source, "\t\tif (level_name)\n\t\t{\n\t\t\treturn level_name;\n\t\t}",
                         "\t\t(void)level_name;")
    else:
        source = mutated(source, "\tif (multiplayer_level_present(level_name))\n\t{\n\t\treturn TRUE;\n\t}",
                         "\treturn TRUE;")
    status, output = execute(source, case)
    assert status == 1 and "check failed" in output, output
