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

	$Id$
*/

inline qboolean GIB_Escaped (const char *str, int i);

char GIB_Parse_Match_Brace (const char *str, unsigned int *i);
char GIB_Parse_Match_Backtick (const char *str, unsigned int *i);
char GIB_Parse_Match_Index (const char *str, unsigned int *i);

void GIB_Parse_Strip_Comments (struct cbuf_s *cbuf);
void GIB_Parse_Extract_Line (struct cbuf_s *cbuf);
void GIB_Parse_Tokenize_Line (struct cbuf_s *cbuf);

extern struct cbuf_interpreter_s gib_interp;
