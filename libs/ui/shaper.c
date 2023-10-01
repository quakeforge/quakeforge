/*
	shaper.c

	Immediate mode user inferface

	Copyright (C) 2023 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2023/07/01

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
#include <stdlib.h>
#include <string.h>

#include "QF/darray.h"
#include "QF/ecs.h"
#include "QF/fbsearch.h"
#include "QF/hash.h"
#include "QF/mathlib.h"
#include "QF/progs.h"
#include "QF/quakeio.h"
#include "QF/va.h"

#include "QF/input/event.h"

#include "QF/ui/font.h"
#include "QF/ui/shaper.h"

typedef struct shaper_cache_s {
	struct shaper_cache_s *next;
	struct shaper_cache_s **prev;
	script_component_t script;
	featureset_t features;
	const font_t *font;
	char       *text;
	size_t      text_len;
	hb_buffer_t *buffer;
} shaper_cache_t;

typedef struct shaper_font_s {
	hb_font_t  *hb_font;
	uint32_t    fontid;
} shaper_font_t;

struct text_shaper_s {
	hashtab_t  *tab;
	hashctx_t  *hashctx;
	PR_RESMAP (shaper_cache_t) cache_map;
	shaper_cache_t *buffers;
	shaper_cache_t *unused_buffers;	// buffers not used since last flush
	struct DARRAY_TYPE (shaper_font_t) fonts;
};

static uintptr_t
shaper_get_hash (const void *obj, void *data)
{
	const shaper_cache_t *cache = obj;
	uintptr_t script = Hash_Buffer (&cache->script, sizeof (cache->script));
	auto f = &cache->features;
	uintptr_t features = 0;
	if (f->a && f->size) {
		features = Hash_Buffer (f->a, sizeof (hb_feature_t[f->size]));
	}
	uintptr_t font = cache->font->fontid;
	uintptr_t text = Hash_nString (cache->text, cache->text_len);
	return script + features + font + text;
}

static int
shaper_compare (const void *a, const void *b, void *data)
{
	const shaper_cache_t *cachea = a;
	const shaper_cache_t *cacheb = b;
	if (cachea->font->fontid != cacheb->font->fontid) {
		return 0;
	}
	auto af = &cachea->features;
	auto bf = &cacheb->features;
	if (af->size != bf->size || (!af->a != !bf->a)) {
		return 0;
	}
	if (af->size && memcmp (af->a, &bf->a, sizeof (hb_feature_t[af->size]))) {
		return 0;
	}
	if (memcmp (&cachea->script, &cacheb->script, sizeof (cachea->script))) {
		return 0;
	}
	return (cachea->text_len == cacheb->text_len
			&& !strncmp (cachea->text, cacheb->text, cachea->text_len));
}

static void
shaper_free_cache_data (shaper_cache_t *cache)
{
	free (cache->text);
	free (cache->features.a);
	if (cache->buffer) {
		hb_buffer_destroy (cache->buffer);
	}
}

text_shaper_t *
Shaper_New (void)
{
	text_shaper_t *shaper = malloc (sizeof (text_shaper_t));
	*shaper = (text_shaper_t) {
		.fonts = DARRAY_STATIC_INIT (8),
	};
	shaper->tab = Hash_NewTable (511, 0, 0, shaper, &shaper->hashctx);
	Hash_SetHashCompare (shaper->tab, shaper_get_hash, shaper_compare);
	return shaper;
}

void
Shaper_Delete (text_shaper_t *shaper)
{
	for (auto c = shaper->buffers; c; c = c->next) {
		shaper_free_cache_data (c);
	}
	for (auto c = shaper->unused_buffers; c; c = c->next) {
		shaper_free_cache_data (c);
	}
	PR_RESDELMAP (shaper->cache_map);
	Hash_DelTable (shaper->tab);
	Hash_DelContext (shaper->hashctx);
	for (size_t i = 0; i < shaper->fonts.size; i++) {
		hb_font_destroy (shaper->fonts.a[i].hb_font);
	}
	DARRAY_CLEAR (&shaper->fonts);
	free (shaper);
}

static void
shaper_link_cache (text_shaper_t *shaper, shaper_cache_t *cache)
{
	cache->next = shaper->buffers;
	cache->prev = &shaper->buffers;
	if (shaper->buffers) {
		shaper->buffers->prev = &cache->next;
	}
	shaper->buffers = cache;
}

static void
shaper_unlink_cache (shaper_cache_t *cache)
{
	if (cache->next) {
		cache->next->prev = cache->prev;
	}
	*cache->prev = cache->next;
}

static shaper_cache_t *
shaper_cache_new (text_shaper_t *shaper)
{
	shaper_cache_t *cache = PR_RESNEW (shaper->cache_map);
	return cache;
}

static void
shaper_cache_free (text_shaper_t *shaper, shaper_cache_t *cache)
{
	Hash_DelElement (shaper->tab, cache);
	shaper_unlink_cache (cache);
	shaper_free_cache_data (cache);
	PR_RESFREE (shaper->cache_map, cache);
}

void
Shaper_FlushUnused (text_shaper_t *shaper)
{
	shaper_cache_t *c;
	while ((c = shaper->unused_buffers)) {
		shaper_cache_free (shaper, c);
	}
	shaper->unused_buffers = shaper->buffers;
	if (shaper->buffers) {
		shaper->buffers->prev = &shaper->unused_buffers;
	}
	shaper->buffers = 0;
}

static int
shaper_font_compare (const void *a, const void *b)
{
	const uint32_t *fontid = a;
	const shaper_font_t *font = b;
	return *fontid - font->fontid;
}

static hb_font_t *
shaper_find_font (text_shaper_t *shaper, const font_t *font)
{
	shaper_font_t *f = fbsearch (&font->fontid, shaper->fonts.a,
								 shaper->fonts.size, sizeof (shaper_font_t),
								 shaper_font_compare);
	if (f && f->fontid == font->fontid) {
		return f->hb_font;
	}
	auto nf = (shaper_font_t) {
		.hb_font = hb_ft_font_create (font->face, 0),
		.fontid = font->fontid,
	};
	size_t      index = f - shaper->fonts.a;
	if (!f) {
		index = 0;
	}
	DARRAY_INSERT_AT (&shaper->fonts, nf, index);
	return nf.hb_font;
}

shaped_glyphs_t
Shaper_ShapeText (text_shaper_t *shaper, const shaping_t *control,
				  const char *text, size_t text_len)
{
	shaper_cache_t search_cache = {
		.script = *control->script,
		.features.size = control->features->size,
		.features.a = control->features->a,
		.font = control->font,
		.text = (char *) text,
		.text_len = text_len,
	};
	if (!control->features->size) {
		search_cache.features.a = 0;
	}
	shaper_cache_t *cache = Hash_FindElement (shaper->tab, &search_cache);
	if (!cache) {
		cache = shaper_cache_new (shaper);
		cache->script = *control->script;
		cache->features.size = control->features->size;
		if (control->features->size) {
			size_t feat_size = sizeof (hb_feature_t[control->features->size]);
			cache->features.a = malloc (feat_size);
			memcpy (cache->features.a, control->features->a, feat_size);
		} else {
			cache->features.a = 0;
		}
		cache->font = control->font;
		cache->text_len = text_len;
		cache->text = malloc (cache->text_len + 1);
		strncpy (cache->text, text, text_len);
		cache->text[text_len] = 0;
		cache->buffer = hb_buffer_create ();
		hb_buffer_allocation_successful (cache->buffer);

		Hash_AddElement (shaper->tab, cache);

		auto buffer = cache->buffer;
		auto hb_font = shaper_find_font (shaper, control->font);
		auto direction = cache->script.direction;
		auto script = cache->script.script;
		auto language = cache->script.language;
		auto features = &cache->features;
		hb_buffer_reset (cache->buffer);
		hb_buffer_set_direction (buffer, direction | HB_DIRECTION_LTR);
		hb_buffer_set_script (buffer, script);
		hb_buffer_set_language (buffer, language);
		hb_buffer_add_utf8 (buffer, text, cache->text_len, 0, cache->text_len);
		hb_shape (hb_font, buffer, features->a, features->size);
	} else {
		// remove from the unused buffers list if there, else move to head
		// of used buffers list (via link call below)
		shaper_unlink_cache (cache);
	}
	shaper_link_cache (shaper, cache);
	unsigned c;
	shaped_glyphs_t glyphs = {
		.glyphInfo = hb_buffer_get_glyph_infos (cache->buffer, &c),
		.glyphPos = hb_buffer_get_glyph_positions (cache->buffer, &c),
	};
	glyphs.count = c;
	return glyphs;
}
