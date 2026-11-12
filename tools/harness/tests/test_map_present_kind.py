"""cache_files_map_present for one of the game's own maps: the host's map is taken only when the client's is of the
host's kind (MCC's or the disc's), precached or not."""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from harness import CHECK_FAILED, build, function, mutated, read, run  # noqa: E402

CASES = ["same-kind", "other-kind", "precached-same-kind", "precached-other-kind", "missing"]

# a fault in cache_files.c and the case that must catch it
NEGATIVE_CONTROLS = {
    "precached-any-kind": (("if (!other_kind && cache_files_precache_map_loaded(map_name))",
                            "if (cache_files_precache_map_loaded(map_name))"), "precached-other-kind"),
    "kind-ignored": (("if (file != INVALID_HANDLE_VALUE && other_kind)", "if (FALSE)"), "other-kind"),
}


def generated(fault=None):
    code = function(read("source/cache/cache_files.c"), "cache_files_map_present")
    if fault:
        code = mutated(code, *fault)
    return (("under_test.inc", code),)


@pytest.mark.parametrize("case", CASES)
def test_case(case):
    status, output = run(build("map_present_kind", generated()), case)
    assert status == 0, output


@pytest.mark.parametrize("control", sorted(NEGATIVE_CONTROLS))
def test_negative_control(control):
    fault, case = NEGATIVE_CONTROLS[control]
    status, output = run(build("map_present_kind", generated(fault)), case)
    assert status == CHECK_FAILED, f"{control} was not caught by {case}: {output}"
