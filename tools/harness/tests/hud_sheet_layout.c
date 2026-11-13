/*
The high-res HUD's layout check (display.high_res_hud_by_layout): the real
hud_hires_sprite_sheet_matches (port/linux/game/hud_hires_tags.c) and
hud_hires_asset_sheet_matches (port/linux/src/hud_hires.c) over a sprite sheet
built from layout.json's sheet_sprites, whose CRCs tools/embed_assets.py
computes (sheets.inc).
*/

#include "harness.h"
#include <math.h>

/* zlib's CRC-32, as the port's (port/third_party/zlib) */
typedef unsigned long uLong;
typedef unsigned char Bytef;
#define Z_NULL 0
static uLong crc32(uLong crc, const Bytef *bytes, unsigned int length)
{
	unsigned int index, bit;

	if (!bytes)
		return 0;
	crc = ~crc & 0xFFFFFFFFUL;
	for (index = 0; index < length; index++)
	{
		crc ^= bytes[index];
		for (bit = 0; bit < 8; bit++)
			crc = crc & 1 ? (crc >> 1) ^ 0xEDB88320UL : crc >> 1;
	}
	return ~crc & 0xFFFFFFFFUL;
}

typedef struct { real x0, x1, y0, y1; } real_rectangle2d;
struct tag_block { long count; void *address; };
#define TAG_BLOCK_GET_ELEMENT(block, index, type) ((type *)(block)->address + (index))
struct bitmap_group_sprite { short bitmap_index; real_rectangle2d bounds; };
struct bitmap_group_sequence { struct tag_block sprites; };
struct bitmap_group { short type; struct tag_block sequences; };
struct bitmap_data { short width, height; };
#define csmemmove memmove
#define csmemcpy memcpy

#include "config.inc"
#include "sheets.inc"

/* one texture: the asset under test */
struct hud_hires_embedded { unsigned int sheet_crc; };
static struct hud_hires_embedded hud_hires_embedded[1];
#define hud_hires_asset_count() 1

static struct bitmap_group sheet_group;
#define bitmap_group_get(group_index) (&sheet_group)

#include "under_test.inc"

enum { MAXIMUM_TEST_SPRITES = 256 };
static struct bitmap_group_sprite sprites[MAXIMUM_TEST_SPRITES];
static struct bitmap_group_sequence sequences[4];
static struct bitmap_data bitmap;

static void sprite_add(short bitmap_index, real x0, real y0, real x1, real y1)
{
	/* (spread over the sequences, as a sheet's sequences share its bitmaps) */
	long count = 0;
	long sequence;
	struct bitmap_group_sprite *sprite;

	for (sequence = 0; sequence < 4; sequence++)
		count += sequences[sequence].sprites.count;
	CHECK(count < MAXIMUM_TEST_SPRITES, "too many sprites for the test");
	sequence = count % 4;
	sprite = (struct bitmap_group_sprite *)sequences[sequence].sprites.address + sequences[sequence].sprites.count++;
	sprite->bitmap_index = bitmap_index;
	sprite->bounds.x0 = x0;
	sprite->bounds.y0 = y0;
	sprite->bounds.x1 = x1;
	sprite->bounds.y1 = y1;
}

/* the sheet's sprites on bitmap 1, last first and each twice, the first
moved by `moved` texels, and others on bitmaps 0 and 2 */
static void sheet_build(long index, long moved, boolean others)
{
	long sprite;
	long round;

	memset(sprites, 0, sizeof(sprites));
	for (sprite = 0; sprite < 4; sprite++)
	{
		sequences[sprite].sprites.count = 0;
		sequences[sprite].sprites.address = sprites + sprite * (MAXIMUM_TEST_SPRITES / 4);
	}
	sheet_group.type = HIRES_SPRITES_GROUP_TYPE;
	sheet_group.sequences.count = 4;
	sheet_group.sequences.address = sequences;
	bitmap.width = (short)sheets[index].width;
	bitmap.height = (short)sheets[index].height;
	hud_hires_embedded[0].sheet_crc = sheets[index].crc;
	for (round = 0; round < 2; round++)
	{
		for (sprite = sheets[index].count - 1; sprite >= 0; sprite--)
		{
			long const *rect = sheets[index].rects[sprite];

			sprite_add(1, (real)(rect[0] + (sprite ? 0 : moved)) / bitmap.width, (real)rect[1] / bitmap.height,
				(real)(rect[2] + (sprite ? 0 : moved)) / bitmap.width, (real)rect[3] / bitmap.height);
		}
	}
	if (others)
	{
		sprite_add(0, 0.0f, 0.0f, 0.25f, 0.25f);
		sprite_add(2, 0.5f, 0.5f, 1.0f, 1.0f);
	}
}

