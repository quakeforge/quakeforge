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

#ifndef __QF_cbuf_h
#define __QF_cbuf_h

typedef struct cbuf_args_s {
	int         argc;
	struct dstring_s **argv;
	const char **args;
	int         argv_size;
} cbuf_args_t;

typedef struct cbuf_s {
	struct dstring_s *buf;
	struct dstring_s *line;
	cbuf_args_t *args;
	void        (*extract_line) (struct cbuf_s *cbuf);
	void        (*parse_line) (struct cbuf_s *cbuf);
} cbuf_t;

cbuf_t *Cbuf_New (void);
void CBuf_Delete (cbuf_t *cbuf);
void Cbuf_AddText (cbuf_t *cbuf, const char *text);
void Cbuf_InsertText (cbuf_t *cbuf, const char *text);
void Cbuf_Execute (cbuf_t *cbuf);
void Cbuf_Execute_Sets (cbuf_t *cbuf);

#endif//__QF_cbuf_h
