/*
	options.h

	command line options handling

	Copyright (C) 2002 Bill Currie

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

#ifndef __options_h
#define __options_h

#include "QF/qtypes.h"

typedef struct {
	int			verbosity;		// 0=silent
	qboolean	drawflag;
	qboolean	notjunc;
	qboolean	nofill;
	qboolean	noclip;
	qboolean	onlyents;
	qboolean	portal;
	qboolean    extract;
	qboolean    extract_textures;
	qboolean    extract_entities;
	qboolean	usehulls;
	qboolean	watervis;
	int			hullnum;
	int			subdivide_size;
	char		*mapfile;
	char		*bspfile;
	char		*portfile;
	char		*pointfile;
	char		*hullfile;
	const char	*wadpath;
} options_t;

extern options_t options;
int DecodeArgs (int argc, char **argv);
void usage (int status);
extern const char *this_program;
#endif//__options_h
