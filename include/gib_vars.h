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

#include "QF/hash.h"
#include "QF/cbuf.h"

extern hashtab_t *gib_globals;

typedef struct gib_var_s {
	const char *key;
	struct gib_varray_s {
		struct dstring_s *value;
		struct hashtab_s *leaves;
	} *array;
	unsigned int size;
} gib_var_t;

typedef struct gib_domain_s {
	const char *name;
	hashtab_t *vars;
} gib_domain_t;

gib_var_t *GIB_Var_Get (hashtab_t *first, hashtab_t *second, const char *key);
gib_var_t *GIB_Var_Get_Complex (hashtab_t **first, hashtab_t **second, char *key, unsigned int *ind, bool create);
gib_var_t *GIB_Var_Get_Very_Complex (hashtab_t ** first, hashtab_t ** second, dstring_t *key, unsigned int start, unsigned int *ind, bool create);
void GIB_Var_Assign (gib_var_t *var, unsigned int index, dstring_t **values, unsigned int numv, bool shrink);
hashtab_t *GIB_Domain_Get (const char *name);
hashtab_t *GIB_Var_Hash_New (void);

void GIB_Var_Init (void);
