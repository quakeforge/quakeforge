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

#ifndef __QF_progs_h
#define __QF_progs_h

#include "QF/link.h"
#include "QF/vfile.h"
#include "QF/pr_comp.h"
#include "QF/pr_debug.h"

typedef union pr_type_u {
	float	float_var;
	string_t string_var;
	func_t	func_var;
	int		entity_var;
	float	vector_var[1];		// really 3, but this structure must be 32 bits
	int		integer_var;
	unsigned int uinteger_var;
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

#ifndef PR_RESOURCE_T
typedef struct pr_resource_s pr_resource_t;
#define PR_RESOURCE_T
#endif

//============================================================================

void PR_Init (void);
void PR_Init_Cvars (void);

void PR_PrintStatement (progs_t * pr, dstatement_t *s);
void PR_ExecuteProgram (progs_t *pr, func_t fnum);
void PR_LoadProgsFile (progs_t * pr, VFile *file, int size, int edicts,
					   int zone);
void PR_LoadProgs (progs_t *pr, const char *progsname, int edicts, int zone);
void PR_LoadStrings (progs_t *pr);
void PR_LoadDebug (progs_t *pr);
edict_t *PR_InitEdicts (progs_t *pr, int num_edicts);
void PR_Check_Opcodes (progs_t *pr);

void PR_Profile_f (void);

void ED_ClearEdict (progs_t * pr, edict_t *e, int val);
edict_t *ED_Alloc (progs_t *pr);
void ED_Free (progs_t *pr, edict_t *ed);

char	*ED_NewString (progs_t *pr, const char *string);
// returns a copy of the string allocated from the server's string heap

void ED_Print (progs_t *pr, edict_t *ed);
void ED_Write (progs_t *pr, VFile *f, edict_t *ed);
const char *ED_ParseEdict (progs_t *pr, const char *data, edict_t *ent);

void ED_WriteGlobals (progs_t *pr, VFile *f);
void ED_ParseGlobals (progs_t *pr, const char *data);

void ED_LoadFromFile (progs_t *pr, const char *data);

ddef_t *ED_FieldAtOfs (progs_t *pr, int ofs);
ddef_t *ED_FindField (progs_t *pr, const char *name);
int ED_GetFieldIndex (progs_t *pr, const char *name);
dfunction_t *ED_FindFunction (progs_t *pr, const char *name);

int PR_AccessField (progs_t *pr, const char *name, etype_t type,
					const char *file, int line);

//define EDICT_NUM(p,n) ((edict_t *)(*(p)->edicts+ (n)*(p)->pr_edict_size))
//define NUM_FOR_EDICT(p,e) (((byte *)(e) - *(p)->edicts)/(p)->pr_edict_size)

edict_t *EDICT_NUM(progs_t *pr, int n);
int NUM_FOR_EDICT(progs_t *pr, edict_t *e);
int NUM_FOR_BAD_EDICT(progs_t *pr, edict_t *e);

#define	NEXT_EDICT(p,e) ((edict_t *)( (byte *)e + (p)->pr_edict_size))

#define PR_edicts(p)		((byte *)*(p)->edicts)

#define	EDICT_TO_PROG(p,e) ((byte *)(e) - PR_edicts (p))
#define PROG_TO_EDICT(p,e) ((edict_t *)(PR_edicts (p) + (e)))

//============================================================================

#define G_var(p,o,t)	((p)->pr_globals[o].t##_var)

#define	G_FLOAT(p,o)	G_var (p, o, float)
#define	G_INT(p,o)		G_var (p, o, integer)
#define	G_UINT(p,o)		G_var (p, o, uinteger)
#define G_EDICT(p,o)	((edict_t *)(PR_edicts (p) + G_INT (p, o)))
#define G_EDICTNUM(p,o)	NUM_FOR_EDICT(p, G_EDICT(p, o))
#define	G_VECTOR(p,o)	G_var (p, o, vector)
#define	G_STRING(p,o)	PR_GetString (p, G_var (p, o, string))
#define	G_FUNCTION(p,o)	G_var (p, o, func)

#define RETURN_STRING(p, s) ((p)->pr_globals[OFS_RETURN].integer_var = PR_SetString((p), s))
#define RETURN_EDICT(p, e) ((p)->pr_globals[OFS_RETURN].integer_var = EDICT_TO_PROG(p, e))


#define E_var(e,o,t)	((e)->v[o].t##_var)

#define	E_FLOAT(e,o)	E_var (e, o, float)
#define	E_INT(e,o)		E_var (e, o, integer)
#define	E_UINT(e,o)		E_var (e, o, uinteger)
#define	E_VECTOR(e,o)	E_var (e, o, vector)
#define	E_STRING(p,e,o)	(PR_GetString (p, E_var (e, o, string)))

typedef void (*builtin_proc) (progs_t *pr);
typedef struct {
	const char *name;
	builtin_proc proc;
	int first_statement;
} builtin_t;

ddef_t *PR_FindGlobal (progs_t *pr, const char *name);
ddef_t *ED_GlobalAtOfs (progs_t * pr, int ofs);

pr_type_t *PR_GetGlobalPointer (progs_t *pr, const char *name);
func_t PR_GetFunctionIndex (progs_t *pr, const char *name);
int PR_GetFieldOffset (progs_t *pr, const char *name);

void PR_Error (progs_t *pr, const char *error, ...) __attribute__((format(printf,2,3), noreturn));
void PR_RunError (progs_t *pr, const char *error, ...) __attribute__((format(printf,2,3), noreturn));

void ED_PrintEdicts (progs_t *pr, const char *fieldval);
void ED_PrintNum (progs_t *pr, int ent);
void ED_Count (progs_t *pr);
void PR_Profile (progs_t *pr);

char *PR_GlobalString (progs_t *pr, int ofs, etype_t type);
char *PR_GlobalStringNoContents (progs_t *pr, int ofs, etype_t type);

pr_type_t *GetEdictFieldValue(progs_t *pr, edict_t *ed, const char *field);

void PR_AddBuiltin (progs_t *pr, const char *name, builtin_proc builtin, int num);
builtin_t *PR_FindBuiltin (progs_t *pr, const char *name);
int PR_RelocateBuiltins (progs_t *pr);
int PR_ResolveGlobals (progs_t *pr);

//
// PR Strings stuff
//

char *PR_GetString(progs_t *pr, int num);
int PR_SetString(progs_t *pr, const char *s);
void PR_GarbageCollect (progs_t *pr);

//
// PR Resources stuff
//

void
PR_Resources_Init (progs_t *pr);
void PR_Resources_Clear (progs_t *pr);
void PR_Resources_Register (progs_t *pr, const char *name, void *data,
							void (*clear)(progs_t *, void *));
void *PR_Resources_Find (progs_t *pr, const char *name);

//
// PR Zone stuff
//

void PR_Zone_Init (progs_t *pr);
void PR_Zone_Free (progs_t *pr, void *ptr);
void PR_Zone_Malloc (progs_t *pr, int size);

//
// PR Debug stuff
//

void PR_Debug_Init (void);
void PR_Debug_Init_Cvars (void);
pr_auxfunction_t *PR_Get_Lineno_Func (progs_t *pr, pr_lineno_t *lineno);
unsigned int PR_Get_Lineno_Addr (progs_t *pr, pr_lineno_t *lineno);
unsigned int PR_Get_Lineno_Line (progs_t *pr, pr_lineno_t *lineno);
pr_lineno_t *PR_Find_Lineno (progs_t *pr, unsigned int addr);
const char *PR_Get_Source_File (progs_t *pr, pr_lineno_t *lineno);
const char *PR_Get_Source_Line (progs_t *pr, unsigned int addr);
ddef_t *PR_Get_Local_Def (progs_t *pr, int offs);
void PR_DumpState (progs_t *pr);
void PR_StackTrace (progs_t * pr);

extern struct cvar_s *pr_debug;
extern struct cvar_s *pr_deadbeef_ents;
extern struct cvar_s *pr_deadbeef_locals;
extern struct cvar_s *pr_boundscheck;

extern const char *pr_gametype;

//
// PR Cmds stuff
//

void PR_Cmds_Init (progs_t *pr);

//============================================================================

#define MAX_STACK_DEPTH		32
#define LOCALSTACK_SIZE		2048

typedef struct {
	int         s;
	dfunction_t *f;
} prstack_t;

typedef struct strref_s {
	struct strref_s * next;
	char *string;
	int count;
} strref_t;

struct progs_s {
	const char		*progs_name;
	dprograms_t		*progs;
	int				progs_size;

	struct memzone_s *zone;
	int             zone_size;

	struct hashtab_s *builtin_hash;
	struct hashtab_s *function_hash;
	struct hashtab_s *global_hash;
	struct hashtab_s *field_hash;

	// garbage collected strings
	strref_t		*static_strings;
	strref_t		**dynamic_strings;
	strref_t		*free_string_refs;
	int				dyn_str_size;
	struct hashtab_s *strref_hash;
	int				num_strings;

	dfunction_t		*pr_functions;
	char			*pr_strings;
	int				pr_stringsize;
	ddef_t			*pr_globaldefs;
	ddef_t			*pr_fielddefs;
	dstatement_t	*pr_statements;
	pr_type_t		*pr_globals;			// same as pr_global_struct
	int				globals_size;

	int				pr_edict_size;	// in bytes
	int				pr_edictareasize; // for bounds checking, starts at 0

	int				pr_argc;

	qboolean		pr_trace;
	dfunction_t		*pr_xfunction;
	int				pr_xstatement;

	prstack_t		pr_stack[MAX_STACK_DEPTH];
	int				pr_depth;

	int				localstack[LOCALSTACK_SIZE];
	int				localstack_used;

	edict_t			**edicts;
	int				*num_edicts;
	int				*reserved_edicts;	//alloc will start at reserved_edicts+1
	double			*time;
	int				null_bad;
	int				no_exec_limit;

	unsigned short	crc;

	void			(*unlink)(edict_t *ent);
	void			(*flush)(void);

	int				(*parse_field)(progs_t *pr, const char *key, const char *value);
	int				(*prune_edict)(progs_t *pr, edict_t *ent);
	void			(*free_edict)(progs_t *pr, edict_t *ent);

	void			*(*allocate_progs_mem)(progs_t *pr, int size);
	void			(*free_progs_mem)(progs_t *pr, void *mem);

	builtin_t		**builtins;
	int				numbuiltins;

	pr_resource_t	*resources;
	struct hashtab_s *resource_hash;

	// debug info
	char			*debugfile;
	struct pr_debug_header_s *debug;
	struct pr_auxfunction_s *auxfunctions;
	struct pr_auxfunction_s **auxfunction_map;
	struct pr_lineno_s *linenos;
	ddef_t			*local_defs;

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

#endif//__QF_progs_h
