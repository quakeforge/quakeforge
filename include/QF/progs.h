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
#include "QF/pr_comp.h"
#include "QF/pr_debug.h"
#include "QF/quakeio.h"

#define MAX_ENT_LEAFS	16
typedef struct edict_s {
	qboolean    free;
	link_t      area;			// linked to a division node or leaf

	int         num_leafs;
	short       leafnums[MAX_ENT_LEAFS];

	float       freetime;		// sv.time when the object was freed
	void       *data;			// external per-edict data
	pr_type_t   v[1];			// fields from progs
} edict_t;
#define EDICT_FROM_AREA(l) STRUCT_FROM_LINK(l,edict_t,area)

typedef struct progs_s progs_t;
typedef struct pr_resource_s pr_resource_t;

//============================================================================

void PR_Init (void);
void PR_Init_Cvars (void);

void PR_PrintStatement (progs_t *pr, dstatement_t *s, int contents);
void PR_PushFrame (progs_t *pr);
void PR_PopFrame (progs_t *pr);
void PR_EnterFunction (progs_t *pr, dfunction_t *f);
void PR_ExecuteProgram (progs_t *pr, func_t fnum);
void PR_LoadProgsFile (progs_t *pr, QFile *file, int size, int edicts,
					   int zone);
void PR_LoadProgs (progs_t *pr, const char *progsname, int edicts, int zone);
int PR_LoadStrings (progs_t *pr);
int PR_LoadDebug (progs_t *pr);
edict_t *PR_InitEdicts (progs_t *pr, int num_edicts);
int PR_Check_Opcodes (progs_t *pr);

void PR_Profile_f (void);

void ED_ClearEdict (progs_t *pr, edict_t *e, int val);
edict_t *ED_Alloc (progs_t *pr);
void ED_Free (progs_t *pr, edict_t *ed);

void ED_Print (progs_t *pr, edict_t *ed);
void ED_Write (progs_t *pr, QFile *f, edict_t *ed);
const char *ED_ParseEdict (progs_t *pr, const char *data, edict_t *ent);

void ED_WriteGlobals (progs_t *pr, QFile *f);
void ED_ParseGlobals (progs_t *pr, const char *data);

void ED_LoadFromFile (progs_t *pr, const char *data);

ddef_t *PR_FieldAtOfs (progs_t *pr, int ofs);
ddef_t *PR_GlobalAtOfs (progs_t *pr, int ofs);

ddef_t *PR_FindField (progs_t *pr, const char *name);
ddef_t *PR_FindGlobal (progs_t *pr, const char *name);
dfunction_t *PR_FindFunction (progs_t *pr, const char *name);

#define NEXT_EDICT(p,e)		((edict_t *)( (byte *)e + (p)->pr_edict_size))

#define PR_edicts(p)		((byte *)*(p)->edicts)

#define EDICT_TO_PROG(p,e)	((byte *)(e) - PR_edicts (p))
#define PROG_TO_EDICT(p,e)	((edict_t *)(PR_edicts (p) + (e)))

//============================================================================

#define G_var(p,o,t)	((p)->pr_globals[o].t##_var)

#define G_FLOAT(p,o)	G_var (p, o, float)
#define G_INT(p,o)		G_var (p, o, integer)
#define G_UINT(p,o)		G_var (p, o, uinteger)
#define G_VECTOR(p,o)	G_var (p, o, vector)
#define G_STRING(p,o)	G_var (p, o, string)
#define G_FUNCTION(p,o)	G_var (p, o, func)
#define G_POINTER(p,o)	G_var (p, o, pointer)

#define G_EDICT(p,o)	((edict_t *)(PR_edicts (p) + G_INT (p, o)))
#define G_EDICTNUM(p,o)	NUM_FOR_EDICT(p, G_EDICT (p, o))
#define G_GSTRING(p,o)	PR_GetString (p, G_STRING (p, o))
#define G_DSTRING(p,o)	PR_GetDString (p, G_STRING (p, o))
#define G_GPOINTER(p,o)	PR_GetPointer (p, o)
#define G_STRUCT(p,t,o)	(*(t *)G_GPOINTER (p, o))

#define P_var(p,n,t)	((p)->pr_params[n]->t##_var)

#define P_FLOAT(p,n)	P_var (p, n, float)
#define P_INT(p,n)		P_var (p, n, integer)
#define P_UINT(p,n)		P_var (p, n, uinteger)
#define P_VECTOR(p,n)	P_var (p, n, vector)
#define P_STRING(p,n)	P_var (p, n, string)
#define P_FUNCTION(p,n)	P_var (p, n, func)
#define P_POINTER(p,n)	P_var (p, n, pointer)

#define P_EDICT(p,n)	((edict_t *)(PR_edicts (p) + P_INT (p, n)))
#define P_EDICTNUM(p,n)	NUM_FOR_EDICT (p, P_EDICT (p, n))
#define P_GSTRING(p,n)	PR_GetString (p, P_STRING (p, n))
#define P_DSTRING(p,n)	PR_GetDString (p, P_STRING (p, n))
#define P_GPOINTER(p,n)	PR_GetPointer (p, P_POINTER (p, n))
#define P_STRUCT(p,t,n)	(*(t *)P_GPOINTER (p, n))

