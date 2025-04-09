/*
	pak.h

	Pakfile tool (definitions)

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

#ifndef __pak_h
#define __pak_h

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QF/qtypes.h>

typedef enum {
	mo_none,
	mo_test,
	mo_create,
	mo_extract,
} pakmode_t;

typedef struct {
	pakmode_t	mode;			// see above
	int			verbosity;		// 0=silent
	bool		compress;		// for the future
	bool		pad;			// pad area of files to 4-byte boundary
	char		*packfile;		// pak file to read/write/test
} options_t;

#endif	// __pak_h
