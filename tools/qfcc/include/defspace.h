/*
	defspace.h

	management of data segments

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/01/16

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

#ifndef __defspace_h
#define __defspace_h

#include "QF/pr_comp.h"
#include "QF/pr_debug.h"

typedef struct defspace_s {
	struct defspace_s *next;
	struct locref_s *free_locs;
	struct def_s *defs;
	struct def_s **def_tail;
	pr_type_t  *data;
	int         size;
	int         max_size;
	int       (*grow) (struct defspace_s *space);
	int         qfo_space;
} defspace_t;

defspace_t *new_defspace (void);
int defspace_new_loc (defspace_t *space, int size);
void defspace_free_loc (defspace_t *space, int ofs, int size);
int defspace_add_data (defspace_t *space, pr_type_t *data, int size);

#endif//__defspace_h
