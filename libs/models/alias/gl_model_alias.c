/*
	gl_model_alias.c

	alias model loading and caching for gl

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
// models are the only shared resource between a client and server running
// on the same machine.

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/skin.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/GL/qf_textures.h"

#include "mod_internal.h"
#include "r_internal.h"

#include "compat.h"

static void
gl_alias_clear (model_t *m, void *data)
{
	m->needload = true;

	Cache_Free (&m->cache);
}

void
gl_Mod_LoadAllSkins (mod_alias_ctx_t *alias_ctx)
{
	auto mesh = alias_ctx->mesh;
	int         skinsize = alias_ctx->skinwidth * alias_ctx->skinheight;
	uint32_t    num_skins = alias_ctx->skins.size;
	tex_t      *tex_block = Hunk_AllocName (nullptr, sizeof (tex_t[num_skins]),
											alias_ctx->mod->name);
	byte       *texel_block = Hunk_AllocName (nullptr, skinsize * num_skins,
											  alias_ctx->mod->name);
	auto skindesc = (keyframedesc_t *) ((byte *) mesh + mesh->skin.descriptors);
	auto skinframe = (keyframe_t *) ((byte *) mesh + mesh->skin.keyframes);

	int index = 0;
	for (uint32_t i = 0; i < num_skins; i++) {
		for (uint32_t j = 0; j < skindesc[i].numframes; j++) {
			auto skin = alias_ctx->skins.a + index;
			auto skintex = &tex_block[index];
			byte *texels = texel_block + index * skinsize;
			skinframe[index].data = (byte *)skintex - (byte *) mesh;
			*skintex = (tex_t) {
				.width = alias_ctx->skinwidth,
				.height = alias_ctx->skinheight,
				.format = tex_palette,
				.relative = true,
				.palette = vid.palette,
				.data = (byte *) (texels - (byte *) mesh),
			};
			memcpy (texels, skin->texels, skinsize);
			index++;
		}
	}
}

void
gl_Mod_FinalizeAliasModel (mod_alias_ctx_t *alias_ctx)
{
	//mesh_t     *mesh = alias_ctx->mesh;

	//if (strequal (alias_ctx->mod->path, "progs/eyes.mdl")) {
	//	mesh->mdl.scale_origin[2] -= (22 + 8);
	//	VectorScale (mesh->mdl.scale, 2, mesh->mdl.scale);
	//}

	alias_ctx->mod->clear = gl_alias_clear;
}

static void
Mod_LoadExternalSkin (glskin_t *skin, char *filename)
{
	tex_t		*tex, *glow;
	char		*ptr;

	ptr = strrchr (filename, '/');
	if (!ptr)
		ptr = filename;

	tex = LoadImage (filename, 1);
	if (!tex)
		tex = LoadImage (va ("textures/%s", ptr + 1), 1);
	if (tex) {
		skin->id = GL_LoadTexture (filename, tex->width, tex->height,
								   tex->data, true, false,
								   tex->format > 2 ? tex->format : 1);

		skin->fb = 0;

		glow = LoadImage (va ("%s_luma", filename), 1);
		if (!glow)
			glow = LoadImage (va ("%s_glow", filename), 1);
		if (!glow)
			glow = LoadImage (va ("textures/%s_luma", ptr + 1), 1);
		if (!glow)
			glow = LoadImage (va ("textures/%s_glow", ptr + 1), 1);
		if (glow)
			skin->fb =
				GL_LoadTexture (va ("fb_%s", filename), glow->width,
								glow->height, glow->data, true, true,
								glow->format > 2 ? glow->format : 1);
		else if (tex->format < 3)
			skin->fb = Mod_Fullbright (tex->data, tex->width, tex->height,
									   va ("fb_%s", filename));
	}
}

void
gl_Mod_LoadExternalSkins (mod_alias_ctx_t *alias_ctx)
{
	return;	//FIXME external skin support need a bit of thought
	auto mesh = alias_ctx->mesh;
	auto skin = &mesh->skin;
	int         num_skins = alias_ctx->skins.size;
	auto skindesc = (keyframedesc_t *) ((byte *) mesh + skin->descriptors);
	auto skinframe = (keyframe_t *) ((byte *) mesh + skin->keyframes);
	dstring_t  *filename = dstring_new ();

	char modname[strlen (alias_ctx->mod->path) + 1];
	QFS_StripExtension (alias_ctx->mod->path, modname);

	glskin_t   *skins = Hunk_AllocName (nullptr, sizeof (glskin_t[num_skins]),
										alias_ctx->mod->name);
	uint32_t    index = 0;

	for (uint32_t i = 0; i < alias_ctx->numskins; i++) {
		for (uint32_t j = 0; j < skindesc[i].numframes; j++) {
			if (skindesc[i].numframes == 1) {
				dsprintf (filename, "%s_%i", modname, i);
			} else {
				dsprintf (filename, "%s_%i_%i", modname, i, j);
			}
			Mod_LoadExternalSkin (&skins[index], filename->str);
			skinframe[index].data = (byte *) &skins[index] - (byte *) mesh;
		}
	}
}
