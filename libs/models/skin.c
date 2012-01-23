/*
	skin.c

	Skin support

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/1/23

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

static __attribute__ ((used)) const char rcsid[] = "$Id$";

#ifdef HAVE_STRING_H
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include <stdlib.h>

#include "QF/image.h"
#include "QF/model.h"
#include "QF/skin.h"
#include "QF/sys.h"

#include "QF/GLSL/funcs.h"

// if more than 32 clients are to be supported, then this will need to be
// updated
#define MAX_TRANSLATIONS 32
static byte translations[MAX_TRANSLATIONS][VID_GRADES * 256];

static skin_t *
new_skin (void)
{
	return calloc (1, sizeof (skin_t));
}

VISIBLE void
Skin_SetTranslation (int cmap, int top, int bottom)
{
#if 0
	int         i, j;
	byte       *source;

	top = bound (0, top, 13) * 16;
	bottom = bound (0, bottom, 13) * 16;

	source = vid.colormap8;

	for (i = 0; i < VID_GRADES; i++, source += 256) {
		if (top < 128)	// the artists made some backwards ranges.
			memcpy (trans->top[i], source + top, 16);
		else
			for (j = 0; j < 16; j++)
				trans->top[i][j] = source[top + 15 - j];

		if (bottom < 128)
			memcpy (trans->bottom[i], source + bottom, 16);
		else
			for (j = 0; j < 16; j++)
				trans->bottom[i][j] = source[bottom + 15 - j];
	}
#endif
}

skin_t *
Skin_SetColormap (skin_t *skin, int cmap)
{
	if (!skin)
		skin = new_skin ();
	skin->colormap = 0;
	if (cmap < 0 || cmap > MAX_TRANSLATIONS)
		Sys_MaskPrintf (SYS_SKIN, "invalid skin slot: %d\n", cmap);
	if (cmap > 0 && cmap <= MAX_TRANSLATIONS)
		skin->colormap = translations[cmap - 1];
	return skin;
}
