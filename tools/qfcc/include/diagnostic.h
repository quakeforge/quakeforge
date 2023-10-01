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

/**	\defgroup qfcc_diagnostic Diagnostic Messages
	\ingroup qfcc
*/
///@{

typedef void (*diagnostic_hook)(const char *message);
extern diagnostic_hook bug_hook;
extern diagnostic_hook error_hook;
extern diagnostic_hook warning_hook;
extern diagnostic_hook notice_hook;

struct expr_s *_error (const struct expr_s *e, const char *file,
					   int line, const char *func, const char *fmt, ...)
	__attribute__ ((format (PRINTF, 5, 6)));
#define error(e, fmt...) _error(e, __FILE__, __LINE__, __FUNCTION__, fmt)

void _internal_error (const struct expr_s *e, const char *file, int line,
					  const char *func, const char *fmt, ...)
	__attribute__ ((format (PRINTF, 5, 6), noreturn));
#define internal_error(e, fmt...) _internal_error(e, __FILE__, __LINE__, __FUNCTION__, fmt)

struct expr_s *_warning (const struct expr_s *e, const char *file,
						 int line, const char *func, const char *fmt, ...)
	__attribute__ ((format (PRINTF, 5, 6)));
#define warning(e, fmt...) _warning(e, __FILE__, __LINE__, __FUNCTION__, fmt)

struct expr_s *_notice (const struct expr_s *e, const char *file,
						int line, const char *func, const char *fmt, ...)
	__attribute__ ((format (PRINTF, 5, 6)));
#define notice(e, fmt...) _notice(e, __FILE__, __LINE__, __FUNCTION__, fmt)

void _debug (const struct expr_s *e, const char *file, int line,
			 const char *func, const char *fmt, ...)
	__attribute__ ((format (PRINTF, 5, 6)));
#define debug(e, fmt...) _debug(e, __FILE__, __LINE__, __FUNCTION__, fmt)

void _bug (const struct expr_s *e, const char *file, int line,
		   const char * func, const char *fmt, ...)
	__attribute__ ((format (PRINTF, 5, 6)));
#define bug(e, fmt...) _bug(e, __FILE__, __LINE__, __FUNCTION__, fmt)

void print_srcline (int rep, const struct expr_s *e);

///@}

#endif//__diagnostic_h
