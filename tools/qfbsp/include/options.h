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

*/

#ifndef qfbsp_options_h
#define qfbsp_options_h

#include "QF/qtypes.h"

/**	\defgroup qfbsp_options Command-line Options Parsing
	\ingroup qfbsp
*/
///@{

typedef struct {
	int         verbosity;		// 0=silent
	qboolean    drawflag;
	qboolean    notjunc;
	qboolean    nofill;
	qboolean    noclip;
	qboolean    onlyents;
	qboolean    portal;
	qboolean    info;
	qboolean    extract;
	qboolean    extract_textures;
	qboolean    extract_entities;
	qboolean    extract_hull;
	qboolean    smart_leak;
	qboolean    usehulls;
	qboolean    watervis;
	int         hullnum;
	int         subdivide_size;
	char       *mapfile;
	char       *bspfile;
	char       *portfile;
	char       *pointfile;
	char       *hullfile;
	char       *output_file;
	const char *wadpath;
} options_t;

extern options_t options;
int DecodeArgs (int argc, char **argv);
extern const char *this_program;

///@}

#endif//qfbsp_options_h
