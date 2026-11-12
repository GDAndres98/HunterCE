#include "harness.h"
#include "types.inc"
#include "constants.inc"
#define csmemcpy(destination, source, size) memcpy(destination, source, size)

/* what the function reads of the import's globals */
static struct
{
	struct mcc_import_tag_instance *instances;
	long tag_count;
	long *out_sources;
	unsigned long *out_groups;
	long *old_to_new;
	long disc_count;
	long rows_dropped;
} mcc_import_globals;

#include "under_test.inc"

/* the loaded tags: a list and a container (the disc's), the list's rows
(resume, restart, quit the disc's; save and settings MCC's own), the
container's button bar (MCC's own) and a bitmap whose first short is a
list's type */
enum
{
	LIST,
	CONTAINER,
	RESUME,
	SAVE,
	RESTART,
	SETTINGS,
	QUIT,
	BUTTON_BAR,
	BITMAP,
	TAG_COUNT
};

#define WIDGET_BYTES (V5_WIDGET_CHILD_WIDGETS_OFFSET + 12)

static byte list[WIDGET_BYTES], container[WIDGET_BYTES], bitmap[WIDGET_BYTES];
static byte list_rows[6][V5_WIDGET_CHILD_BYTES], container_rows[2][V5_WIDGET_CHILD_BYTES],
	bitmap_rows[2][V5_WIDGET_CHILD_BYTES];
static struct mcc_import_tag_instance instances[TAG_COUNT];

static void row_set(byte *row, long tag, short vertical, short horizontal)
{
	((struct tag_reference *)row)->index = tag == NONE ? NONE : (long)(0xE1740000 | tag);
	snprintf((char *)row + 0x10, 32, "row %ld", tag);
	memcpy(row + V5_WIDGET_CHILD_PLACE_OFFSET, &vertical, sizeof(vertical));
	memcpy(row + V5_WIDGET_CHILD_PLACE_OFFSET + 2, &horizontal, sizeof(horizontal));
}

static long row_tag(byte const *row)
{
	long index = ((struct tag_reference const *)row)->index;

	return index == NONE ? NONE : index & 0xFFFF;
}

static short row_place(byte const *row, short which)
{
	short value;

	memcpy(&value, row + V5_WIDGET_CHILD_PLACE_OFFSET + which * 2, sizeof(value));
	return value;
}

static void widget_set(byte *widget, short type, void *rows, long count)
{
	struct tag_block *children = (struct tag_block *)(widget + V5_WIDGET_CHILD_WIDGETS_OFFSET);

	memcpy(widget, &type, sizeof(type));
	children->count = count;
	children->address = rows;
}

int main(int argc, char **argv)
{
	/* the disc's order: the list, the container, the three rows, the
	bitmap and a tag MCC has not (written empty); the others are MCC's own */
	static long out_sources[] = { LIST, CONTAINER, RESUME, RESTART, QUIT, BITMAP, NONE };
	static unsigned long out_groups[] = { 'DeLa', 'DeLa', 'DeLa', 'DeLa', 'DeLa', 'bitm', 'DeLa' };
	static long old_to_new[TAG_COUNT] = { 0, 1, 2, NONE, 3, NONE, 4, NONE, 5 };
	char const *mode = argv[1];
	struct tag_block const *children;
	long index;

	widget_set(list, V5_WIDGET_TYPE_COLUMN_LIST, list_rows, 6);
	row_set(list_rows[0], RESUME, -2, 0);
	row_set(list_rows[1], SAVE, 28, 10);
	row_set(list_rows[2], RESTART, 58, 20);
	row_set(list_rows[3], SETTINGS, 88, 30);
	row_set(list_rows[4], NONE, 118, 40);
	row_set(list_rows[5], QUIT, 148, 50);
	widget_set(container, 0, container_rows, 2);
	row_set(container_rows[0], BUTTON_BAR, 16, 0);
	row_set(container_rows[1], RESUME, 32, 0);
	/* (a bitmap is no widget, whatever its first bytes) */
	widget_set(bitmap, V5_WIDGET_TYPE_COLUMN_LIST, bitmap_rows, 2);
	row_set(bitmap_rows[0], BUTTON_BAR, 16, 0);
	row_set(bitmap_rows[1], RESUME, 32, 0);
	for (index = 0; index < TAG_COUNT; index++)
		instances[index].group_tag = (long)(index == BITMAP ? 'bitm' : 'DeLa');
	instances[LIST].base_address = list;
	instances[CONTAINER].base_address = container;
	instances[BITMAP].base_address = bitmap;
	mcc_import_globals.instances = instances;
	mcc_import_globals.tag_count = TAG_COUNT;
	mcc_import_globals.out_sources = out_sources;
	mcc_import_globals.out_groups = out_groups;
	mcc_import_globals.old_to_new = old_to_new;
	mcc_import_globals.disc_count = 7;

	mcc_import_lists_disc_rows();

	if (!strcmp(mode, "list"))
	{
		static long const expected[] = { RESUME, RESTART, NONE, QUIT };

		children = (struct tag_block const *)(list + V5_WIDGET_CHILD_WIDGETS_OFFSET);
		CHECK(children->count == 4, "the list keeps 4 rows, not %ld", children->count);
		for (index = 0; index < 4; index++)
		{
			byte const *row = list_rows[index];
			char name[16];

			snprintf(name, sizeof(name), "row %ld", expected[index]);
			CHECK(row_tag(row) == expected[index], "row %ld is tag %ld, not %ld", index, row_tag(row), expected[index]);
			CHECK(!strcmp((char const *)row + 0x10, name), "row %ld is named %s", index, (char const *)row + 0x10);
			/* (each in the place of the row it moved into) */
			CHECK(row_place(row, 0) == (short)(index * 30 - 2), "row %ld at vertical %d",
				index, row_place(row, 0));
			CHECK(row_place(row, 1) == (short)(index * 10), "row %ld at horizontal %d", index, row_place(row, 1));
		}
		CHECK(mcc_import_globals.rows_dropped == 2, "%ld rows left out, not 2", mcc_import_globals.rows_dropped);
		return 0;
	}
	if (!strcmp(mode, "container"))
	{
		children = (struct tag_block const *)(container + V5_WIDGET_CHILD_WIDGETS_OFFSET);
		CHECK(children->count == 2, "the container keeps its 2 children, not %ld", children->count);
		CHECK(row_tag(container_rows[0]) == BUTTON_BAR && row_place(container_rows[0], 0) == 16,
			"the container's button bar stays where it was");
		children = (struct tag_block const *)(bitmap + V5_WIDGET_CHILD_WIDGETS_OFFSET);
		CHECK(children->count == 2 && row_tag(bitmap_rows[0]) == BUTTON_BAR, "the bitmap is untouched");
		return 0;
	}
	fprintf(stderr, "no case %s\n", mode);
	return 2;
}
