/*
	locs.h

	Parsing and handling of location files.

	Copyright (C) 2000       Anton Gavrilov (tonik@quake.ru)

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

#ifndef __locs_h
#define __locs_h

#include "QF/qtypes.h"

typedef struct
{
	vec3_t	loc;
	char	*name;
} location_t;

location_t *locs_find(vec3_t target);
void locs_load(char *filename);
void locs_reset();
void locs_add(vec3_t location, char *name);
void map_to_loc (char *mapname, char *filename);
void locs_del (vec3_t loc);
void locs_edit (vec3_t loc, char *desc);
void locs_mark (vec3_t loc, char *desc);
void locs_save (char *filename, qboolean gz);
int locs_nearest (vec3_t loc);
#endif // __locs_h
