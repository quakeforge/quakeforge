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

#ifndef __type_h
#define __type_h

extern	type_t	type_void;
extern	type_t	type_string;
extern	type_t	type_float;
extern	type_t	type_vector;
extern	type_t	type_entity;
extern	type_t	type_field;
extern	type_t	type_function;
extern	type_t	type_pointer;
extern	type_t	type_floatfield;
extern	type_t	type_quaternion;
extern	type_t	type_integer;
extern	type_t	type_uinteger;
extern	type_t	type_short;
extern	type_t	type_struct;
extern	type_t	type_id;
extern	type_t	type_Class;
extern	type_t	type_SEL;
extern	type_t	type_IMP;
extern	type_t	type_method_list;
extern	type_t	*type_method;

extern	def_t	def_void;
extern	def_t	def_function;

type_t *find_type (type_t *new);
void new_typedef (const char *name, struct type_s *type);
struct type_s *get_typedef (const char *name);
struct type_s *pointer_type (struct type_s *aux);
void print_type (struct type_s *type);

void init_types (void);

#endif//__type_h
