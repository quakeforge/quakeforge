/*
	pr_lex.c

	Lexical parser for GameC

	Copyright (C) 1996-1997 Id Software, Inc.
	Copyright (C) 2001 Jeff Teunissen <deek@dusknet.dhs.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
static const char rcsid[] =
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <QF/sys.h>

#include "qfcc.h"

int         pr_source_line;

char       *pr_file_p;
char       *pr_line_start;				// start of current source line

int         pr_bracelevel;

char        pr_token[2048];
int         pr_token_len;
type_t     *pr_immediate_type;
eval_t      pr_immediate;

int         pr_error_count;
