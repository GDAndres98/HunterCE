/*
MAP_PRESENT_KIND.C (test)

source/cache/cache_files.c's cache_files_map_present, for one of the game's
own maps: a client takes the host's map only when it is of the host's kind
(MCC's or the disc's: cache_files_map_version), whether it is precached or
not. test_map_present_kind.py takes the code (under_test.inc).
*/

#include "harness.h"

#include <stddef.h>

#define NUMBEROF(array) (sizeof(array) / sizeof((array)[0]))

typedef void *HANDLE;
#define INVALID_HANDLE_VALUE ((HANDLE)-1)
#define GENERIC_READ 0x80000000UL
#define OPEN_EXISTING 3

/* the client's maps: whether it has the file, whether it is precached, and
its kind (cache_files_map_version's: 0 the disc's, MCC_VERSION MCC's) */
#define MCC_VERSION 0x4D434331UL
static boolean has_file, precached;
static unsigned long local_version;
static boolean error_shown;

#define tag_name_strip_path(name) (name)
#define mcc_level_name(name) FALSE
#define mcc_share_game_downloading(name) FALSE
#define mcc_cache_require(name, version) FALSE
#define custom_edition_level_name(name) FALSE
#define custom_edition_cache_present(name, version, message, size) FALSE
#define map_share_download_begin(name, version, message) FALSE
#define custom_edition_map_file_present(name) FALSE
#define cache_files_map_directory() "maps/"
#define cache_files_precache_map_loaded(name) precached
#define cache_files_map_version(name) local_version
#define mcc_map_version() MCC_VERSION
#define CreateFileA(path, access, share, security, disposition, flags, template) \
	(has_file ? (HANDLE)1 : INVALID_HANDLE_VALUE)
#define CloseHandle(file) ((void)0)
#define display_error_text_when_main_menu_loaded(text) (error_shown = TRUE)

void platform_log(char const *format, ...)
{
	(void)format;
}

#include "under_test.inc"

int main(int argc, char **argv)
{
	char const *case_name = argc > 1 ? argv[1] : "";
	boolean present;

	has_file = TRUE;
	precached = FALSE;
	local_version = 0;
	error_shown = FALSE;
	if (!strcmp(case_name, "same-kind"))
	{
		present = cache_files_map_present("bloodgulch", 0);
		CHECK(present && !error_shown, "the disc's map with a disc host is present");
	}
	else if (!strcmp(case_name, "other-kind"))
	{
		present = cache_files_map_present("bloodgulch", MCC_VERSION);
		CHECK(!present && error_shown, "the disc's map with an MCC host is refused");
	}
	else if (!strcmp(case_name, "precached-same-kind"))
	{
		precached = TRUE;
		has_file = FALSE;
		present = cache_files_map_present("bloodgulch", 0);
		CHECK(present, "a precached map of the host's kind is present");
	}
	else if (!strcmp(case_name, "precached-other-kind"))
	{
		precached = TRUE;
		local_version = MCC_VERSION;
		present = cache_files_map_present("bloodgulch", 0);
		CHECK(!present && error_shown, "a precached MCC map with a disc host is refused");
	}
	else if (!strcmp(case_name, "missing"))
	{
		has_file = FALSE;
		present = cache_files_map_present("bloodgulch", 0);
		CHECK(!present && error_shown, "a missing map is refused");
	}
	else
	{
		fprintf(stderr, "unknown case %s\n", case_name);
		return 2;
	}
	return 0;
}
