/*
	glyphcache.c

	Cache management for glyphs

	Copyright (C) 2026 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2026/08/20

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

#include "QF/image.h"

#include "QF/ui/canvas.h"
#include "QF/ui/font.h"
#include "QF/ui/glyphcache.h"
#include "QF/ui/text.h"

#include "QF/plugin/vid_render.h"

#define GLYPH_CACHE_SIZE 4096
#define GLYPH_SCRAP_SIZE 2048

glyphcache_t *
glyphcache_new (font_t **font_list, uint32_t num_fonts)
{
	qfZoneScoped (true);
	size_t size = sizeof (glyphcache_t)
				+ sizeof (glyphkey_t[GLYPH_CACHE_SIZE])
				+ sizeof (glyphkey_t[GLYPH_CACHE_SIZE])
				+ sizeof (uint32_t[GLYPH_CACHE_SIZE])
				+ sizeof (rscrap_t)
				+ sizeof (font_t *[num_fonts]);
	glyphcache_t *cache = malloc (size);
	auto map = (glyphkey_t *) &cache[1];
	auto objs = (glyphkey_t *) &map[GLYPH_CACHE_SIZE];
	auto rects = (uint32_t *) &objs[GLYPH_CACHE_SIZE];
	auto scrap = (rscrap_t *) &rects[GLYPH_CACHE_SIZE];
	auto fonts = (font_t **) &scrap[1];
	tex_t tex = {
		.width = 2048,
		.height = 2048,
		.format = tex_a,
	};
	uint32_t atlas_id
		= r_funcs->draw.make_atlas ("glyphs", &tex, GLYPH_CACHE_SIZE, nullptr);
	*cache = (glyphcache_t) {
		.map = map,
		.objs = objs,
		.rects = rects,
		.scrap = scrap,
		.fonts = fonts,
		.num_fonts = num_fonts,
		.atlas_id = atlas_id,
	};
	memset (map, 0, sizeof (glyphkey_t[GLYPH_CACHE_SIZE]));
	memset (objs, 0, sizeof (glyphkey_t[GLYPH_CACHE_SIZE]));
	memset (rects, ~0u, sizeof (uint32_t[GLYPH_CACHE_SIZE]));
	memcpy (fonts, font_list, sizeof (font_t *[num_fonts]));
	R_ScrapInit (scrap, GLYPH_SCRAP_SIZE, GLYPH_SCRAP_SIZE);
	return cache;
}

/*
	64-bit Integer Bit Mixer (Avalanche Mixer)

	Algorithm: Stafford's Mix1 variant of the SplitMix64 finalizer.
	Attribution: Based on Austin Appleby's MurmurHash3 mixer structure,
	             with optimal constants discovered via simulated annealing
	             by David Stafford (2011).

	This function provides full 64-bit bit dispersion (bijective mapping)
	ensuring sequential properties (like sequential font glyph IDs) are
	distributed perfectly across a linear-probing hash map.
*/
static uint64_t
hash_uint64 (uint64_t x)
{
	x ^= x >> 30;
	x *= UINT64_C (0xbf58476d1ce4e5b9);	// Stafford Mix1 Constant
	x ^= x >> 27;
	return x;
}

static int __attribute__((pure))
glyphcache_lookup (glyphcache_t *cache, glyphkey_t key)
{
	qfZoneScoped (true);
	int index = hash_uint64 (key.raw_key) & (GLYPH_CACHE_SIZE - 1);
	int start = index;
	while (cache->map[index].raw_key != key.raw_key) {
		if (!cache->map[index].raw_key) {
			return -1;
		}
		index = (index + 1) & (GLYPH_CACHE_SIZE - 1);
		if (index == start) {
			return -1;
		}
	}
	return index;
}

static int
glyphcache_add (glyphcache_t *cache, glyphkey_t key)
{
	qfZoneScoped (true);
	int index = hash_uint64 (key.raw_key) & (GLYPH_CACHE_SIZE - 1);
	int start = index;
	while (cache->map[index].raw_key != key.raw_key) {
		if (!cache->map[index].raw_key) {
			cache->map[index] = key;
			return index;
		}
		index = (index + 1) & (GLYPH_CACHE_SIZE - 1);
		if (index == start) {
			return -1;
		}
	}
	return index;
}

void
glyphcache_load_glyphs (canvas_system_t *canvas_sys)
{
	qfZoneScoped (true);
	static int glyph_comps[] = {
		canvas_passage_glyphs,
		canvas_glyphs
	};
	auto reg = canvas_sys->reg;
	auto cache = canvas_sys->glyphcache;
	glyphkey_t new_keys[GLYPH_CACHE_SIZE] = {};
	int new_inds[GLYPH_CACHE_SIZE] = {};
	int num_keys = 0;
	for (uint32_t i = 0; i < countof (glyph_comps); i++) {
		auto pool = &reg->comp_pools[canvas_sys->base + glyph_comps[i]];
		for (uint32_t j = 0; j < pool->count; j++) {
			auto        glyphset = &((glyphset_t *) pool->data)[j];
			for (uint32_t k = 0; k < glyphset->count; k++) {
				auto gobj = &glyphset->glyphs[k];
				int index = glyphcache_lookup (cache, gobj->key);
				if (index < 0) {
					index = glyphcache_add (cache, gobj->key);
					new_keys[num_keys] = gobj->key;
					new_inds[num_keys] = index;
					gobj->cacheind = index;
					num_keys++;
				} else {
					gobj->cacheind = index;
				}
			}
		}
	}
	if (!num_keys) {
		return;
	}

	//Font_LoadGlyphs (new_keys, num_keys);
	for (int i = 0; i < num_keys; i++) {
		cache->objs[new_inds[i]] = new_keys[i];
	}
}
