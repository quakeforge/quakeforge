/*
	link.h

	qc object file linking

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/7/3

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

#ifndef __linker_h
#define __linker_h

struct qfo_s;
typedef struct type_s type_t;

void linker_begin (void);
int linker_add_string (const char *str);
void linker_add_def (const char *name, const type_t *type, unsigned flags,
					 void *val);
struct qfo_def_s *linker_find_def (const char *name);
int linker_add_qfo (struct qfo_s *qfo);
int linker_add_object_file (const char *filename);
int linker_add_lib (const char *libname);
void linker_add_path (const char *path);
struct qfo_s *linker_finish (void);

#endif//__linker_h
