/*
	codespace.h

	management of code segement

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/01/07

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

#ifndef __codespace_h
#define __codespace_h

typedef struct codespace_s {
	struct dstatement_s *code;
	int         size;
	int         max_size;
	int         qfo_space;
} codespace_t;

codespace_t *codespace_new (void);
void codespace_delete (codespace_t *codespace);
void codespace_addcode (codespace_t *codespace, struct dstatement_s *code,
						int size);
struct dstatement_s *codespace_newstatement (codespace_t *codespace);

#endif//__codespace_h
