/*
	diagnostic.h

	Diagnostic messages.

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/01/24

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

#ifndef __diagnostic_h
#define __diagnostic_h

#include "QF/pr_comp.h"

/**	\defgroup qfcc_diagnostic Diagnostic Messages
	\ingroup qfcc
*/
//@{

struct expr_s *error (struct expr_s *e, const char *fmt, ...)
	__attribute__ ((format (printf, 2, 3)));
void
internal_error (struct expr_s *e, const char *fmt, ...)
	__attribute__ ((format (printf, 2, 3), noreturn));
struct expr_s *warning (struct expr_s *e, const char *fmt, ...)
	__attribute__ ((format (printf, 2, 3)));
struct expr_s *notice (struct expr_s *e, const char *fmt, ...)
	__attribute__ ((format (printf, 2, 3)));
void debug (struct expr_s *e, const char *mft, ...)
	__attribute__ ((format (printf, 2, 3)));

//@}

#endif//__diagnostic_h
