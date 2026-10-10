/*
	font.c

	Font management

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/8/26

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <math.h>

#include <fontconfig/fontconfig.h>

#include "QF/prefixsum.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/math/bitop.h"

#include "QF/plugin/vid_render.h"

#include "QF/thread/notifier.h"
#include "QF/thread/schedule.h"
#include "QF/ui/font.h"
#include "QF/ui/glyphcache.h"

#include "compat.h"

#include FT_OUTLINE_H
#include FT_BBOX_H

static FT_Library ft;

static void
copy_glyph (scrapbox_t *rect, FT_GlyphSlot src_glyph, font_t *font)
{
	int         dst_pitch = font->scrap.width;
	byte       *dst = font->scrap_bitmap + rect->x + rect->y * dst_pitch;
	int         src_pitch = src_glyph->bitmap.pitch;
	byte       *src = src_glyph->bitmap.buffer;

	for (unsigned i = 0; i < src_glyph->bitmap.rows; i++) {
		memcpy (dst, src, src_glyph->bitmap.width);
		dst += dst_pitch;
		src += src_pitch;
	}
}

static void
Font_shutdown (void *data)
{
	FT_Done_FreeType (ft);
}

VISIBLE void
Font_Init (void)
{
	qfZoneScoped (true);
	if (FT_Init_FreeType (&ft)) {
		Sys_Error ("Could not init FreeType library");
	}
	Sys_RegisterShutdown (Font_shutdown, 0);
}

VISIBLE void
Font_Free (font_t *font)
{
	if (font->face) {
		FT_Done_Face (font->face);
	}
	if (font->scrap.free_rects) {
		R_ScrapDelete (&font->scrap);
	}
	free (font->glyph_rects);
	free (font->glyph_bearings);
	free (font->scrap_bitmap);
	free (font->font_data);
	free (font);
}

VISIBLE font_t *
Font_Load (QFile *font_file, int index, int size)
{
	byte       *font_data = QFS_LoadFile (font_file, 0);
	if (!font_data) {
		return 0;
	}
	size_t      font_size = qfs_filesize;
	font_t     *font = calloc (1, sizeof (font_t));
	font->font_data = font_data;
	font->font_size = font_size;
	if (FT_New_Memory_Face (ft, font_data, font_size, index, &font->face)) {
		Font_Free (font);
		return 0;
	}

	FT_Set_Pixel_Sizes(font->face, 0, size);
	int         pixels = 0;
	for (FT_Long gind = 0; gind < font->face->num_glyphs; gind++) {
		FT_Load_Glyph (font->face, gind, FT_LOAD_DEFAULT);

		__auto_type g = font->face->glyph;
		// include padding around the glyph to avoid texel leaks
		pixels += (g->bitmap.width + 1) * (g->bitmap.rows + 1);
	}
	pixels = sqrt (5 * pixels / 4);
	pixels = BITOP_RUP (pixels);
	R_ScrapInit (&font->scrap, pixels, pixels);
	font->scrap_bitmap = calloc (1, pixels * pixels);
	font->num_glyphs = font->face->num_glyphs;
	font->glyph_rects = malloc (font->num_glyphs * sizeof (vrect_t));
	font->glyph_bearings = malloc (font->num_glyphs * sizeof (vec2i_t));

	for (FT_Long gind = 0; gind < font->face->num_glyphs; gind++) {
		scrapbox_t *rect = &font->glyph_rects[gind];
		vec2i_t    *bearing = &font->glyph_bearings[gind];
		FT_Load_Glyph (font->face, gind, FT_LOAD_DEFAULT);
		__auto_type slot = font->face->glyph;
		FT_Render_Glyph (slot, FT_RENDER_MODE_NORMAL);
		// add padding to create a buffer around the glyph to prevent texel
		// leaks
		int     width = slot->bitmap.width + 1;
		int     height = slot->bitmap.rows + 1;
		*rect = *R_ScrapAlloc (&font->scrap, width, height);
		*bearing = (vec2i_t) { slot->bitmap_left, slot->bitmap_top };
		// shrink the rect so as to NOT include the padding
		rect->width -= 1;
		rect->height -= 1;

		copy_glyph (rect, slot, font);
	}
	font->fontid = r_funcs->draw.AddFont (font);

	return font;
}

fontspec_t
Font_SystemFont (const char *font_pattern)
{
	auto config = FcInitLoadConfigAndFonts ();
	auto pattern = FcNameParse ((const FcChar8 *) font_pattern);
	FcConfigSubstitute (config, pattern, FcMatchPattern);
	FcDefaultSubstitute (pattern);

	FcResult result;
	auto font = FcFontMatch (config, pattern, &result);
	fontspec_t fontspec = {};
	if (font) {
		FcChar8 *str;
		if (FcPatternGetString (font, FC_FILE, 0, &str) == FcResultMatch) {
			fontspec.path = strdup ((char *) str);
		}
		FcPatternGetInteger (font, FC_INDEX, 0, &fontspec.index);
		FcPatternDestroy (font);
	}
	FcPatternDestroy (pattern);
	return fontspec;
}

typedef struct fontctx_s {
	glyphkey_t *keys;
	FT_BBox    *bboxes;
	FT_Face    *faces;
	byte       *buffer;
	uint32_t   *offsets;

	task_t     *tasks;
	notifier_t *notifier;
} fontctx_t;

static void
glyph_wait (task_t *task, int worker_id)
{
	auto ctx = *(fontctx_t *) task->data;
	notifier_notify_all (ctx.notifier);
	//int ind = task - ctx.tasks;
	//printf ("glyph_wait: %d %d\n", worker_id, ind);
}

static void
glyph_setup (int worker_id, task_t *task)
{
	auto ctx = *(fontctx_t *) task->data;
	int index = task - ctx.tasks;
	auto key = ctx.keys[index];
	auto face = ctx.faces[worker_id];

	FT_Set_Char_Size (face, 0, key.ptsize, 72, 72);
	FT_Load_Glyph (face, key.glyphid, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP);
	FT_Pos shift_x = key.subpixel_x << 4;
	FT_Pos shift_y = key.subpixel_y << 4;
	FT_Outline_Translate (&face->glyph->outline, shift_x, shift_y);
}

static void
glyph_calc_bounds (task_t *task, int worker_id)
{
	auto ctx = *(fontctx_t *) task->data;
	int index = task - ctx.tasks;

	glyph_setup (worker_id, task);

	auto face = ctx.faces[worker_id];
	auto bbox = &ctx.bboxes[index];
	FT_Outline_Get_BBox (&face->glyph->outline, bbox);
	int x_min = bbox->xMin >> 6;
	int y_min = bbox->yMin >> 6;
	int x_max = (bbox->xMax + 63) >> 6;
	int y_max = (bbox->yMax + 63) >> 6;
	int x_len = x_max - x_min;
	int y_len = y_max - y_min;
	ctx.offsets[index] = x_len * y_len;
	//printf ("glyph_calc_bounds: %d %d -> %d [%d %d]\n",
	//		worker_id, index, ctx.offsets[index], x_len, y_len);
}

typedef struct renderctx_s {
	byte       *buffer;
	int         x_len;
	int         y_len;
	int         x_pos;
	int         y_pos;
} renderctx_t;

static void
glyph_spans (int y, int count, const FT_Span *spans, void *user)
{
	auto ctx = *(renderctx_t *) user;

	if (y < 0 || y >= ctx.y_len) {
		return;
	}
	int row = ctx.y_pos - y;
	int ofs = row * ctx.x_len;
	auto data = ctx.buffer + ofs;
	for (int i = 0; i < count; i++) {
		int start = ctx.x_pos + spans[i].x;
		byte coverage = spans[i].coverage;
		for (int j = 0; j < spans[i].len; j++) {
			int x = start + j;
			if (x >= 0 && x < ctx.x_len) {
				data[x] = coverage;
			}
		}
	}
}

static void
glyph_render (task_t *task, int worker_id)
{
	auto ctx = *(fontctx_t *) task->data;
	int index = task - ctx.tasks;

	glyph_setup (worker_id, task);

	auto face = ctx.faces[worker_id];
	auto bbox = &ctx.bboxes[index];

	int x_min = bbox->xMin >> 6;
	int y_min = bbox->yMin >> 6;
	int x_max = (bbox->xMax + 63) >> 6;
	int y_max = (bbox->yMax + 63) >> 6;
	int x_len = x_max - x_min;
	int y_len = y_max - y_min;

	renderctx_t renderctx = {
		.buffer = ctx.buffer + ctx.offsets[index],
		.x_len = x_len,
		.y_len = y_len,
		.x_pos = -x_min,
		.y_pos = y_max - 1,
	};
	FT_Raster_Params params = {
		.flags = FT_RASTER_FLAG_DIRECT | FT_RASTER_FLAG_AA,
		.gray_spans = glyph_spans,
		.user = &renderctx,
	};
	//printf ("glyph_render: %d %d -> %d [%d %d]\n",
	//		worker_id, index, ctx.offsets[index], x_len, y_len);
	FT_Outline_Render (face->glyph->library, &face->glyph->outline, &params);
}

void
Font_LoadGlyphs (font_t *font, glyphkey_t *keys, int count, wssched_t *sched)
{
	if (count < 1) {
		return;
	}

	int num_workers = wssched_worker_count (sched);
	FT_Face faces[num_workers];
	for (int i = 0; i < num_workers; i++) {
		FT_New_Memory_Face (ft, font->font_data, font->font_size, 0, &faces[i]);
	}

	FT_BBox bboxes[count] = {};
	uint32_t offsets[count] = {};

	waiter_t    waiter = {
		.mut = PTHREAD_MUTEX_INITIALIZER,
		.cond = PTHREAD_COND_INITIALIZER,
	};
	task_t glyph_tasks[count + 1];
	fontctx_t   fontctx = {
		.keys = keys,
		.bboxes = bboxes,
		.faces = faces,
		.offsets = offsets,
		.tasks = glyph_tasks,
		.notifier = notifier_new (1, &waiter)
	};
	auto wait_task = &glyph_tasks[count];
	*wait_task = (task_t) {
		.dependency_count = count,
		.execute = glyph_wait,
		.data = &fontctx,
	};

	task_t *glyph_task_set[count];
	for (int i = 0; i < count; i++) {
		glyph_tasks[i] = (task_t) {
			.child_count = 1,
			.children = &wait_task,
			.execute = glyph_calc_bounds,
			.data = &fontctx,
		};
		glyph_task_set[i] = &glyph_tasks[i];
	}

	wssched_insert (sched, count, glyph_task_set);

	notifier_prepare_to_wait (fontctx.notifier, &waiter);
	notifier_commit_wait (fontctx.notifier, &waiter);

	uint32_t size = prefixsum_uint32 (offsets, count);
	//printf ("%d bytes\n", size);

	wait_task->dependency_count = count;
	for (int i = 0; i < count; i++) {
		glyph_tasks[i].execute = glyph_render;
	}
	byte buffer[size] = {};
	fontctx.buffer = buffer;

	wssched_insert (sched, count, glyph_task_set);

	notifier_prepare_to_wait (fontctx.notifier, &waiter);
	notifier_commit_wait (fontctx.notifier, &waiter);

	//for (int i = 0; i < count; i++) {
	//	auto bbox = bboxes[i];
	//	int x_min = bbox.xMin >> 6;
	//	int y_min = bbox.yMin >> 6;
	//	int x_max = (bbox.xMax + 63) >> 6;
	//	int y_max = (bbox.yMax + 63) >> 6;
	//	int x_len = x_max - x_min;
	//	int y_len = y_max - y_min;
	//	byte *data = buffer + offsets[i];
	//	for (int y = 0; y < y_len; y++) {
	//		for (int x = 0; x < x_len; x++) {
	//			putc (" .oO"[data[y * x_len + x] / 64], stdout);
	//		}
	//		putc ('\n', stdout);
	//	}
	//	putc ('\n', stdout);
	//}

	for (int i = 0; i < num_workers; i++) {
		FT_Done_Face (faces[i]);
	}
	notifier_delete (fontctx.notifier);
}