#define R_var(p,t)		((p)->pr_return->t##_var)

#define R_FLOAT(p)		R_var (p, float)
#define R_INT(p)		R_var (p, integer)
#define R_UINT(p)		R_var (p, uinteger)
#define R_VECTOR(p)		R_var (p, vector)
#define R_STRING(p)		R_var (p, string)
#define R_FUNCTION(p)	R_var (p, func)
#define R_POINTER(p)	R_var (p, pointer)

#define RETURN_STRING(p, s)		(R_STRING (p) = PR_SetReturnString((p), s))
#define RETURN_EDICT(p, e)		(R_STRING (p) = EDICT_TO_PROG(p, e))
#define RETURN_POINTER(pr,p)	(R_POINTER (pr) = POINTER_TO_PROG (pr, p))
#define RETURN_VECTOR(p, v)		VectorCopy (v, R_VECTOR (p))

#define E_var(e,o,t)	((e)->v[o].t##_var)

#define E_FLOAT(e,o)	E_var (e, o, float)
#define E_INT(e,o)		E_var (e, o, integer)
#define E_UINT(e,o)		E_var (e, o, uinteger)
#define E_VECTOR(e,o)	E_var (e, o, vector)
#define E_STRING(e,o)	E_var (e, o, string)
#define E_FUNCTION(p,o)	E_var (p, o, func)
#define E_POINTER(p,o)	E_var (p, o, pointer)

#define E_GSTRING(p,e,o) (PR_GetString (p, E_STRING (e, o)))
#define E_DSTRING(p,e,o) (PR_GetDString (p, E_STRING (e, o)))

/*
	Builtins are arranged into groups of 65536 to allow for diffent expantion
	sets. eg, stock id is 0x0000xxxx, quakeforge is 0x000fxxxx. predefined
	groups go up to 0x0fffxxxx allowing 4096 different groups. Builtins
	0x10000000 to 0x7fffffff are reserved for auto-allocation. The range
	0x8000000 to 0xffffffff is unavailable due to the builtin number being
	a negative statement address.
*/
#define PR_RANGE_SHIFT	16
#define PR_RANGE_MASK	0xffff0000

#define PR_RANGE_QF		0x000f

#define PR_RANGE_AUTO	0x1000
#define PR_RANGE_NONE	0xffff
#define PR_AUTOBUILTIN	(PR_RANGE_AUTO << PR_RANGE_SHIFT)

typedef void (*builtin_proc) (progs_t *pr);
typedef struct {
	const char *name;
	builtin_proc proc;
	int         binum;
} builtin_t;

typedef struct {
	int         first_statement;
	builtin_proc func;
} bfunction_t;


pr_type_t *PR_GetGlobalPointer (progs_t *pr, const char *name);
func_t PR_GetFunctionIndex (progs_t *pr, const char *name);
int PR_GetFieldOffset (progs_t *pr, const char *name);

void PR_Error (progs_t *pr, const char *error, ...) __attribute__((format(printf,2,3), noreturn));
void PR_RunError (progs_t *pr, const char *error, ...) __attribute__((format(printf,2,3), noreturn));

void ED_PrintEdicts (progs_t *pr, const char *fieldval);
void ED_PrintNum (progs_t *pr, int ent);
void ED_Count (progs_t *pr);
void PR_Profile (progs_t *pr);

pr_type_t *GetEdictFieldValue(progs_t *pr, edict_t *ed, const char *field);

void PR_RegisterBuiltins (progs_t *pr, builtin_t *builtins);
builtin_t *PR_FindBuiltin (progs_t *pr, const char *name);
builtin_t *PR_FindBuiltinNum (progs_t *pr, int num);
int PR_RelocateBuiltins (progs_t *pr);
int PR_ResolveGlobals (progs_t *pr);

typedef int pr_load_func_t (progs_t *);
void PR_AddLoadFunc (progs_t *pr, pr_load_func_t *func);
void PR_AddLoadFinishFunc (progs_t *pr, pr_load_func_t *func);
int PR_RunLoadFuncs (progs_t *pr);

//
// PR Strings stuff
//

qboolean PR_StringValid (progs_t *pr, int num);
const char *PR_GetString(progs_t *pr, int num);
struct dstring_s *PR_GetDString(progs_t *pr, int num);
int PR_SetString(progs_t *pr, const char *s);
int PR_SetReturnString(progs_t *pr, const char *s);
int PR_SetTempString(progs_t *pr, const char *s);
void PR_MakeTempString(progs_t *pr, int str);
int PR_NewString (progs_t *pr);
void PR_ClearReturnStrings (progs_t *pr);
void PR_FreeString (progs_t *pr, int str);
void PR_FreeTempStrings (progs_t *pr);
void PR_Sprintf (progs_t *pr, struct dstring_s *result, const char *name,
				 const char *format, int count, pr_type_t **args);

//
// PR Resources stuff
//

void PR_Resources_Init (progs_t *pr);
void PR_Resources_Clear (progs_t *pr);
void PR_Resources_Register (progs_t *pr, const char *name, void *data,
							void (*clear)(progs_t *, void *));
