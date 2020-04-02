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

*/

#ifndef __QF_pr_debug_h
#define __QF_pr_debug_h

#ifndef __QFCC__
#include "QF/pr_comp.h"

typedef struct pr_auxfunction_s {
	pr_uint_t   function;		// function def this aux info is for
	pr_uint_t   source_line;	// first source line for this function
	pr_uint_t   line_info;		// index to first lineno entry
	pr_uint_t   local_defs;		// index to the first local def
	pr_uint_t   num_locals;		// number of local defs
	pr_uint_t   return_type;	// return type of this function
} pr_auxfunction_t;

typedef struct pr_lineno_s {
	union {
		pr_uint_t   func;		// (line==0) index of function aux info
		pr_uint_t   addr;		// (line!=0) dstatement_t address
	} fa;
	pr_uint_t   line;
} pr_lineno_t;

#define PROG_DEBUG_VERSION 0x00001003	// MMmmmRRR 0.001.002 (hex)

typedef struct pr_debug_header_s {
	pr_int_t    version;
	pr_ushort_t crc;			// of the progs.dat this progs.sym file is for
	pr_ushort_t you_tell_me_and_we_will_both_know;
	pr_uint_t   auxfunctions;
	pr_uint_t   num_auxfunctions;
	pr_uint_t   linenos;
	pr_uint_t   num_linenos;
	pr_uint_t   locals;
	pr_uint_t   num_locals;
} pr_debug_header_t;
#endif

typedef enum prdebug_e {
	prd_none,
	prd_trace,
	prd_breakpoint,
	prd_watchpoint,
	prd_subenter,
	prd_subexit,		// current invocation of PR_ExecuteProgram finished
	prd_begin,			// not sent by VM
	prd_terminate,		// not sent by VM
	prd_runerror,
	prd_error,			// lower level error thann prd_runerror
} prdebug_t;

#endif//__QF_pr_debug_h
