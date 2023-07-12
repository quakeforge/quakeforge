/*
	wad.h

	wadfile tool (definitions)

	Copyright (C) 1996-1997 Id Software, Inc.
	Copyright (C) 2002 Bill Currie <bill@taniwha.org>
	Copyright (C) 2002 Jeff Teunissen <deek@quakeforge.net>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License as
	published by the Free Software Foundation; either version 2 of
	the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/

#ifndef __wad_h
#define __wad_h

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QF/qtypes.h>

typedef enum {
	mo_none,
	mo_test,
	mo_create,
	mo_extract,
} wadmode_t;

typedef struct {
	wadmode_t	mode;			// see above
	int			verbosity;		// 0=silent
	bool		compress;		// for the future
	bool		pad;			// pad area of files to 4-byte boundary
	bool		nomip;			// exclude mipmaps from output textures.
	char		*wadfile;		// wad file to read/write/test
} options_t;

extern struct tex_s *image;
extern byte default_palette[];
extern byte *lumpbuffer, *lump_p;
extern struct dstring_s *lumpname;

struct script_s;
void GrabPalette (struct script_s *script);
void GrabColormap (struct script_s *script);
void GrabPic (struct script_s *script);
void GrabMip (struct script_s *script);
void GrabRaw (struct script_s *script);
void GrabColormap2 (struct script_s *script);

void process_wad_script (const char *name);

#endif	// __wad_h
