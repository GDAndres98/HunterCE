#include "harness.h"

/* the maps this machine has: Xbox map files in its maps folder, Custom
Edition maps, MCC maps (each case sets them) */
static char const *xbox_files[4];
static char const *custom_maps[4];
static char const *mcc_multiplayer[4];
static char const *list_levels[16];
static short list_count;

typedef void *HANDLE;
#define INVALID_HANDLE_VALUE ((HANDLE)-1)
#define GENERIC_READ 0
#define OPEN_EXISTING 0

static int listed(char const *const *names, char const *name)
{
	int index;

	for (index = 0; index < 4 && names[index]; index++)
	{
		if (!strcmp(names[index], name))
			return 1;
	}
	return 0;
}

static char const *tag_name_strip_path(char const *name)
{
	char const *last = strrchr(name, '\\');

	return last ? last + 1 : name;
}

#define cache_files_map_directory() "maps\\"
static HANDLE CreateFileA(char const *path, int access, int share, void *security, int create, int flags, void *template_file)
{
	char name[64];
	size_t length;

	(void)access; (void)share; (void)security; (void)create; (void)flags; (void)template_file;
	snprintf(name, sizeof(name), "%s", path + strlen("maps\\"));
	length = strlen(name);
	if (length > 4)
		name[length - 4] = 0;
	return listed(xbox_files, name) ? (HANDLE)1 : INVALID_HANDLE_VALUE;
}
#define CloseHandle(file) ((void)(file))
#define mcc_level_name(name) (!strncmp(name, "mcc_maps\\", 9))
#define mcc_maps_find(name) (listed(mcc_multiplayer, name) ? 0 : NONE)
#define custom_edition_level_name(name) (!strncmp(name, "custom_maps\\", 12))
#define custom_edition_cache_playable(name) listed(custom_maps, name)
#define mcc_maps_type_count(campaign) (campaign ? 0 : count_of(mcc_multiplayer))
#define mcc_maps_type_index(row, campaign) (row)
#define mcc_maps_level_name(index) (mcc_multiplayer[index])
#include <wchar.h>
#define NUMBEROF(array) ((short)(sizeof(array) / sizeof((array)[0])))

static int messages;
static wchar_t const *message_text;

static void display_error_text_deferred(wchar_t const *text, short local_player_index)
{
	(void)local_player_index;
	message_text = text;
	messages++;
}

static short count_of(char const *const *names)
{
	short count = 0;

	while (count < 4 && names[count])
		count++;
	return count;
}

static short ui_widget_port_multiplayer_maps(char const *const **names, short *last_used)
{
	(void)last_used;
	*names = list_levels;
	return list_count;
}

#include "under_test.inc"

int main(int argc, char **argv)
{
	char const *mode = argv[1];
	char const *first;
	short index;

	/* the Xbox's levels, then the Custom Edition maps, as the level list */
	static char const *const xbox_levels[] = { "levels\\test\\carousel\\carousel", "levels\\test\\bloodgulch\\bloodgulch" };

	for (index = 0; index < 2; index++)
		list_levels[list_count++] = xbox_levels[index];
	if (!strcmp(mode, "xbox"))
	{
		xbox_files[0] = "bloodgulch";
		custom_maps[0] = list_levels[list_count++] = "custom_maps\\hugeass";
		mcc_multiplayer[0] = "mcc_maps\\mcc_multiplayer\\icefields";
		first = multiplayer_level_first_present();
		CHECK(first && !strcmp(first, "levels\\test\\bloodgulch\\bloodgulch"), "the Xbox map the folder has, not %s",
			first ? first : "none");
		CHECK(!multiplayer_level_present("levels\\test\\carousel\\carousel"), "carousel, which the folder lacks");
		return 0;
	}
	if (!strcmp(mode, "custom"))
	{
		custom_maps[0] = list_levels[list_count++] = "custom_maps\\hugeass";
		mcc_multiplayer[0] = "mcc_maps\\mcc_multiplayer\\icefields";
		first = multiplayer_level_first_present();
		CHECK(first && !strcmp(first, "custom_maps\\hugeass"), "the Custom Edition map, not %s", first ? first : "none");
		return 0;
	}
	if (!strcmp(mode, "mcc"))
	{
		mcc_multiplayer[0] = "mcc_maps\\mcc_multiplayer\\icefields";
		first = multiplayer_level_first_present();
		CHECK(first && !strcmp(first, "mcc_maps\\mcc_multiplayer\\icefields"), "the MCC map, not %s",
			first ? first : "none");
		CHECK(multiplayer_level_present(first), "an MCC map in mcc_maps is present");
		CHECK(!multiplayer_level_present("mcc_maps\\mcc_multiplayer\\timberland"), "one mcc_maps lacks is not");
		return 0;
	}
	if (!strcmp(mode, "none"))
	{
		CHECK(!multiplayer_level_first_present(), "no multiplayer map at all");
		CHECK(!multiplayer_level_chosen_present("levels\\test\\carousel\\carousel"), "a missing map chosen is refused");
		CHECK(messages == 1, "and said once, not %d times", messages);
		CHECK(message_text && wcsstr(message_text, L"carousel.map"), "naming the map");
		return 0;
	}
	fprintf(stderr, "no case %s\n", mode);
	return 2;
}
