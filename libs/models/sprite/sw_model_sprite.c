/*
	sw_model_sprite.c

	sw renderer support routines for sprite model loading and caching

	Copyright (C) 1996-1997  Id Software, Inc.

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif

#include "QF/zone.h"

#include "mod_internal.h"

void
sw_Mod_SpriteLoadFrames (mod_sprite_ctx_t *ctx)
{
	auto sprite = ctx->sprite;
	for (int i = 0; i < ctx->numframes; i++) {
		auto        dframe = ctx->dframes[i];
		size_t      pixels = dframe->width * dframe->height;
		size_t      size = field_offset (qpic_t, data[pixels])
							+ sizeof (mspriteframe_t);
		mspriteframe_t *frame = Hunk_AllocName (0, size, ctx->mod->name);
		ctx->frames[i]->data = (byte *) frame - (byte *) sprite;
		Mod_LoadSpriteFrame (frame, dframe);
		auto pic = (qpic_t *) &frame[1];
		*pic = (qpic_t) {
			.width = dframe->width,
			.height = dframe->height,
		};
		memcpy (pic->data, dframe + 1, pixels);
	}
}