static boolean matches(void)
{
	return hud_hires_sprite_sheet_matches(0, 1, &bitmap, 0);
}

int main(int argc, char **argv)
{
	char const *name = argc > 1 ? argv[1] : "";
	long index;

	CHECK(NUMBEROF_SHEETS > 0, "no sheet_sprites in layout.json");
	if (!strcmp(name, "group-type"))
	{
		CHECK(HIRES_SPRITES_GROUP_TYPE == _bitmap_group_type_sprites, "not the sprites' group type");
	}
	else if (!strcmp(name, "disc-layout"))
	{
		for (index = 0; index < NUMBEROF_SHEETS; index++)
		{
			sheet_build(index, 0, FALSE);
			CHECK(matches(), "sheet %ld laid out as the disc's is not recognised", index);
		}
	}
	else if (!strcmp(name, "other-bitmaps"))
	{
		for (index = 0; index < NUMBEROF_SHEETS; index++)
		{
			sheet_build(index, 0, TRUE);
			CHECK(matches(), "sheet %ld: sprites on other bitmaps were counted", index);
		}
	}
	else if (!strcmp(name, "moved-sprite"))
	{
		for (index = 0; index < NUMBEROF_SHEETS; index++)
		{
			sheet_build(index, 1, FALSE);
			CHECK(!matches(), "sheet %ld with a sprite moved a texel was taken", index);
		}
	}
	else if (!strcmp(name, "extra-sprite"))
	{
		for (index = 0; index < NUMBEROF_SHEETS; index++)
		{
			sheet_build(index, 0, FALSE);
			sprite_add(1, 0.0f, 0.0f, 0.5f, 0.75f);
			CHECK(!matches(), "sheet %ld with a sprite of its own was taken", index);
		}
	}
	else if (!strcmp(name, "outside"))
	{
		sheet_build(0, 0, FALSE);
		sprite_add(1, 0.0f, 0.0f, 1.5f, 0.5f);
		CHECK(!matches(), "a sprite outside the bitmap was taken");
		sheet_build(0, 0, FALSE);
		sprites[0].bounds.y1 = NAN;
		CHECK(!matches(), "a sprite whose bounds are not numbers was taken");
	}
	else if (!strcmp(name, "no-sheet"))
	{
		sheet_build(0, 0, FALSE);
		sheet_group.type = _bitmap_group_type_2d_textures;
		CHECK(!matches(), "a group of 2D textures was taken as a sprite sheet");
		sheet_build(0, 0, FALSE);
		hud_hires_embedded[0].sheet_crc = 0;
		CHECK(!matches(), "a texture with no sheet was taken");
		sheet_build(0, 0, FALSE);
		sheet_group.sequences.count = 0;
		CHECK(!matches(), "a sheet with no sprites was taken");
	}
	else if (!strcmp(name, "too-many"))
	{
		sheet_build(0, 0, FALSE);
		sheet_group.sequences.count = 4;
		for (index = 0; index < 4; index++)
			sequences[index].sprites.count = 0;
		for (index = 0; index < 200; index++)
			sprite_add(1, (real)index / 256.0f, 0.0f, (real)(index + 1) / 256.0f, 1.0f);
		bitmap.width = 256;
		CHECK(!matches(), "a sheet of more sprites than the check holds was taken");
	}
	else
	{
		CHECK(0, "no case %s", name);
	}
	return 0;
}
