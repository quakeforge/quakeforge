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

typedef struct pr_auxfunction_s {
	unsigned int function;		// function def this aux info is for
	unsigned int source_line;	// first source line for this function
	unsigned int line_info;	// index to first lineno entry
	unsigned int local_defs;	// index to the first local def
	unsigned int num_locals;	// number of local defs
} pr_auxfunction_t;

typedef struct pr_lineno_s {
	union {
		unsigned int func;		// (line==0) index of function aux info
		unsigned int addr;		// (line!=0) dstatement_t address
	} fa;
	unsigned int line;
} pr_lineno_t;

#define PROG_DEBUG_VERSION 0x00001001	// MMmmmRRR 0.001.001 (hex)

typedef struct pr_debug_header_s {
	int				version;
	unsigned short	crc;		// of the progs.dat this progs.sym file is for
	unsigned short	you_tell_me_and_we_will_both_know;
	unsigned int	auxfunctions;
	unsigned int	num_auxfunctions;
	unsigned int	linenos;
	unsigned int	num_linenos;
	unsigned int	locals;
	unsigned int	num_locals;
} pr_debug_header_t;

#endif//__pr_debug_h
