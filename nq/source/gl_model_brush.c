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

	$Id$
*/

// models are the only shared resource between a client and server running
// on the same machine.

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "r_local.h"
#include "QF/sys.h"
#include "QF/compat.h"
#include "QF/console.h"
#include "QF/qendian.h"
#include "QF/checksum.h"
#include "glquake.h"

extern char loadname[];
extern model_t *loadmodel;
extern byte mod_novis[];
extern byte *mod_base;

int         Mod_Fullbright (byte * skin, int width, int height, char *name);

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

/*
=================
Mod_LoadLighting
=================
*/
void
Mod_LoadLighting (lump_t *l)
{
	int         i;
	byte       *in, *out;
	byte        d;
	char        litfilename[1024];

	if (!l->filelen) {
		loadmodel->lightdata = NULL;
		return;
	}

	strcpy (litfilename, loadmodel->name);
	COM_StripExtension (litfilename, litfilename);
	strcat (litfilename, ".lit");

	loadmodel->lightdata = (byte *) COM_LoadHunkFile (litfilename);
	if (!loadmodel->lightdata)			// expand the white lighting data
	{
		loadmodel->lightdata = Hunk_AllocName (l->filelen * 3, litfilename);
		in = loadmodel->lightdata + l->filelen * 2;	// place the file at the
		// end, so it will not be 
		// overwritten until the
		// very last write
		out = loadmodel->lightdata;
		memcpy (in, mod_base + l->fileofs, l->filelen);
		for (i = 0; i < l->filelen; i++) {
			d = *in++;
			*out++ = d;
			*out++ = d;
			*out++ = d;
		}
	}
}
