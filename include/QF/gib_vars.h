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

#include "QF/hash.h"
#include "QF/cbuf.h"

extern hashtab_t *gib_globals;

typedef struct gib_var_s {
	struct dstring_s *key, *value;
	struct hashtab_s *subvars;
} gib_var_t;

void GIB_Var_Set_Local (cbuf_t *cbuf, const char *key, const char *value);
void GIB_Var_Set_Global (const char *key, const char *value);
void GIB_Var_Set (cbuf_t *cbuf, char *key, const char *value);
const char *GIB_Var_Get_Local (cbuf_t *cbuf, const char *key);
const char *GIB_Var_Get_Global (const char *key);
const char *GIB_Var_Get (cbuf_t *cbuf, char *key);
const char *GIB_Var_Get_Key (void *ele, void *ptr);
void GIB_Var_Free (void *ele, void *ptr);
void GIB_Var_Free_Global (const char *key);
