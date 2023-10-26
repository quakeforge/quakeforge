/*
	cpp.h

	cpp preprocessing support

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/06/04

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

#ifndef __cpp_h
#define __cpp_h

struct dstring_s;

void parse_cpp_name (void);
void add_cpp_undef (const char *arg);
void add_cpp_def (const char *arg);

int cpp_depend (const char *opt, const char *arg);
int cpp_include (const char *opt, const char *arg);
void cpp_define (const char *arg);
void cpp_undefine (const char *arg);
const char *cpp_find_file (const char *name, int quote, bool *is_system);
void cpp_set_quote_file (const char *path);

void intermediate_file (struct dstring_s *ifile, const char *filename,
						const char *ext, int local);
FILE *preprocess_file (const char *filename, const char *ext);
extern const char *cpp_name;
extern struct dstring_s *tempname;

void cpp_write_dependencies (const char *sourcefile);

#endif//__cpp_h
