/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2002 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

#include "regex.h"

typedef struct gib_regex_s {
		char *regex;
		regex_t comp;
		int cflags;
} gib_regex_t;

void GIB_Regex_Init (void);
regex_t *GIB_Regex_Compile (const char *regex, int cflags);
const char *GIB_Regex_Error (void) __attribute__((const));
int GIB_Regex_Translate_Options (const char *opstr) __attribute__((pure));
int GIB_Regex_Translate_Runtime_Options (const char *opstr) __attribute__((pure));
unsigned int GIB_Regex_Apply_Match (regmatch_t match[10], dstring_t *dstr, unsigned int ofs, const char *replace);