void *PR_Resources_Find (progs_t *pr, const char *name);

//
// PR Zone stuff
//

void PR_Zone_Init (progs_t *pr);
void PR_Zone_Free (progs_t *pr, void *ptr);
void *PR_Zone_Malloc (progs_t *pr, int size);
void *PR_Zone_Realloc (progs_t *pr, void *ptr, int size);

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
void PR_StackTrace (progs_t *pr);

extern struct cvar_s *pr_debug;
extern struct cvar_s *pr_deadbeef_ents;
extern struct cvar_s *pr_deadbeef_locals;
extern struct cvar_s *pr_boundscheck;
extern struct cvar_s *pr_faultchecks;

extern const char *pr_gametype;

//
// PR Cmds stuff
//

char *PF_VarString (progs_t *pr, int first);
void PR_Cmds_Init (progs_t *pr);

//============================================================================

#define MAX_STACK_DEPTH		64
#define LOCALSTACK_SIZE		4096
#define PR_RS_SLOTS			16

typedef struct strref_s strref_t;

typedef struct {
	int         s;
	dfunction_t *f;
	strref_t   *tstr;
} prstack_t;

struct progs_s {
	const char *progs_name;
	dprograms_t *progs;
	int         progs_size;

	struct memzone_s *zone;
	int         zone_size;

	struct hashtab_s *builtin_hash;
	struct hashtab_s *builtin_num_hash;
	unsigned    bi_next;
	unsigned  (*bi_map) (progs_t *pr, unsigned binum);

	struct hashtab_s *function_hash;
	struct hashtab_s *global_hash;
	struct hashtab_s *field_hash;

	int         num_load_funcs;
	int         max_load_funcs;
	pr_load_func_t **load_funcs;

	// cleared each load
	int         num_load_finish_funcs;
	int         max_load_finish_funcs;
	pr_load_func_t **load_finish_funcs;

	struct dstring_mem_s *ds_mem;
	strref_t   *free_string_refs;
	strref_t   *static_strings;
	strref_t  **string_map;
	strref_t   *return_strings[PR_RS_SLOTS];
	int         rs_slot;
	unsigned    dyn_str_size;
	struct hashtab_s *strref_hash;
	int         num_strings;
	strref_t   *pr_xtstr;

	dfunction_t *pr_functions;
	char       *pr_strings;
	int         pr_stringsize;
	ddef_t     *pr_globaldefs;
	ddef_t     *pr_fielddefs;
	dstatement_t *pr_statements;
	pr_type_t  *pr_globals;			// same as pr_global_struct
	int         globals_size;

	pr_type_t  *pr_return;
	pr_type_t  *pr_params[MAX_PARMS];
	int         pr_param_size;		// covers both params and return

	int         pr_edict_size;	// in bytes
	int         pr_edictareasize; // for bounds checking, starts at 0

	int         pr_argc;

	qboolean    pr_trace;
	dfunction_t *pr_xfunction;
	int         pr_xstatement;

	prstack_t   pr_stack[MAX_STACK_DEPTH];
	int         pr_depth;

	int         localstack[LOCALSTACK_SIZE];
	int         localstack_used;

	edict_t   **edicts;
	int        *num_edicts;
	int        *reserved_edicts;	//alloc will start at reserved_edicts+1
	double     *time;
	int         null_bad;
	int         no_exec_limit;

	unsigned short crc;

	void      (*unlink) (edict_t *ent);
	void      (*flush) (void);

	int       (*parse_field) (progs_t *pr, const char *key, const char *value);
	int       (*prune_edict) (progs_t *pr, edict_t *ent);
	void      (*free_edict) (progs_t *pr, edict_t *ent);

	void      (*file_error) (progs_t *pr, const char *path);
	void     *(*load_file) (progs_t *pr, const char *path);
	void     *(*allocate_progs_mem) (progs_t *pr, int size);
	void      (*free_progs_mem) (progs_t *pr, void *mem);

	int       (*resolve) (progs_t *pr);

	pr_resource_t *resources;
	struct hashtab_s *resource_hash;

	// obj info
	struct hashtab_s *selectors;
	struct hashtab_s *classes;
	struct hashtab_s *categories;
	struct hashtab_s *protocols;

	// debug info
	const char *debugfile;
	struct pr_debug_header_s *debug;
	struct pr_auxfunction_s *auxfunctions;
	struct pr_auxfunction_s **auxfunction_map;
	struct pr_lineno_s *linenos;
	ddef_t     *local_defs;

	// required globals
	struct {
		float      *time;
		int        *self;
	} globals;
	// required fields (offsets into the edict)
	struct {
		int         nextthink;
		int         frame;
		int         think;
		int         this;
	} fields;
};

static inline pr_type_t *
PR_GetPointer (progs_t *pr, int o)
{
	return o ? pr->pr_globals + o : 0;
}

static inline pointer_t
PR_SetPointer (progs_t *pr, void *p)
{
	return p ? (pr_type_t *) p - pr->pr_globals : 0;
}

#endif//__QF_progs_h
