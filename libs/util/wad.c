/*
	wad.c

	(description)

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

#include "QF/cvar.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/wad.h"

int         wad_numlumps;
lumpinfo_t *wad_lumps;
byte       *wad_base;

/*
	W_CleanupName

	Lowercases name and pads with spaces and a terminating 0 to the length of
	lumpinfo_t->name. Used so lumpname lookups can proceed rapidly by
	comparing 4 chars at a time Space padding is so names can be printed
	nicely in tables. Can safely be performed in place.
*/
static void
W_CleanupName (const char *in, char *out)
{
	int			i;
	int			c;

	for (i = 0; i < 16; i++) {
		c = in[i];
		if (!c)
			break;

		if (c >= 'A' && c <= 'Z')
			c += ('a' - 'A');
		out[i] = c;
	}

	for (; i < 16; i++)
		out[i] = 0;
}

VISIBLE void
W_LoadWadFile (const char *filename)
{
	lumpinfo_t *lump_p;
	wadinfo_t  *header;
	int         i;
	int			infotableofs;

	wad_base = QFS_LoadHunkFile (QFS_FOpenFile (filename));
	if (!wad_base)
		Sys_Error ("W_LoadWadFile: unable to load %s", filename);

	header = (wadinfo_t *) wad_base;

	if (header->id[0] != 'W' || header->id[1] != 'A' || header->id[2] != 'D'
		|| header->id[3] != '2')
		Sys_Error ("Wad file %s doesn't have WAD2 id", filename);

	wad_numlumps = LittleLong (header->numlumps);
	infotableofs = LittleLong (header->infotableofs);
	wad_lumps = (lumpinfo_t *) (wad_base + infotableofs);

	for (i = 0, lump_p = wad_lumps; i < wad_numlumps; i++, lump_p++) {
		lump_p->filepos = LittleLong (lump_p->filepos);
		lump_p->size = LittleLong (lump_p->size);
		W_CleanupName (lump_p->name, lump_p->name);
		if (lump_p->type == TYP_QPIC)
			SwapPic ((qpic_t *) (wad_base + lump_p->filepos));
	}
}

static lumpinfo_t *
W_GetLumpinfo (const char *name)
{
	int         i;
	lumpinfo_t *lump_p;
	char        clean[16];

	W_CleanupName (name, clean);

	for (lump_p = wad_lumps, i = 0; i < wad_numlumps; i++, lump_p++) {
		if (!strcmp (clean, lump_p->name))
			return lump_p;
	}

	Sys_MaskPrintf (SYS_warn, "W_GetLumpinfo: %s not found", name);
	return NULL;
}

VISIBLE void *
W_GetLumpName (const char *name)
{
	lumpinfo_t *lump;

	lump = W_GetLumpinfo (name);

	return (void *) (wad_base + lump->filepos);
}

VISIBLE void
SwapPic (qpic_t *pic)
{
	pic->width = LittleLong (pic->width);
	pic->height = LittleLong (pic->height);
}
