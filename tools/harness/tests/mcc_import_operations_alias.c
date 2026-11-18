#include "harness.h"

struct mcc_patch_operation
{
	char *kind;
	char *tag;
	char *path;
	char *a;
	char *b;
};

/* what the functions read of the operations' globals */
static struct
{
	struct mcc_patch_operation *operations;
	long operation_count;
	byte *arena;
	unsigned long arena_used;
} mcc_patch_globals;

#include "under_test.inc"

#define PAUSE "ui\\shell\\solo_game\\pause_game\\"

static struct mcc_patch_operation operations[] =
{
	{ "set", "DeLa:" PAUSE "pausebox2_left", "root", "0x28", "9f" },
	{ "alias", "DeLa:" PAUSE "pausebox2_left", "-", "DeLa:" PAUSE "pausebox4_left", NULL },
	{ "alias", "bitm:ui\\shell\\bitmaps\\pausebox2_left", "-", "bitm:ui\\shell\\bitmaps\\pausebox5_left", NULL },
	/* (another group's name: not an alias) */
	{ "alias", "DeLa:" PAUSE "pausebox2_right", "-", "bitm:ui\\shell\\bitmaps\\pausebox5_right", NULL },
	{ "alias", "DeLa:" PAUSE "pausebox_left", "-", NULL, NULL },
};

int main(int argc, char **argv)
{
	char const *mode = argv[1];
	static byte arena[64], other[16];

	mcc_patch_globals.operations = operations;
	mcc_patch_globals.operation_count = sizeof(operations) / sizeof(operations[0]);
	mcc_patch_globals.arena = arena;
	mcc_patch_globals.arena_used = 32;
	if (!strcmp(mode, "alias"))
	{
		char const *name = mcc_import_operations_alias('DeLa', PAUSE "pausebox2_left");

		CHECK(name && !strcmp(name, PAUSE "pausebox4_left"), "the disc's widget is MCC's pausebox4_left (%s)",
			name ? name : "none");
		name = mcc_import_operations_alias('bitm', "ui\\shell\\bitmaps\\pausebox2_left");
		CHECK(name && !strcmp(name, "ui\\shell\\bitmaps\\pausebox5_left"), "the disc's bitmap is MCC's pausebox5_left");
		return 0;
	}
	if (!strcmp(mode, "none"))
	{
		CHECK(!mcc_import_operations_alias('bitm', PAUSE "pausebox2_left"), "no alias of another group's tag");
		CHECK(!mcc_import_operations_alias('DeLa', PAUSE "pausebox2_right"), "no alias to another group");
		CHECK(!mcc_import_operations_alias('bitm', PAUSE "pausebox2_right"), "no alias of a name of another group");
		CHECK(!mcc_import_operations_alias('DeLa', PAUSE "pausebox_left"), "no alias without MCC's name");
		CHECK(!mcc_import_operations_alias('DeLa', PAUSE "pause_list"), "no alias of a tag no line names");
		return 0;
	}
	if (!strcmp(mode, "made"))
	{
		CHECK(mcc_import_operations_made(arena, 32), "the arena's used bytes are the operations'");
		CHECK(mcc_import_operations_made(arena + 16, 16), "a part of them too");
		CHECK(!mcc_import_operations_made(arena + 16, 17), "not past what is used");
		CHECK(!mcc_import_operations_made(arena + 40, 4), "not the arena's unused bytes");
		CHECK(!mcc_import_operations_made(other, 4), "not another address (a map's)");
		mcc_patch_globals.arena = NULL;
		CHECK(!mcc_import_operations_made(arena, 4), "none without an arena");
		return 0;
	}
	fprintf(stderr, "no case %s\n", mode);
	return 2;
}
