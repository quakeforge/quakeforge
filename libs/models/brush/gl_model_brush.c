/*
	gl_model_brush.c

	model loading and caching

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
static const char rcsid[] = 
	"$Id$";

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

#include "QF/model.h"
#include "QF/qendian.h"
#include "QF/sys.h"
#include "QF/texture.h"
#include "QF/tga.h"
#include "QF/vfs.h"
#include "QF/GL/qf_textures.h"

#include "compat.h"

const int   mod_lightmap_bytes = 3;

void
Mod_ProcessTexture (miptex_t *mt, texture_t *tx)
{
	char        name[32];

	snprintf (name, sizeof (name), "fb_%s", mt->name);
	tx->gl_fb_texturenum =
		Mod_Fullbright ((byte *) (tx + 1), tx->width, tx->height, name);
	tx->gl_texturenum =
		GL_LoadTexture (mt->name, tx->width, tx->height, (byte *) (tx + 1),
						true, false, 1);
}

void
Mod_LoadExternalTextures (model_t *mod)
{
	texture_t	*tx;
	char		filename[MAX_QPATH + 8];
	VFile		*f;
	tex_t		*targa;
	int			i, length;

	for (i = 0; i < mod->numtextures; i++)
	{
		tx = mod->textures[i];
		length = strlen (tx->name) - 1;

		// backslash at the end of texture name indicates external texture
		if (tx->name[length] != '\\')
			continue;

		// replace special flag characters with underscores
		if (tx->name[0] == '+' || tx->name[0] == '*')
		 	snprintf (filename, sizeof (filename), "maps/_%s", tx->name + 1);
		else
		 	snprintf (filename, sizeof (filename), "maps/%s", tx->name);

		length += 5; // add "maps/" to the string length calculation
		snprintf (filename + length, sizeof (filename) - length, ".tga");

		COM_FOpenFile (filename, &f);
		if (f) {
			targa = LoadTGA (f);
			Qclose (f);
			if (targa->format < 4)
				tx->gl_texturenum = GL_LoadTexture (tx->name, targa->width,
					targa->height, targa->data, true, false, 3);
			else
				tx->gl_texturenum = GL_LoadTexture (tx->name, targa->width,
					targa->height, targa->data, true, true, 4);
		}
	}
}

void
Mod_LoadLighting (lump_t *l)
{
	int         i;
	byte       *in, *out, *data;
	byte        d;
	char        litfilename[1024];

	loadmodel->lightdata = NULL;
	// LordHavoc: check for a .lit file to load
	strcpy (litfilename, loadmodel->name);
	COM_StripExtension (litfilename, litfilename);
	strncat (litfilename, ".lit", sizeof (litfilename) - strlen (litfilename));
	data = (byte *) COM_LoadHunkFile (litfilename);
	if (data) {
		if (data[0] == 'Q' && data[1] == 'L' && data[2] == 'I'
			&& data[3] == 'T') {
			i = LittleLong (((int *) data)[1]);
			if (i == 1) {
				Sys_DPrintf ("%s loaded", litfilename);
				loadmodel->lightdata = data + 8;
				return;
			} else
				Sys_Printf ("Unknown .lit file version (%d)\n", i);
		} else
			Sys_Printf ("Corrupt .lit file (old version?), ignoring\n");
	}
	// LordHavoc: oh well, expand the white lighting data
	if (!l->filelen)
		return;
	loadmodel->lightdata = Hunk_AllocName (l->filelen * 3, litfilename);
	in = loadmodel->lightdata + l->filelen * 2;	// place the file at the end, 
												// so it will not be
												// overwritten until the very 
												// last write
	out = loadmodel->lightdata;
	memcpy (in, mod_base + l->fileofs, l->filelen);
	for (i = 0; i < l->filelen; i++) {
		d = *in++;
		*out++ = d;
		*out++ = d;
		*out++ = d;
	}
}
