/*
	glsl_model_sprite.c

	Sprite model mesh processing for GLSL

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/12/23

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
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/image.h"
#include "QF/quakefs.h"
#include "QF/va.h"
#include "QF/GLSL/qf_textures.h"

#include "compat.h"
#include "mod_internal.h"

static void
glsl_sprite_clear (model_t *m, void *data)
{
	int         i, j;
	msprite_t  *sprite = (msprite_t *) m->cache.data;
	mspritegroup_t *group;
	mspriteframe_t *frame;

	m->needload = true;
	m->cache.data = 0;
	for (i = 0; i < sprite->numframes; i++) {
		if (sprite->frames[i].type == SPR_SINGLE) {
			frame = sprite->frames[i].frame;
			GLSL_ReleaseTexture (frame->gl_texturenum);
		} else {
			group = sprite->frames[i].group;
			for (j = 0; j < group->numframes; j++) {
				frame = group->frames[j];
				GLSL_ReleaseTexture (frame->gl_texturenum);
			}
		}
	}
}

void
glsl_Mod_SpriteLoadFrames (mod_sprite_ctx_t *ctx)
{
	ctx->mod->clear = glsl_sprite_clear;
	for (int i = 0; i < ctx->numframes; i++) {
		__auto_type dframe = ctx->dframes[i];
		size_t      size = sizeof (mspriteframe_t);
		mspriteframe_t *frame = Hunk_AllocName (0, size, ctx->mod->name);
		*ctx->frames[i] = frame;
		Mod_LoadSpriteFrame (frame, dframe);
		const char *name = va (0, "%s_%i", ctx->mod->path,
							   ctx->frame_numbers[i]);
		frame->gl_texturenum =
			GLSL_LoadQuakeTexture (name, dframe->width, dframe->height,
								   (const byte *)(dframe + 1));
	}
}
