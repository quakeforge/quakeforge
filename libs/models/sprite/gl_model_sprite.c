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

void
gl_Mod_SpriteLoadTexture (mspriteframe_t *pspriteframe, int framenum)
{
	tex_t      *targa;
	const char *name;

	targa = LoadImage (name = va ("%s_%i", loadmodel->name, framenum), 1);
	if (targa) {
		if (targa->format < 4)
			pspriteframe->gl_texturenum = GL_LoadTexture (name,
				targa->width, targa->height, targa->data,
				true, false, 3);
		else
			pspriteframe->gl_texturenum = GL_LoadTexture (name,
				targa->width, targa->height, targa->data,
				true, true, 4);
		return;
	}
	pspriteframe->gl_texturenum =
		GL_LoadTexture (name, pspriteframe->width, pspriteframe->height,
						pspriteframe->pixels, true, true, 1);
}
