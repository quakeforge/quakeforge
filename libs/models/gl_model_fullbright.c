/*
	gl_model_fullbright.c

	fullbright skin handling

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

#include <stdlib.h>

#include "QF/GL/qf_textures.h"
#include "QF/checksum.h"
#include "QF/qendian.h"
#include "QF/sys.h"

#include "mod_internal.h"

int
Mod_Fullbright (byte *skin, int width, int height, const char *name)
{
	byte   *ptexels;
	int		pixels;
	int		texnum = 0;

	pixels = width * height;

//	ptexels = Hunk_Alloc(s);
	ptexels = malloc (pixels);
	SYS_CHECKMEM (ptexels);

	// Check for fullbright pixels
	if (Mod_CalcFullbright (skin, ptexels, pixels)) {
		Sys_MaskPrintf (SYS_DEV, "FB Model ID: '%s'\n", name);
		texnum = GL_LoadTexture (name, width, height, ptexels, true, true, 1);
	}

	free (ptexels);
	return texnum;
}
