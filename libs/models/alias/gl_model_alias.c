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

#include "compat.h"

static void
gl_alias_clear (model_t *m, void *data)
{
	m->needload = true;

	Cache_Free (&m->cache);
}

static void
gl_Mod_LoadSkin (mod_alias_ctx_t *alias_ctx, byte *texels,
				 int snum, int gnum, maliasskindesc_t *skindesc)
{
	aliashdr_t *header = alias_ctx->header;
	char	modname[MAX_QPATH + 4];
	int		fb_texnum = 0, texnum = 0;
	dstring_t  *name = dstring_new ();

	Mod_FloodFillSkin (texels, header->mdl.skinwidth, header->mdl.skinheight);
	// save 8 bit texels for the player model to remap
	// FIXME remove model restriction
	if (strequal (alias_ctx->mod->path, "progs/player.mdl"))
		gl_Skin_SetPlayerSkin (header->mdl.skinwidth, header->mdl.skinheight,
							   texels);

	QFS_StripExtension (alias_ctx->mod->path, modname);

	if (!alias_ctx->mod->fullbright) {
		if (gnum != -1) {
			dsprintf (name, "fb_%s_%i_%i", modname, snum, gnum);
		} else {
			dsprintf (name, "fb_%s_%i", modname, snum);
		}
		fb_texnum = Mod_Fullbright (texels, header->mdl.skinwidth,
									header->mdl.skinheight, name->str);
		Sys_MaskPrintf (SYS_glt, "%s %d\n", name->str, fb_texnum);
	}
	if (gnum != -1) {
		dsprintf (name, "%s_%i_%i", modname, snum, gnum);
	} else {
		dsprintf (name, "%s_%i", modname, snum);
	}
	texnum = GL_LoadTexture (name->str, header->mdl.skinwidth,
							 header->mdl.skinheight, texels, true, false, 1);
	Sys_MaskPrintf (SYS_glt, "%s %d\n", name->str, texnum);
	skindesc->texnum = texnum;
	skindesc->fb_texnum = fb_texnum;
	alias_ctx->mod->hasfullbrights = fb_texnum;
	dstring_delete (name);
	// alpha param was true for non group skins
}

void
gl_Mod_LoadAllSkins (mod_alias_ctx_t *alias_ctx)
{
	aliashdr_t *header = alias_ctx->header;
	int         skinsize = header->mdl.skinwidth * header->mdl.skinheight;
	int         num_skins = alias_ctx->skins.size;
	byte       *texel_block = Hunk_AllocName (0, skinsize * num_skins,
											  alias_ctx->mod->name);

	for (int i = 0; i < num_skins; i++) {
		__auto_type skin = alias_ctx->skins.a + i;
		byte      *texels = texel_block + i * skinsize;

		skin->skindesc->skin = texels - (byte *) header;
		memcpy (texels, skin->texels, skinsize);
		gl_Mod_LoadSkin (alias_ctx, texels, skin->skin_num, skin->group_num,
						 skin->skindesc);
	}
}

void
gl_Mod_FinalizeAliasModel (mod_alias_ctx_t *alias_ctx)
{
	aliashdr_t *header = alias_ctx->header;

	if (strequal (alias_ctx->mod->path, "progs/eyes.mdl")) {
		header->mdl.scale_origin[2] -= (22 + 8);
		VectorScale (header->mdl.scale, 2, header->mdl.scale);
	}

	alias_ctx->mod->clear = gl_alias_clear;
}

static void
Mod_LoadExternalSkin (maliasskindesc_t *pskindesc, char *filename)
{
	tex_t		*tex, *glow;
	char		*ptr;

	ptr = strrchr (filename, '/');
	if (!ptr)
		ptr = filename;

	tex = LoadImage (filename, 1);
	if (!tex)
		tex = LoadImage (va (0, "textures/%s", ptr + 1), 1);
	if (tex) {
		pskindesc->texnum = GL_LoadTexture (filename, tex->width, tex->height,
											tex->data, true, false,
											tex->format > 2 ? tex->format : 1);

		pskindesc->fb_texnum = 0;

		glow = LoadImage (va (0, "%s_luma", filename), 1);
		if (!glow)
			glow = LoadImage (va (0, "%s_glow", filename), 1);
		if (!glow)
			glow = LoadImage (va (0, "textures/%s_luma", ptr + 1), 1);
		if (!glow)
			glow = LoadImage (va (0, "textures/%s_glow", ptr + 1), 1);
		if (glow)
			pskindesc->fb_texnum =
				GL_LoadTexture (va (0, "fb_%s", filename), glow->width,
								glow->height, glow->data, true, true,
								glow->format > 2 ? glow->format : 1);
		else if (tex->format < 3)
			pskindesc->fb_texnum = Mod_Fullbright (tex->data, tex->width,
												   tex->height,
												   va (0, "fb_%s", filename));
	}
}

void
gl_Mod_LoadExternalSkins (mod_alias_ctx_t *alias_ctx)
{
	aliashdr_t *header = alias_ctx->header;
	char			   modname[MAX_QPATH + 4];
	int				   i, j;
	maliasskindesc_t  *pskindesc;
	maliasskingroup_t *pskingroup;
	dstring_t  *filename = dstring_new ();

	QFS_StripExtension (alias_ctx->mod->path, modname);

	for (i = 0; i < header->mdl.numskins; i++) {
		pskindesc = ((maliasskindesc_t *)
					 ((byte *) header + header->skindesc)) + i;
		if (pskindesc->type == ALIAS_SKIN_SINGLE) {
			dsprintf (filename, "%s_%i", modname, i);
			Mod_LoadExternalSkin (pskindesc, filename->str);
		} else {
			pskingroup = (maliasskingroup_t *)
				((byte *) header + pskindesc->skin);

			for (j = 0; j < pskingroup->numskins; j++) {
				dsprintf (filename, "%s_%i_%i", modname, i, j);
				Mod_LoadExternalSkin (pskingroup->skindescs + j, filename->str);
			}
		}
	}
}
