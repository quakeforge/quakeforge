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
#include "QF/GLSL/types.h"
#include "QF/GLSL/qf_textures.h"

#include "compat.h"
#include "mod_internal.h"

static void
glsl_sprite_clear (model_t *m, void *data)
{
	msprite_t  *sprite = (msprite_t *) m->cache.data;
	m->cache.data = nullptr;

	m->needload = true;

	auto skin = &sprite->skin;
	auto skindesc = (framedesc_t *) ((byte *) sprite + skin->descriptors);
	auto skinframe = (frame_t *) ((byte *) sprite + skin->frames);
	int index = 0;

	for (int i = 0; i < skin->numdesc; i++) {
		for (int j = 0; j < skindesc[i].numframes; j++) {
			uint32_t data = skinframe[index++].data;
			auto frame = (mspriteframe_t *) ((byte *) sprite + data);
			auto texnum = (GLuint *) &frame[1];
			GLSL_ReleaseTexture (*texnum);
		}
	}
}

void
glsl_Mod_SpriteLoadFrames (mod_sprite_ctx_t *ctx)
{
	ctx->mod->clear = glsl_sprite_clear;
	auto sprite = ctx->sprite;

	for (int i = 0; i < ctx->numframes; i++) {
		auto dframe = ctx->dframes[i];
		size_t      size = sizeof (GLuint) + sizeof (mspriteframe_t);
		mspriteframe_t *frame = Hunk_AllocName (nullptr, size, ctx->mod->name);
		ctx->frames[i]->data = (byte *) frame - (byte *) sprite;
		Mod_LoadSpriteFrame (frame, dframe);
		const char *name = va (0, "%s_%i", ctx->mod->path, i);
		auto texnum = (GLuint *) &frame[1];
		*texnum = GLSL_LoadQuakeTexture (name, dframe->width, dframe->height,
									     (const byte *)(dframe + 1));
	}
}
