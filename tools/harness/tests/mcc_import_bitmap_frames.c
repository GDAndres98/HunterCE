#include "harness.h"

/* the small types the bitmap structures use (their sizes are the game's) */
union point2d { struct { short x, y; } s; long n; };
typedef struct { real x0, x1, y0, y1; } real_rectangle2d;
typedef struct { real x, y; } real_point2d;
#include "types.inc"
#define csmemcpy(destination, source, size) memcpy(destination, source, size)
#define TAG_BLOCK_GET_ELEMENT(block, index, type) ((type *)(block)->address + (index))
#define mcc_import_error(...) (fprintf(stderr, __VA_ARGS__), fputc(10, stderr))
#define MCC_IMPORT_MAXIMUM_ORDERED_FRAMES 64

/* what the function reads of the import's globals */
static struct
{
	struct mcc_import_tag_instance *instances;
	long frames_ordered;
} mcc_import_globals;

/* the operations' bitmap_frames line of the bitmap (NULL: none) */
static char const *frames_line;

static char const *mcc_import_operations_bitmap_frames(char const *name)
{
	return frames_line;
}

static void *mcc_import_operations_allocate(unsigned long size)
{
	return calloc(1, size);
}

#include "under_test.inc"

#define MCC_FRAMES 9

static struct mcc_import_tag_instance instance;
static struct bitmap_group group;
static struct bitmap_group_sequence sequence;
static struct bitmap_group_sprite sprite;
static struct bitmap_data mcc_frames[MCC_FRAMES];

/* a bitmap of `count` of MCC's frames, frame n at pixels n * 100 and n + 1
wide, in one sequence of them all (with a sprite when `sprites`) */
static void bitmap_set(long count, boolean sprites)
{
	long index;

	memset(&group, 0, sizeof(group));
	memset(&sequence, 0, sizeof(sequence));
	for (index = 0; index < count; index++)
	{
		memset(&mcc_frames[index], 0, sizeof(mcc_frames[index]));
		mcc_frames[index].width = (short)(index + 1);
		mcc_frames[index].height = 256;
		mcc_frames[index].format = 15;
		mcc_frames[index].pixels_offset = index * 100;
		mcc_frames[index].pixels_size = 0x20000;
	}
	group.bitmaps.count = count;
	group.bitmaps.address = mcc_frames;
	sequence.bitmap_count = (short)count;
	sequence.sprites.count = sprites ? 1 : 0;
	sequence.sprites.address = sprites ? &sprite : NULL;
	group.sequences.count = 1;
	group.sequences.address = &sequence;
	instance.group_tag = 'bitm';
	instance.name = "ui\\shell\\main_menu\\multiplayer_type_select\\mp_options";
	instance.base_address = &group;
	mcc_import_globals.instances = &instance;
	mcc_import_globals.frames_ordered = 0;
}

static struct bitmap_data const *frame(long index)
{
	return (struct bitmap_data const *)group.bitmaps.address + index;
}

/* the disc's frame `index` is MCC's frame `source`, or empty (NONE) of the
size of MCC's frame `model` */
static void frame_check(long index, long source, long model)
{
	CHECK(frame(index)->pixels_offset == (source == NONE ? NONE : source * 100),
		"the disc's frame %ld has MCC's frame %ld's pixels (offset %ld)", index, source, (long)frame(index)->pixels_offset);
	CHECK(frame(index)->width == (source == NONE ? model : source) + 1, "the disc's frame %ld has its size", index);
}

static void unchanged_check(long count)
{
	CHECK(group.bitmaps.address == mcc_frames && group.bitmaps.count == count, "the frames are MCC's");
	CHECK(sequence.bitmap_count == count, "the sequence is MCC's");
	CHECK(mcc_import_globals.frames_ordered == 0, "no bitmap counted");
}

int main(int argc, char **argv)
{
	char const *mode = argv[1];

	CHECK(sizeof(struct bitmap_data) == 0x30 && sizeof(struct bitmap_group_sequence) == 0x40,
		"the structures are the game's");
	if (!strcmp(mode, "larger"))
	{
		/* (the multiplayer options: MCC's 3, the disc's 4, the last MCC's 2) */
		bitmap_set(3, FALSE);
		frames_line = "-1,-1,-1,2";
		mcc_import_bitmap_frames(0);
		CHECK(group.bitmaps.count == 4 && sequence.bitmap_count == 4, "the disc's 4 frames, in one sequence");
		frame_check(0, NONE, 2);
		frame_check(1, NONE, 2);
		frame_check(2, NONE, 2);
		frame_check(3, 2, 2);
		CHECK(mcc_import_globals.frames_ordered == 1, "one bitmap counted");
		return 0;
	}
	if (!strcmp(mode, "smaller"))
	{
		/* (the profile options: MCC's 9, the disc's 5) */
		bitmap_set(9, FALSE);
		frames_line = "0,2,-1,7,-1";
		mcc_import_bitmap_frames(0);
		CHECK(group.bitmaps.count == 5 && sequence.bitmap_count == 5, "the disc's 5 frames, in one sequence");
		frame_check(0, 0, 0);
		frame_check(1, 2, 0);
		frame_check(2, NONE, 0);
		frame_check(3, 7, 0);
		frame_check(4, NONE, 0);
		return 0;
	}
	if (!strcmp(mode, "sprites"))
	{
		bitmap_set(3, TRUE);
		frames_line = "2,1,0";
		mcc_import_bitmap_frames(0);
		unchanged_check(3);
		return 0;
	}
	if (!strcmp(mode, "range"))
	{
		bitmap_set(3, FALSE);
		frames_line = "0,3";
		mcc_import_bitmap_frames(0);
		unchanged_check(3);
		return 0;
	}
	if (!strcmp(mode, "empty"))
	{
		bitmap_set(3, FALSE);
		frames_line = "-1,-1";
		mcc_import_bitmap_frames(0);
		unchanged_check(3);
		return 0;
	}
	if (!strcmp(mode, "none"))
	{
		bitmap_set(3, FALSE);
		frames_line = NULL;
		mcc_import_bitmap_frames(0);
		unchanged_check(3);
		return 0;
	}
	fprintf(stderr, "no case %s\n", mode);
	return 2;
}
