/*
	gl_model_sprite.c

	gl support routines for sprite model loading and caching

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
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/image.h"
#include "QF/quakefs.h"
#include "QF/va.h"
#include "QF/GL/qf_textures.h"

#include "compat.h"
#include "mod_internal.h"

static int
load_texture (model_t *mod, int framenum, const dspriteframe_t *dframe)
{
	tex_t      *targa;
	const char *name;

	targa = LoadImage (name = va (0, "%s_%i", mod->path, framenum), 1);
	if (targa) {
		if (targa->format < 4) {
			return GL_LoadTexture (name, targa->width, targa->height,
								   targa->data, true, false, 3);
		} else {
			return GL_LoadTexture (name, targa->width, targa->height,
								   targa->data, true, true, 4);
		}
	}
	return GL_LoadTexture (name, dframe->width, dframe->height,
						   (const byte *)(dframe + 1), true, true, 1);
}

void
gl_Mod_SpriteLoadFrames (mod_sprite_ctx_t *ctx)
{
	auto sprite = ctx->sprite;
	for (int i = 0; i < ctx->numframes; i++) {
		auto dframe = ctx->dframes[i];
		size_t      size = sizeof (GLuint) + sizeof (mspriteframe_t);
		mspriteframe_t *frame = Hunk_AllocName (nullptr, size, ctx->mod->name);
		ctx->frames[i]->data = (byte *) frame - (byte *) sprite;
		Mod_LoadSpriteFrame (frame, dframe);
		auto texnum = (GLuint *) &frame[1];
		*texnum = load_texture (ctx->mod, i, dframe);
	}
}
