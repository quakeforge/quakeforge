/*
	gib.h

	#DESCRIPTION#

	Copyright (C) 2000       #AUTHOR#

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

#ifndef __GIB_H
#define __GIB_H

typedef int (*gib_func_t) (void);

typedef struct gib_var_s
{
        char *key;  
        char *value;
        struct gib_var_s *next;
} gib_var_t;


typedef struct gib_sub_s
{
	char *name;
	char *code;
	gib_var_t *vars;
	struct gib_sub_s *next;
} gib_sub_t;

typedef struct gib_module_s
{
	char *name;
	gib_sub_t *subs;
	gib_var_t *vars;
	struct gib_module_s *next;
} gib_module_t;

typedef struct gib_inst_s
{
	char *name;
	gib_func_t func;
	struct gib_inst_s *next;
} gib_inst_t;

void GIB_Init (void);
void GIB_Gib_f (void);
void GIB_Load_f (void);
void GIB_Stats_f (void);

#endif // __GIB_H
