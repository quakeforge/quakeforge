/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

#ifndef __struct_h
#define __struct_h

typedef struct struct_field_s {
	struct struct_field_s *next;
	const char		*name;
	struct type_s	*type;
	int				offset;
} struct_field_t;

struct_field_t *new_struct_field (struct type_s *strct, struct type_s *type,
								  const char *name);
struct_field_t *struct_find_field (struct type_s *strct, const char *name);
struct type_s *new_struct (const char *name);
struct type_s *find_struct (const char *name);

void process_enum (struct expr_s *enm);
expr_t *get_enum (const char *name);

#endif//__struct_h
