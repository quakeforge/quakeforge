/*
	shared.h

	Functions and data shared between languages.

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/10/26

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

#ifndef __shared_h
#define __shared_h

extern struct function_s *current_func;
extern struct class_type_s *current_class;
extern enum vis_e       current_visibility;
extern enum storage_class_e current_storage;
extern struct symtab_s *current_symtab;

struct symbol_s *check_redefined (struct symbol_s *sym);
extern struct symbol_s *check_undefined (struct symbol_s *sym);

#endif//__shared_h
