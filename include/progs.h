/*
	progs.h

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.

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

#ifndef _PROGS_H
#define _PROGS_H

#include "link.h"
#include "quakeio.h"
#include "pr_comp.h"

typedef union eval_s
{
	string_t		string;
	float			_float;
	float			vector[3];
	func_t			function;
	int				_int;
	int				edict;
} eval_t;	

typedef union pr_type_u {
	float	float_var;
	int		int_var;
	string_t string_var;
	func_t	func_var;
	int		entity_var;
	float	vector_var[1];		// really 3, but this structure must be 32 bits
} pr_type_t;

#define	MAX_ENT_LEAFS	16
typedef struct edict_s
{
	qboolean	free;
	link_t		area;				// linked to a division node or leaf
	
	int			num_leafs;
	short		leafnums[MAX_ENT_LEAFS];

	float		freetime;			// sv.time when the object was freed
	void		*data;			// external per-edict data
	pr_type_t   v[1];			// fields from progs
} edict_t;
#define	EDICT_FROM_AREA(l) STRUCT_FROM_LINK(l,edict_t,area)

#ifndef PROGS_T
typedef struct progs_s progs_t;
#define PROGS_T
#endif

//============================================================================

void PR_Init (void);
void PR_Init_Cvars (void);

void PR_ExecuteProgram (progs_t *pr, func_t fnum);
void PR_LoadProgs (progs_t *pr, char *progsname);
void PR_LoadStrings (progs_t *pr);

void PR_Profile_f (void);

edict_t *ED_Alloc (progs_t *pr);
void ED_Free (progs_t *pr, edict_t *ed);

char	*ED_NewString (progs_t *pr, char *string);
// returns a copy of the string allocated from the server's string heap

void ED_Print (progs_t *pr, edict_t *ed);
void ED_Write (progs_t *pr, QFile *f, edict_t *ed);
char *ED_ParseEdict (progs_t *pr, char *data, edict_t *ent);

void ED_WriteGlobals (progs_t *pr, QFile *f);
void ED_ParseGlobals (progs_t *pr, char *data);

void ED_LoadFromFile (progs_t *pr, char *data);

ddef_t *ED_FindField (progs_t *pr, char *name);
int ED_GetFieldIndex (progs_t *pr, char *name);
dfunction_t *ED_FindFunction (progs_t *pr, char *name);


//define EDICT_NUM(p,n) ((edict_t *)(*(p)->edicts+ (n)*(p)->pr_edict_size))
//define NUM_FOR_EDICT(p,e) (((byte *)(e) - *(p)->edicts)/(p)->pr_edict_size)

edict_t *EDICT_NUM(progs_t *pr, int n);
int NUM_FOR_EDICT(progs_t *pr, edict_t *e);

#define	NEXT_EDICT(p,e) ((edict_t *)( (byte *)e + (p)->pr_edict_size))

#define PR_edicts(p)		((byte *)*(p)->edicts)

#define	EDICT_TO_PROG(p,e) ((byte *)(e) - PR_edicts (p))
#define PROG_TO_EDICT(p,e) ((edict_t *)(PR_edicts (p) + (e)))

//============================================================================

#define G_var(p,o,t)	((p)->pr_globals[o].t##_var)

#define	G_FLOAT(p,o)	G_var (p, o, float)
#define	G_INT(p,o)		G_var (p, o, int)
#define G_EDICT(p,o)	((edict_t *)(PR_edicts (p) + G_INT (p, o)))
#define G_EDICTNUM(p,o)	NUM_FOR_EDICT(p, G_EDICT(p, o))
#define	G_VECTOR(p,o)	G_var (p, o, vector)
#define	G_STRING(p,o)	PR_GetString (p, G_var (p, o, string))
#define	G_FUNCTION(p,o)	G_var (p, o, func)

#define E_var(e,o,t)	((e)->v[o].t##_var)

#define	E_FLOAT(e,o)	E_var (e, o, float)
#define	E_INT(e,o)		E_var (e, o, int)
#define	E_VECTOR(e,o)	E_var (e, o, vector)
#define	E_STRING(p,e,o)	(PR_GetString (p, E_var (e, o, string)))

extern	int		type_size[8];

typedef void (*builtin_t) (progs_t *pr);
extern	builtin_t *pr_builtins;
extern int pr_numbuiltins;

ddef_t *PR_FindGlobal (progs_t *pr, const char *name);
eval_t *PR_GetGlobalPointer (progs_t *pr, const char *name);

void PR_Error (progs_t *pr, const char *error, ...) __attribute__((format(printf,2,3)));
void PR_RunError (progs_t *pr, char *error, ...) __attribute__((format(printf,2,3)));

void ED_PrintEdicts (progs_t *pr);
void ED_PrintNum (progs_t *pr, int ent);
void ED_Count (progs_t *pr);
void PR_Profile (progs_t *pr);

char *PR_GlobalString (progs_t *pr, int ofs);
char *PR_GlobalStringNoContents (progs_t *pr, int ofs);

eval_t *GetEdictFieldValue(progs_t *pr, edict_t *ed, char *field);

//
// PR STrings stuff
//
#define MAX_PRSTR 1024

char *PR_GetString(progs_t *pr, int num);
int PR_SetString(progs_t *pr, char *s);

// externaly supplied functions

int ED_Parse_Extra_Fields (progs_t *pr, char *key, char *value);
int ED_Prune_Edict (progs_t *pr, edict_t *ent);

//============================================================================

#define MAX_STACK_DEPTH		32
#define LOCALSTACK_SIZE		2048

typedef struct {
	int         s;
	dfunction_t *f;
} prstack_t;

typedef struct strref_s {
	struct strref_s *next;
	struct strref_s *prev;
	char *string;
	int count;
} strref_t;

struct progs_s {
	dprograms_t		*progs;

	struct hashtab_s *function_hash;
	struct hashtab_s *global_hash;
	struct hashtab_s *field_hash;

	// garbage collected strings
	strref_t		*static_strings;
	strref_t		dynamic_strings; // head of linked list;
	struct hashtab_s *strref_hash;
	int				num_strings;

	dfunction_t		*pr_functions;
	char			*pr_strings;
	int				pr_stringsize;
	ddef_t			*pr_globaldefs;
	ddef_t			*pr_fielddefs;
	dstatement_t	*pr_statements;
	pr_type_t		*pr_globals;			// same as pr_global_struct

	int				pr_edict_size;	// in bytes
	int				pr_edictareasize; // for bounds checking, starts at 0

	int				pr_argc;

	qboolean		pr_trace;
	dfunction_t		*pr_xfunction;
	int				pr_xstatement;

	char			*pr_strtbl[MAX_PRSTR];
	int				num_prstr;

	prstack_t		pr_stack[MAX_STACK_DEPTH];
	int				pr_depth;

	int				localstack[LOCALSTACK_SIZE];
	int				localstack_used;

	edict_t			**edicts;
	int				*num_edicts;
	int				*reserved_edicts;	//alloc will start at reserved_edicts+1
	double			*time;
	int				null_bad;

	unsigned short	crc;

	void			(*unlink)(edict_t *ent);
	void			(*flush)(void);

	// required globals
	struct {
		float		*time;
		int			*self;
	} globals;
	// required fields (offsets into the edict)
	struct {
		int			nextthink;
		int			frame;
		int			think;
	} fields;
};

#endif // _PROGS_H
