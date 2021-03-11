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

*/

#ifndef __QF_locs_h
#define __QF_locs_h

#include "QF/qtypes.h"

typedef struct
{
	vec3_t	loc;
	char	*name;
} location_t;

location_t *locs_find(const vec3_t target) __attribute__((pure));
void locs_add (const vec3_t location, const char *name);
void locs_del (const vec3_t loc);
void locs_edit (const vec3_t loc, const char *desc);
void locs_load(const char *filename);
void locs_mark (const vec3_t loc, const char *desc);
int  locs_nearest (const vec3_t loc) __attribute__((pure));
void locs_reset (void);
void locs_save (const char *filename, qboolean gz);
void map_to_loc (const char *mapname, char *filename);
void locs_draw (vec3_t simorg);

#endif//__QF_locs_h
