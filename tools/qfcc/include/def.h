/*
	def.h

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

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

#ifndef __def_h
#define __def_h

#include "QF/pr_comp.h"
#include "QF/pr_debug.h"

typedef struct def_s {
	struct type_s	*type;
	const char		*name;
	int				ofs;

	struct reloc_s *refs;			// for relocations

	unsigned		initialized:1;	// for uninit var detection
	unsigned		constant:1;	// 1 when a declaration included "= immediate"
	unsigned		freed:1;		// already freed from the scope
	unsigned		removed:1;		// already removed from the symbol table
	unsigned		used:1;			// unused local detection
	unsigned		absolute:1;		// don't relocate (for temps for shorts)
	unsigned		managed:1;		// managed temp

	string_t		file;			// source file
	int				line;			// source line

	int				users;			// ref counted temps
	struct expr_s	*expr;			// temp expr using this def

	struct def_s	*def_next;		// next def in scope
	struct def_s	*next;			// general purpose linking
	struct scope_s	*scope;			// scope the var was defined in
	struct def_s	*parent;		// vector/quaternion member

	void			*return_addr;	// who allocated this
} def_t;

typedef struct defspace_s {
	struct defspace_s *next;
	pr_type_t  *data;
	int         size;
	int         max_size;
	int       (*grow) (struct defspace_s *space);
} defspace_t;

typedef struct scope_s {
	struct scope_s *next;
	defspace_t *space;
	def_t      *head;
	def_t     **tail;
	int         num_defs;
	struct scope_s *parent;
} scope_t;

extern	def_t	def_ret, def_parms[MAX_PARMS];
extern	def_t	def_void;
extern	def_t	def_function;

scope_t *new_scope (defspace_t *space, scope_t *parent);
defspace_t *new_defspace (void);

def_t *PR_GetDef (struct type_s *type, const char *name, scope_t *scope,
				  int allocate);
def_t *PR_NewDef (struct type_s *type, const char *name, scope_t *scope);
int PR_NewLocation (struct type_s *type, defspace_t *space);
void PR_FreeLocation (def_t *def);
def_t *PR_GetTempDef (struct type_s *type, scope_t *scope);
void PR_FreeTempDefs ();
void PR_ResetTempDefs ();
void PR_FlushScope (scope_t *scope, int force_used);
void PR_DefInitialized (def_t *d);


#endif//__def_h
