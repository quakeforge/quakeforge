/*
	pr_debug.h

	progs debug info

	Copyright (C) 2001       Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2001/7/12

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

#ifndef __pr_debug_h
#define __pr_debug_h

typedef struct {
	unsigned long function;		// function def this aux info is for
	unsigned long source_line;	// first source line for this function
	unsigned long line_info;	// index to first lineno entry
	unsigned long local_defs;	// index to the first local def
	unsigned long num_locals;	// number of local defs
} pr_auxfunction_t;

typedef struct {
	union {
		unsigned long func;		// (line==0) index of function aux info
		unsigned long addr;		// (line!=0) dstatement_t address
	} fa;
	unsigned long line;
} pr_lineno_t;

#define PROG_DEBUG_VERSION 0x00001001	// MMmmmRRR 0.001.001 (hex)

typedef struct {
	int				version;
	unsigned short	crc;		// of the progs.dat this progs.sym file is for
	unsigned short	you_tell_me_and_we_will_both_know;
	unsigned long	auxfunctions;
	unsigned long	num_auxfunctions;
	unsigned long	linenos;
	unsigned long	num_linenos;
	unsigned long	locals;
	unsigned long	num_locals;
} pr_debug_header_t;

#endif//__pr_debug_h
