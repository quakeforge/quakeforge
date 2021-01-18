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

void *
gl_Mod_LoadSkin (byte *skin, int skinsize, int snum, int gnum, qboolean group,
				 maliasskindesc_t *skindesc)
{
	byte   *pskin;
	char	modname[MAX_QPATH + 4];
	int		fb_texnum = 0, texnum = 0;
	dstring_t  *name = dstring_new ();

	pskin = Hunk_AllocName (skinsize, loadname);
	skindesc->skin = (byte *) pskin - (byte *) pheader;

	memcpy (pskin, skin, skinsize);

	Mod_FloodFillSkin (pskin, pheader->mdl.skinwidth, pheader->mdl.skinheight);
	// save 8 bit texels for the player model to remap
	// FIXME remove model restriction
	if (strequal (loadmodel->name, "progs/player.mdl"))
		gl_Skin_SetPlayerSkin (pheader->mdl.skinwidth, pheader->mdl.skinheight,
							   pskin);

	QFS_StripExtension (loadmodel->name, modname);

	if (!loadmodel->fullbright) {
		if (group) {
			dsprintf (name, "fb_%s_%i_%i", modname, snum, gnum);
		} else {
			dsprintf (name, "fb_%s_%i", modname, snum);
		}
		fb_texnum = Mod_Fullbright (pskin, pheader->mdl.skinwidth,
									pheader->mdl.skinheight, name->str);
		Sys_MaskPrintf (SYS_GLT, "%s %d\n", name->str, fb_texnum);
	}
	if (group) {
		dsprintf (name, "%s_%i_%i", modname, snum, gnum);
	} else {
		dsprintf (name, "%s_%i", modname, snum);
	}
	texnum = GL_LoadTexture (name->str, pheader->mdl.skinwidth,
							 pheader->mdl.skinheight, pskin, true, false, 1);
	Sys_MaskPrintf (SYS_GLT, "%s %d\n", name->str, texnum);
	skindesc->texnum = texnum;
	skindesc->fb_texnum = fb_texnum;
	loadmodel->hasfullbrights = fb_texnum;
	dstring_delete (name);
	// alpha param was true for non group skins
	return skin + skinsize;
}

void
gl_Mod_FinalizeAliasModel (model_t *m, aliashdr_t *hdr)
{
	if (strequal (m->name, "progs/eyes.mdl")) {
		hdr->mdl.scale_origin[2] -= (22 + 8);
		VectorScale (hdr->mdl.scale, 2, hdr->mdl.scale);
	}
}

static void
Mod_LoadExternalSkin (maliasskindesc_t *pskindesc, char *filename)
{
	tex_t		*tex, *glow;
	char		*ptr;

	ptr = strrchr (filename, '/');
	if (!ptr)
		ptr = filename;

	tex = LoadImage (filename);
	if (!tex)
		tex = LoadImage (va ("textures/%s", ptr + 1));
	if (tex) {
		pskindesc->texnum = GL_LoadTexture (filename, tex->width, tex->height,
											tex->data, true, false,
											tex->format > 2 ? tex->format : 1);

		pskindesc->fb_texnum = 0;

		glow = LoadImage (va ("%s_luma", filename));
		if (!glow)
			glow = LoadImage (va ("%s_glow", filename));
		if (!glow)
			glow = LoadImage (va ("textures/%s_luma", ptr + 1));
		if (!glow)
			glow = LoadImage (va ("textures/%s_glow", ptr + 1));
		if (glow)
			pskindesc->fb_texnum =
				GL_LoadTexture (va ("fb_%s", filename), glow->width,
								glow->height, glow->data, true, true,
								glow->format > 2 ? glow->format : 1);
		else if (tex->format < 3)
			pskindesc->fb_texnum = Mod_Fullbright (tex->data, tex->width,
												   tex->height,
												   va ("fb_%s", filename));
	}
}

void
gl_Mod_LoadExternalSkins (model_t *mod)
{
	char			   modname[MAX_QPATH + 4];
	int				   i, j;
	maliasskindesc_t  *pskindesc;
	maliasskingroup_t *pskingroup;
	dstring_t  *filename = dstring_new ();

	QFS_StripExtension (mod->name, modname);

	for (i = 0; i < pheader->mdl.numskins; i++) {
		pskindesc = ((maliasskindesc_t *)
					 ((byte *) pheader + pheader->skindesc)) + i;
		if (pskindesc->type == ALIAS_SKIN_SINGLE) {
			dsprintf (filename, "%s_%i", modname, i);
			Mod_LoadExternalSkin (pskindesc, filename->str);
		} else {
			pskingroup = (maliasskingroup_t *)
				((byte *) pheader + pskindesc->skin);

			for (j = 0; j < pskingroup->numskins; j++) {
				dsprintf (filename, "%s_%i_%i", modname, i, j);
				Mod_LoadExternalSkin (pskingroup->skindescs + j, filename->str);
			}
		}
	}
}
