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
extern	type_t	type_Protocol;
extern	type_t	type_SEL;
extern	type_t	type_IMP;
extern	type_t	type_obj_exec_class;
extern	type_t	type_Method;
extern	type_t	*type_category;
extern	type_t	*type_ivar;
extern	type_t	*type_module;

extern	def_t	def_void;
extern	def_t	def_function;

struct dstring_s;

type_t *find_type (type_t *new);
void new_typedef (const char *name, type_t *type);
type_t *get_typedef (const char *name);
type_t *pointer_type (type_t *aux);
type_t *array_type (type_t *aux, int size);
void print_type (type_t *type);
void encode_type (struct dstring_s *encodking, type_t *type);
int type_assignable (type_t *dst, type_t *src);
int type_size (type_t *type);

void init_types (void);

#endif//__type_h
