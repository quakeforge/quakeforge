/*
	qfprogs.h

	Progs dumping, main header.

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/05/13

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

#ifndef __qfprogs_h
#define __qfprogs_h

struct progs_s;
struct qfo_s;

extern int sorted;
extern int verbosity;

extern const char *reloc_names[];

void disassemble_progs (struct progs_s *pr);

void dump_globals (struct progs_s *pr);
void dump_fields (struct progs_s *pr);
void dump_functions (struct progs_s *pr);
void dump_types (struct progs_s *pr);

void dump_lines (struct progs_s *pr);

void dump_modules (struct progs_s *pr);

struct dfunction_s *func_find (int st_num);

void dump_strings (struct progs_s *pr);

void qfo_globals (struct qfo_s *qfo);
void qfo_functions (struct qfo_s *qfo);
void qfo_lines (struct qfo_s *qfo);
void qfo_relocs (struct qfo_s *qfo);
void qfo_types (struct qfo_s *qfo);

#endif//__qfprogs_h
