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

/** \defgroup progs QuakeC Virtual Machine (VM)
	\image html vm-mem.png
	\image latex vm-mem.eps "VM memory map"
*/

#include "QF/link.h"
#include "QF/pr_comp.h"
#include "QF/pr_debug.h"
#include "QF/quakeio.h"

/** \ingroup progs */
//@{
typedef struct progs_s progs_t;
typedef struct pr_resource_s pr_resource_t;
typedef struct edict_s edict_t;
//@}

//============================================================================

/** \defgroup progs_misc Miscelaneous functions
	\ingroup progs
*/
//@{

/** Initialize the progs engine.
*/
void PR_Init (void);

/** Initialize the Cvars for the progs engine. Call before calling PR_Init().
*/
void PR_Init_Cvars (void);

void PR_Error (progs_t *pr, const char *error, ...) __attribute__((format(printf,2,3), noreturn));
void PR_RunError (progs_t *pr, const char *error, ...) __attribute__((format(printf,2,3), noreturn));

//@}

/** \defgroup progs_execution Execution
	\ingroup progs
*/
//@{

/** Ensure P_* macros point to the right place for passing parameters to progs
	functions.
	\param pr pointer to ::progs_t VM struct
	\warning failure to use this macro before assigning to the P_* macros can
	cause corruption of the VM data due to "register" based calling. Can be
	safely ignored for parameterless functions, or forwardning parameters
	though a builtin.

	\hideinitializer
*/
#define PR_RESET_PARAMS(pr)							\
	do {											\
		(pr)->pr_params[0] = (pr)->pr_real_params[0];	\
		(pr)->pr_params[1] = (pr)->pr_real_params[1];	\
	} while (0)

/** Push an execution frame onto the VM stack. Saves current execution state.
	\param pr pointer to ::progs_t VM struct
*/
void PR_PushFrame (progs_t *pr);

/** Pop an execution frame from the VM stack. Restores execution state. Also
	frees any temporary strings allocated in this frame (via
	PR_FreeTempStrings()).
	\param pr pointer to ::progs_t VM struct
*/
void PR_PopFrame (progs_t *pr);

/** Run a progs function. if \p fnum is a builtin rather than a progs
	function, PR_ExecuteProgram() will call the function and then immediately
	return to the caller. Nested calls are fully supported.
	\param pr pointer to ::progs_t VM struct
	\param fnum number of the function to call
*/
void PR_ExecuteProgram (progs_t *pr, func_t fnum);

/** Setup to call a function. If \p fnum is a builtin rather than a progs
	function, then the function is called immediately. When called from a
	builtin function, the progs function will execute upon return of control
	to PR_ExecuteProgram().
	\param pr pointer to ::progs_t VM struct
	\param fnum number of the function to call
	\return true if \p fnum was a progs function, false if \p fnum was a
	builtin
*/
int PR_CallFunction (progs_t *pr, func_t fnum);

//@}

/** \defgroup progs_load Loading
	\ingroup progs
*/
//@{

/** Type of functions that are called at progs load.
	\param pr pointer to ::progs_t VM struct
	\return true for success, false for failure
*/
typedef int pr_load_func_t (progs_t *pr);

/** Initialize a ::progs_t VM struct from an already open file.
	\param pr pointer to ::progs_t VM struct
	\param file handle of file to read progs data from
	\param size bytes of \p file to read
	\param edicts \e number of entities to allocate space for
	\param zone minimum size of dynamic memory to allocate space for

	\note \e All runtime strings (permanent or temporary) are allocated from
	the VM's dynamic memory space, so be sure \p zone is of sufficient size.
	So far, 1MB has proven more than sufficient for Quakeword, even when using
	Ruamoko objects.
*/
void PR_LoadProgsFile (progs_t *pr, QFile *file, int size, int edicts,
					   int zone);

/** Convenience wrapper for PR_LoadProgsFile() and PR_RunLoadFuncs().
	Searches for the specified file in the Quake filesystem.
	\param pr pointer to ::progs_t VM struct
	\param progsname name of the file to load as progs data
	\param edicts \e number of entities to allocate space for
	\param zone minimum size of dynamic memory to allocate space for
*/
void PR_LoadProgs (progs_t *pr, const char *progsname, int edicts, int zone);

/** Register a primary function to be called after the progs code has been
	loaded. These functions are remembered across progs loads. They will be
	called in order of registration.
	\param pr pointer to ::progs_t VM struct
	\param func function to call
*/
void PR_AddLoadFunc (progs_t *pr, pr_load_func_t *func);

/** Register a secondary function to be called after the progs code has been
	loaded. These functions will be forgotten after each load. They will be
	called in \e reverse order of being registered. 
	\param pr pointer to ::progs_t VM struct
	\param func function to call
*/
void PR_AddLoadFinishFunc (progs_t *pr, pr_load_func_t *func);

/** Run all load functions. Any load function returning false will abort the
	load operation.
	\param pr pointer to ::progs_t VM struct

	Calls the first set of internal load functions followed by the supplied
	symbol resolution function, if any (progs_t::resolve), the second set of
	internal load functions. After that, any primary load functions are called
	in order of registration followed by any \c .ctor functions in the progs,
	then any secondary load functions the primary load functions registered
	in \e reverse order of registration.
*/
int PR_RunLoadFuncs (progs_t *pr);

/** Validate the opcodes and statement addresses in the progs. This is an
	internal load function.
	\param pr pointer to ::progs_t VM struct

	\todo should this be elsewhere?
*/
int PR_Check_Opcodes (progs_t *pr);

//@}

/** \defgroup progs_edict Edict management
	\ingroup progs
*/
//@{

#define MAX_ENT_LEAFS	16
struct edict_s {
	qboolean    free;
	link_t      area;			///< linked to a division node or leaf

	int         num_leafs;
	short       leafnums[MAX_ENT_LEAFS];

	float       freetime;		///< sv.time when the object was freed
	void       *data;			///< external per-edict data
	pr_type_t   v[1];			///< fields from progs
};
#define EDICT_FROM_AREA(l) STRUCT_FROM_LINK(l,edict_t,area)

// pr_edict.c
void ED_ClearEdict (progs_t *pr, edict_t *e, int val);
edict_t *ED_Alloc (progs_t *pr);
void ED_Free (progs_t *pr, edict_t *ed);
edict_t *ED_EdictNum(progs_t *pr, int n);
int ED_NumForEdict(progs_t *pr, edict_t *e);
void ED_Count (progs_t *pr);
qboolean PR_EdictValid (progs_t *pr, int e);

// pr_debug.c
void ED_Print (progs_t *pr, edict_t *ed);
void ED_PrintEdicts (progs_t *pr, const char *fieldval);
void ED_PrintNum (progs_t *pr, int ent);

// pr_parse.c
struct script_s;
qboolean ED_ParseEpair (progs_t *pr, pr_type_t *base, ddef_t *key,
						const char *s);
void ED_Write (progs_t *pr, QFile *f, edict_t *ed);
void ED_WriteGlobals (progs_t *pr, QFile *f);
void ED_ParseEdict (progs_t *pr, struct script_s *script, edict_t *ent);
void ED_ParseGlobals (progs_t *pr, struct script_s *script);
void ED_LoadFromFile (progs_t *pr, const char *data);
void ED_EntityParseFunction (progs_t *pr);

#define PR_edicts(p)		((byte *) *(p)->edicts)

#define NEXT_EDICT(p,e)		((edict_t *) ((byte *) e + (p)->pr_edict_size))
#define EDICT_TO_PROG(p,e)	((long) ((byte *) (e) - PR_edicts (p)))
#define PROG_TO_EDICT(p,e)	((edict_t *) (PR_edicts (p) + (e)))
#define NUM_FOR_BAD_EDICT(p,e) (EDICT_TO_PROG (p, e) / (p)->pr_edict_size)
#ifndef PR_PARANOID_PROGS
# define EDICT_NUM(p,n)		(PROG_TO_EDICT (p, (n) * (p)->pr_edict_size))
# define NUM_FOR_EDICT(p,e)	NUM_FOR_BAD_EDICT (p, e)
#else
# define EDICT_NUM(p,n)		ED_EdictNum (p, n)
# define NUM_FOR_EDICT(p,e)	ED_NumForEdict (p, e)
#endif

//@}

/** \defgroup pr_symbols Symbol Management
	\ingroup progs
	Lookup functions for symbol name resolution.
*/
//@{

ddef_t *PR_FieldAtOfs (progs_t *pr, int ofs);
ddef_t *PR_GlobalAtOfs (progs_t *pr, int ofs);

ddef_t *PR_FindField (progs_t *pr, const char *name);
ddef_t *PR_FindGlobal (progs_t *pr, const char *name);
dfunction_t *PR_FindFunction (progs_t *pr, const char *name);

int PR_ResolveGlobals (progs_t *pr);

int PR_AccessField (progs_t *pr, const char *name, etype_t type,
					const char *file, int line);

void PR_Undefined (progs_t *pr, const char *type, const char *name) __attribute__((noreturn));
//@}

//============================================================================

/** \defgroup progs_data_access Data Access
	\ingroup progs
	Macros for accessing data in the VM address space
*/

/** \defgroup prda_globals Globals
	\ingroup progs_data_access
	Typed global access marcos. No checking is done against the QC type, but
	the appropriate C type will be used.
*/
//@{

/** \internal
	\param p pointer to ::progs_t VM struct
	\param o offset into global data space
	\param t typename prefix (see pr_type_u)
	\return lvalue of the appropriate type

	\hideinitializer
*/
#define G_var(p,o,t)	((p)->pr_globals[o].t##_var)

/** Access a float global. Can be assigned to.

	\par QC type:
		\c float
	\param p pointer to ::progs_t VM struct
	\param o offset into global data space
	\return float lvalue

	\hideinitializer
*/
#define G_FLOAT(p,o)	G_var (p, o, float)

/** Access a integer global. Can be assigned to.

	\par QC type:
		\c integer
	\param p pointer to ::progs_t VM struct
	\param o offset into global data space
	\return int lvalue

	\hideinitializer
*/
#define G_INT(p,o)		G_var (p, o, integer)

/** Access a unsigned integer global. Can be assigned to.

	\par QC type:
		\c uinteger
	\param p pointer to ::progs_t VM struct
	\param o offset into global data space
	\return unsigned int lvalue

	\hideinitializer
*/
#define G_UINT(p,o)		G_var (p, o, uinteger)

/** Access a vector global. Can be assigned to.

	\par QC type:
		\c vector
	\param p pointer to ::progs_t VM struct
	\param o offset into global data space
	\return vec3_t lvalue

	\hideinitializer
*/
#define G_VECTOR(p,o)	G_var (p, o, vector)

/** Access a string index global. Can be assigned to.

	\par QC type:
		\c string
	\param p pointer to ::progs_t VM struct
	\param o offset into global data space
	\return string_t lvalue

	\hideinitializer
*/
#define G_STRING(p,o)	G_var (p, o, string)

/** Access a function global. Can be assigned to.

	\par QC type:
		\c void()
	\param p pointer to ::progs_t VM struct
	\param o offset into global data space
	\return func_t lvalue

	\hideinitializer
*/
#define G_FUNCTION(p,o)	G_var (p, o, func)

/** Access a pointer global. Can be assigned to.

	\par QC type:
		\c void[]
	\param p pointer to ::progs_t VM struct
	\param o offset into global data space
	\return pointer_t lvalue

	\hideinitializer
*/
#define G_POINTER(p,o)	G_var (p, o, pointer)


/** Access an entity global.

	\par QC type:
		\c entity
	\param p pointer to ::progs_t VM struct
	\param o offset into global data space
	\return C pointer to the entity

	\hideinitializer
*/
#define G_EDICT(p,o)	((edict_t *)(PR_edicts (p) + G_INT (p, o)))

/** Access an entity global.

	\par QC type:
		\c entity
	\param p pointer to ::progs_t VM struct
	\param o offset into global data space
	\return the entity number

	\hideinitializer
*/
#define G_EDICTNUM(p,o)	NUM_FOR_EDICT(p, G_EDICT (p, o))

/** Access a a string global, converting it to a C string. Kills the program
	if the string is invalid.

	\par QC type:
		\c string
	\param p pointer to ::progs_t VM struct
	\param o offset into global data space
	\return const char * to the string

	\hideinitializer
*/
#define G_GSTRING(p,o)	PR_GetString (p, G_STRING (p, o))

/** Access a dstring global. Kills the program if the dstring is invalid.

	\par QC type:
		\c string
	\param p pointer to ::progs_t VM struct
	\param o offset into global data space
	\return dstring_t * to the dstring

	\hideinitializer
*/
#define G_DSTRING(p,o)	PR_GetMutableString (p, G_STRING (p, o))

/** Access a pointer parameter.

	\par QC type:
		\c void[]
	\param p pointer to ::progs_t VM struct
	\param o offset into global data space
	\return C pointer represented by the parameter. 0 offset -> NULL

	\hideinitializer
*/
#define G_GPOINTER(p,o)	PR_GetPointer (p, o)

/** Access an entity global.

	\par QC type:
		\c void[]
	\param p pointer to ::progs_t VM struct
	\param t C type of the structure
	\param o offset into global data space
	\return structure lvalue. use & to make a pointer of the appropriate type.

	\hideinitializer
*/
#define G_STRUCT(p,t,o)	(*(t *)G_GPOINTER (p, o))
//@}

/** \defgroup prda_parameters Parameters
	\ingroup progs_data_access
	Typed parameter access marcos. No checking is done against the QC type, but
	the appropriate C type will be used.
*/
//@{

/** \internal
	\param p pointer to ::progs_t VM struct
	\param n parameter number (0-7)
	\param t typename prefix (see pr_type_u)
	\return lvalue of the appropriate type

	\hideinitializer
*/
#define P_var(p,n,t)	((p)->pr_params[n]->t##_var)

/** Access a float parameter. Can be assigned to.

	\par QC type:
		\c float
	\param p pointer to ::progs_t VM struct
	\param n parameter number (0-7)
	\return float lvalue

	\hideinitializer
*/
#define P_FLOAT(p,n)	P_var (p, n, float)

/** Access a integer parameter. Can be assigned to.

	\par QC type:
		\c integer
	\param p pointer to ::progs_t VM struct
	\param n parameter number (0-7)
	\return int lvalue

	\hideinitializer
*/
#define P_INT(p,n)		P_var (p, n, integer)

/** Access a unsigned integer parameter. Can be assigned to.

	\par QC type:
		\c uinteger
	\param p pointer to ::progs_t VM struct
	\param n parameter number (0-7)
	\return unsigned int lvalue

	\hideinitializer
*/
#define P_UINT(p,n)		P_var (p, n, uinteger)

/** Access a vector parameter. Can be used any way a vec3_t variable can.

	\par QC type:
		\c vector
	\param p pointer to ::progs_t VM struct
	\param n parameter number (0-7)
	\return vec3_t lvalue

	\hideinitializer
*/
#define P_VECTOR(p,n)	P_var (p, n, vector)

/** Access a string index parameter. Can be assigned to.

	\par QC type:
		\c string
	\param p pointer to ::progs_t VM struct
	\param n parameter number (0-7)
	\return string_t lvalue

	\hideinitializer
*/
#define P_STRING(p,n)	P_var (p, n, string)

/** Access a function parameter. Can be assigned to.

	\par QC type:
		\c void()
	\param p pointer to ::progs_t VM struct
	\param n parameter number (0-7)
	\return func_t lvalue

	\hideinitializer
*/
#define P_FUNCTION(p,n)	P_var (p, n, func)

/** Access a pointer parameter. Can be assigned to.

	\par QC type:
		\c void[]
	\param p pointer to ::progs_t VM struct
	\param n parameter number (0-7)
	\return pointer_t lvalue

	\hideinitializer
*/
#define P_POINTER(p,n)	P_var (p, n, pointer)


/** Access an entity parameter.

	\par QC type:
		\c entity
	\param p pointer to ::progs_t VM struct
	\param n parameter number (0-7)
	\return C pointer to the entity

	\hideinitializer
*/
#define P_EDICT(p,n)	((edict_t *)(PR_edicts (p) + P_INT (p, n)))

/** Access a entity parameter.

	\par QC type:
		\c entity
	\param p pointer to ::progs_t VM struct
	\param n parameter number (0-7)
	\return the entity number

	\hideinitializer
*/
#define P_EDICTNUM(p,n)	NUM_FOR_EDICT (p, P_EDICT (p, n))

/** Access a string parameter, converting it to a C string. Kills the program
	if the string is invalid.

	\par QC type:
		\c string
	\param p pointer to ::progs_t VM struct
	\param n parameter number (0-7)
	\return const char * to the string

	\hideinitializer
*/
#define P_GSTRING(p,n)	PR_GetString (p, P_STRING (p, n))

/** Access a dstring parameter. Kills the program if the dstring is invalid.

	\par QC type:
		\c string
	\param p pointer to ::progs_t VM struct
	\param n parameter number (0-7)
	\return dstring_t * to the dstring

	\hideinitializer
*/
#define P_DSTRING(p,n)	PR_GetMutableString (p, P_STRING (p, n))

/** Access a pointer parameter.

	\par QC type:
		\c void[]
	\param p pointer to ::progs_t VM struct
	\param n parameter number (0-7)
	\return C pointer represented by the parameter. 0 offset -> NULL

	\hideinitializer
*/
#define P_GPOINTER(p,n)	PR_GetPointer (p, P_POINTER (p, n))

/** Access a structure pointer parameter.

	\par QC type:
		\c void[]
	\param p pointer to ::progs_t VM struct
	\param t C type of the structure
	\param n parameter number (0-7)
	\return structure lvalue. use & to make a pointer of the appropriate type.

	\hideinitializer
*/
#define P_STRUCT(p,t,n)	(*(t *)P_GPOINTER (p, n))
//@}

/** \defgroup prda_return Return value
	\ingroup progs_data_access
	Typed return value access marcos. No checking is done against the QC type,
	but the appropriate C type will be used.
*/
//@{

/** \internal
	\param p pointer to ::progs_t VM struct
	\param t typename prefix (see pr_type_u)
	\return lvalue of the appropriate type

	\hideinitializer
*/
#define R_var(p,t)		((p)->pr_return->t##_var)

/** Access the float return value.

	\par QC type:
		\c float
	\param p pointer to ::progs_t VM struct
	\return float lvalue

	\hideinitializer
*/
#define R_FLOAT(p)		R_var (p, float)

/** Access the integer return value.

	\par QC type:
		\c integer
	\param p pointer to ::progs_t VM struct
	\return int lvalue

	\hideinitializer
*/
#define R_INT(p)		R_var (p, integer)

/** Access the unsigned integer return value.

	\par QC type:
		\c uinteger
	\param p pointer to ::progs_t VM struct
	\return unsigned int lvalue

	\hideinitializer
*/
#define R_UINT(p)		R_var (p, uinteger)

/** Access the vector return value.

	\par QC type:
		\c vector
	\param p pointer to ::progs_t VM struct
	\return vec3_t lvalue

	\hideinitializer
*/
#define R_VECTOR(p)		R_var (p, vector)

/** Access the string index return value.

	\par QC type:
		\c string
	\param p pointer to ::progs_t VM struct
	\return string_t lvalue

	\hideinitializer
*/
#define R_STRING(p)		R_var (p, string)

/** Access the function return value.

	\par QC type:
		\c void()
	\param p pointer to ::progs_t VM struct
	\return func_t lvalue

	\hideinitializer
*/
#define R_FUNCTION(p)	R_var (p, func)

/** Access the pointer return value.

	\par QC type:
		\c void[]
	\param p pointer to ::progs_t VM struct
	\return pointer_t lvalue

	\hideinitializer
*/
#define R_POINTER(p)	R_var (p, pointer)


/** Set the return value to the given C string. The returned string will
	eventually be garbage collected (see PR_SetReturnString()).

	\par QC type:
		\c string
	\param p pointer to ::progs_t VM struct
	\param s C string to be returned

	\hideinitializer
*/
#define RETURN_STRING(p,s)		(R_STRING (p) = PR_SetReturnString((p), s))

/** Set the return value to the given C entity pointer. The pointer is
	converted into a progs entity address.

	\par QC type:
		\c entity
	\param p pointer to ::progs_t VM struct
	\param e C entity pointer to be returned

	\hideinitializer
*/
#define RETURN_EDICT(p,e)		(R_STRING (p) = EDICT_TO_PROG(p, e))

/** Set the return value to the given C pointer. NULL is converted to 0.

	\par QC type:
		\c void[]
	\param p pointer to ::progs_t VM struct
	\param ptr C entity pointer to be returned

	\hideinitializer
*/
#define RETURN_POINTER(p,ptr)	(R_POINTER (p) = PR_SetPointer (p, ptr))

/** Set the return value to the given vector. The pointer is
	converted into a progs entity address.

	\par QC type:
		\c vector
	\param p pointer to ::progs_t VM struct
	\param v C entity pointer to be returned

	\hideinitializer
*/
#define RETURN_VECTOR(p,v)		VectorCopy (v, R_VECTOR (p))
//@}

/** \defgroup prda_entity_fields Entity Fields
	\ingroup progs_data_access
	Typed entity field  access marcos. No checking is done against the QC type,
	but the appropriate C type will be used.
*/
//@{

/** \internal
	\param e pointer to the entity
	\param o field offset into entity data space
	\param t typename prefix (see pr_type_u)
	\return lvalue of the appropriate type

	\hideinitializer
*/
#define E_var(e,o,t)	((e)->v[o].t##_var)


/** Access a float entity field. Can be assigned to.

	\par QC type:
		\c float
	\param e pointer to the entity
	\param o field offset into entity data space
	\return float lvalue

	\hideinitializer
*/
#define E_FLOAT(e,o)	E_var (e, o, float)

/** Access a integer entity field. Can be assigned to.

	\par QC type:
		\c integer
	\param e pointer to the entity
	\param o field offset into entity data space
	\return int lvalue

	\hideinitializer
*/
#define E_INT(e,o)		E_var (e, o, integer)

/** Access a unsigned integer entity field. Can be assigned to.

	\par QC type:
		\c uinteger
	\param e pointer to the entity
	\param o field offset into entity data space
	\return unsigned int lvalue

	\hideinitializer
*/
#define E_UINT(e,o)		E_var (e, o, uinteger)

/** Access a vector entity field. Can be used any way a vec3_t variable can.

	\par QC type:
		\c vector
	\param e pointer to the entity
	\param o field offset into entity data space
	\return vec3_t lvalue

	\hideinitializer
*/
#define E_VECTOR(e,o)	E_var (e, o, vector)

/** Access a string index entity field. Can be assigned to.

	\par QC type:
		\c string
	\param e pointer to the entity
	\param o field offset into entity data space
	\return string_t lvalue

	\hideinitializer
*/
#define E_STRING(e,o)	E_var (e, o, string)

/** Access a function entity field. Can be assigned to.

	\par QC type:
		\c void()
	\param e pointer to the entity
	\param o field offset into entity data space
	\return func_t lvalue

	\hideinitializer
*/
#define E_FUNCTION(e,o)	E_var (e, o, func)

/** Access a pointer entity field. Can be assigned to.

	\par QC type:
		\c void[]
	\param e pointer to the entity
	\param o field offset into entity data space
	\return pointer_t lvalue

	\hideinitializer
*/
#define E_POINTER(e,o)	E_var (e, o, pointer)


/** Access a string entity field, converting it to a C string. Kills the
	program if the string is invalid.

	\par QC type:
		\c string
	\param p pointer to ::progs_t VM struct
	\param e pointer to the entity
	\param o field offset into entity data space
	\return const char * to the string

	\hideinitializer
*/
#define E_GSTRING(p,e,o) (PR_GetString (p, E_STRING (e, o)))

/** Access a dstring entity field. Kills the program if the dstring is invalid.

	\par QC type:
		\c string
	\param p pointer to ::progs_t VM struct
	\param e pointer to the entity
	\param o field offset into entity data space
	\return dstring_t * to the dstring

	\hideinitializer
*/
#define E_DSTRING(p,e,o) (PR_GetMutableString (p, E_STRING (e, o)))
//@}

/** \defgroup pr_builtins VM Builtin functions
	\ingroup progs
	Builtin management functions.

	Builtins are arranged into groups of 65536 to allow for diffent expantion
	sets. eg, stock id is 0x0000xxxx, quakeforge is 0x000fxxxx. predefined
	groups go up to 0x0fffxxxx allowing 4096 different groups. Builtins
	0x10000000 to 0x7fffffff are reserved for auto-allocation. The range
	0x8000000 to 0xffffffff is unavailable due to the builtin number being
	a negative statement address.
*/
//@{

#define PR_RANGE_SHIFT	16
#define PR_RANGE_MASK	0xffff0000

#define PR_RANGE_QF		0x000f

#define PR_RANGE_AUTO	0x1000
#define PR_RANGE_MAX	0x7fff
#define PR_RANGE_NONE	0xffff
#define PR_AUTOBUILTIN	(PR_RANGE_AUTO << PR_RANGE_SHIFT)

typedef void (*builtin_proc) (progs_t *pr);

/** Create a static array of these and pass the array to PR_RegisterBuiltins()
	to register the module's QC builtin functions.
*/
typedef struct {
	/// QC name of the builtin. Must be an exact match for automaticly resolved
	/// builtins (func = \#0 in QC). NULL indicates end of array for
	/// PR_RegisterBuiltins()
	const char *name;
	/// Pointer to the C implementation of the builtin function.
	builtin_proc proc;
	/// The number of the builtin for \#N in QC. -1 for automatic allocation.
	/// 0 or >= ::PR_AUTOBUILTIN is invalid.
	int         binum;
} builtin_t;

/** Aliased over the dfunction_t entry for builtin functions. Removes one
	level of indirection when calling a builtin function.
	\bug alignment access violations on 64 bit architectures (eg, alpha)
*/
typedef struct {
	int         first_statement;
	builtin_proc func;
} bfunction_t;

/** Register a set of builtin functions with the VM. Different VMs within the
	same program can have different builtin sets.
	\param pr pointer to ::progs_t VM struct
	\param builtins array of builtin_t builtins
*/
void PR_RegisterBuiltins (progs_t *pr, builtin_t *builtins);

/** Lookup a builtin function referred by name.
	\param pr pointer to ::progs_t VM struct
	\param name name of the builtin function to lookup
	\return pointer to the builtin function entry, or NULL if not found
*/
builtin_t *PR_FindBuiltin (progs_t *pr, const char *name);

/** Lookup a builtin function by builtin number.
	\param pr pointer to ::progs_t VM struct
	\param num number of the builtin function to lookup
	\return pointer to the builtin function entry, or NULL if not found
*/
builtin_t *PR_FindBuiltinNum (progs_t *pr, int num);

/** Fixup all automaticly resolved builtin functions (func = #0 in QC).
	Performs any necessary builtin function number mapping. Also edits the
	dfunction_t entry for all builtin functions to contain the function
	pointer to the C implementation of the builtin function. Automaticly
	during progs load.
	\bug alignment access violations on 64 bit architectures (eg, alpha)
	\param pr pointer to ::progs_t VM struct
*/
int PR_RelocateBuiltins (progs_t *pr);

//@}

/** \defgroup pr_strings String Management
	\ingroup progs
	Strings management functions.

	All strings accessable by the VM are stored within the VM address space.
	These functions provide facilities to set permanent, dynamic  and
	temporary strings, as well as mutable strings using dstrings.

	Permanent strings are either supplied by the progs (+ve string index) or
	set the the main program using PR_SetString(). Permanent strings can never
	be altered and will exist until the next progs load.

	Dynamic strings can be freed at any time, but not altered.

	Temporary strings are always set by the main program using
	PR_SetTempString() or PR_CatStrings(). They will be freed when the
	current progs stack frame is cleared. Use PR_PushFrame() and PR_PopFrame()
	around the calls to PR_SetTempString() and PR_ExecuteProgram() when using
	strings as parameters to a progs function.

	Mutable strings are dstrings (dstring_t) mapped into the VM address space.
	They can be created, altered, and destroyed at any time by the main
	program (or the progs code via an appropriate builtin function).
*/
//@{

/** Initialize the string tables using the strings supplied by the progs.
	Automaticly called at progs load.
	\param pr pointer to ::progs_t VM struct
*/
int PR_LoadStrings (progs_t *pr);

/** Check the validity of a string index.
	\param pr pointer to ::progs_t VM struct
	\param num string index to be validated
	\return true if the index is valid, false otherwise
*/
qboolean PR_StringValid (progs_t *pr, string_t num);

/** Convert a string index to a C string.
	\param pr pointer to ::progs_t VM struct
	\param num string index to be converted
	\return C pointer to the string.
*/
const char *PR_GetString(progs_t *pr, string_t num);

/** Retieve the dstring_t associated with a mutable string.
	\param pr pointer to ::progs_t VM struct
	\param num string index of the mutable string
	\return the dstring implementing the mutable string
*/
struct dstring_s *PR_GetMutableString(progs_t *pr, string_t num);

/** Make a permanent progs string from the given C string. Will not create a
	duplicate permanent string (temporary and mutable strings are not checked).
	\param pr pointer to ::progs_t VM struct
	\param s C string to be made into a permanent progs string
	\return string index of the progs string
*/
string_t PR_SetString(progs_t *pr, const char *s);

/** Make a temporary progs string that will survive across function returns.
	Will not duplicate a permanent string. If a new progs string is created,
	it will be freed after ::PR_RS_SLOTS calls to this function. ie, up to
	::PR_RS_SLOTS return strings can exist at a time.
	\param pr pointer to ::progs_t VM struct
	\param s C string to be returned to the progs code
	\return string index of the progs string
*/
string_t PR_SetReturnString(progs_t *pr, const char *s);

/** Make a temporary progs string that will be freed when the current progs
	stack frame is exited. Will not duplicate a permantent string.
	\param pr pointer to ::progs_t VM struct
	\param s C string
	\return string index of the progs string
*/
string_t PR_SetTempString(progs_t *pr, const char *s);

/** Make a tempoary progs string that is the concatenation of two C strings.
	\param pr pointer to ::progs_t VM struct
	\param a C string
	\param b C string
	\return string index of the progs string that represents the concatenation
			of strings a and b
*/
string_t PR_CatStrings (progs_t *pr, const char *a, const char *b);

/** Convert a mutable string to a temporary string.
	\param pr pointer to ::progs_t VM struct
	\param str string index of the mutable string to be converted
*/
void PR_MakeTempString(progs_t *pr, string_t str);

/** Create a new mutable string.
	\param pr pointer to ::progs_t VM struct
	\return string index of the newly created mutable string
*/
string_t PR_NewMutableString (progs_t *pr);

/** Make a dynamic progs string from the given C string. Will not create a
	duplicate permanent string (temporary, dynamic  and mutable strings are
	not checked).
	\param pr pointer to ::progs_t VM struct
	\param s C string to be made into a permanent progs string
	\return string index of the progs string
*/
string_t PR_SetDynamicString (progs_t *pr, const char *s);

/** Clear all of the return string slots. Called at progs load.
	\param pr pointer to ::progs_t VM struct
*/
void PR_ClearReturnStrings (progs_t *pr);

/** Destroy a mutable, dynamic or temporary string.
	\param pr pointer to ::progs_t VM struct
	\param str string index of the string to be destroyed
*/
void PR_FreeString (progs_t *pr, string_t str);

/** Free all the temporary strings allocated in the current stack frame.
	\param pr pointer to ::progs_t VM struct
*/
void PR_FreeTempStrings (progs_t *pr);

/** Formatted printing similar to C's vsprintf, but using QC types.
	The format string is a string of characters (other than \c \%) to be printed
	that includes optional format specifiers, one for each arg to be printed.
	A format specifier can be one of two forms:
	<ul>
	<li>\c "%%"	print a single \c '\%'
	<li><tt>%[flags][field width][precision](conversion specifier)</tt>	print a
			single arg of the type specified by the conversion specifier using
			the given format.
	</ul>

	<ul>
	<li>flags (optional)
		<ul>
		<li>\c '#'	print the value using an alternat form
		<li>\c '0'	the value should be zero padded
		<li>\c '-'	the value is to be left adjusted on the field bondary.
		<li><tt>' '</tt>	A blank should be left before a positve number (or
					empty string) produced by a signed conversion.
		<li>\c '+'	A sign (+ or -) will always be placed before a number
					produced by a signed conversion.
		</ul>
	<li>field width (optional)
		<ul>
		<li>a string of decimal digits with nonzero first digit.
		</ul>
	<li>precision (optional)
		<ul>
		<li>a string consisting of <tt>'.'</tt> followed by 0 or more decimal
			digits.
		</ul>
	<li>conversion specifier (mandatory)
		<ul>
		<li>\c '@'	\c id Not yet implemented. Silently ignored.
		<li>\c 'e'	\c entity Prints the edict number of the given entity ("%i")
		<li>\c 'i'	\c integer Print a integer value. ("%i")
		<li>\c 'f'	\c float Print a float value. ("%f")
		<li>\c 'g'	\c float Print a float value. ("%f")
		<li>\c 'p'	\c void[] Print a pointer value. ("%#x")
		<li>\c 's'	\c string Print a string value. ("%s")
		<li>\c 'v'	\c vector Print a vector value. ("'%g %g %g'")
		<li>\c 'q'	\c quaternion Print a quaternion value. ("'%g %g %g %g'")
		<li>\c 'x'	\c uinteger Print an unsigned integer value. ("%x")
		</ul>
	</ul>

	For details, refer to the C documentation for printf.

	\param pr pointer to ::progs_t VM struct
	\param result dstring to which printing is done
	\param name name to be reported in case of errors
	\param format the format string
	\param count number of args
	\param args array of pointers to the values to be printed
*/
void PR_Sprintf (progs_t *pr, struct dstring_s *result, const char *name,
				 const char *format, int count, pr_type_t **args);

//@}

/** \defgroup pr_resources Resource Management
	\ingroup progs
	Builtin module private data management.
*/
//@{

void PR_Resources_Init (progs_t *pr);
void PR_Resources_Clear (progs_t *pr);
void PR_Resources_Register (progs_t *pr, const char *name, void *data,
							void (*clear)(progs_t *, void *));
void *PR_Resources_Find (progs_t *pr, const char *name);

#define PR_RESMAP(type) struct { type *_free; type **_map; unsigned _size; }

#define PR_RESNEW(type,map)									\
	type       *t;											\
															\
	if (!map._free) {										\
		int         i, size;								\
		map._size++;										\
		size = map._size * sizeof (type *);					\
		map._map = realloc (map._map, size);				\
		if (!map._map)										\
			return 0;										\
		map._free = calloc (1024, sizeof (type));			\
		if (!map._free)										\
			return 0;										\
		map._map[map._size - 1] = map._free;				\
		for (i = 0; i < 1023; i++)							\
			*(type **) &map._free[i] = &map._free[i + 1];	\
		*(type **) &map._free[i] = 0;						\
	}														\
	t = map._free;											\
	map._free = *(type **) t;								\
	memset (t, 0, sizeof (type));							\
	return t

#define PR_RESFREE(type,map,t)								\
	memset (t, 0, sizeof (type));							\
	*(type **) t = map._free;								\
	map._free = t

#define PR_RESRESET(type,map)								\
	unsigned    i, j;										\
	if (!map._size)											\
		return;												\
	for (i = 0; i < map._size; i++) {						\
		map._free = map._map[i];							\
		for (j = 0; j < 1023; j++)							\
			*(type **) &map._free[j] = &map._free[j + 1];	\
		if (i < map._size - 1)								\
			*(type **) &map._free[j] = &map._map[i + 1][0];	\
	}														\
	map._free = map._map[0];

#define PR_RESGET(map,col)									\
	unsigned    row = ~col / 1024;							\
	col = ~col % 1024;										\
	if (row >= map._size)									\
		return 0;											\
	return &map._map[row][col]

#define PR_RESINDEX(map,ptr)								\
	unsigned    i;											\
	for (i = 0; i < map._size; i++) {						\
		long d = ptr - map._map[i];							\
		if (d >= 0 && d < 1024)								\
			return ~(i * 1024 + d);							\
	}														\
	return 0

//@}

/** \defgroup pr_zone VM memory management.
	\ingroup progs

	Used to allocate and free memory in the VM address space.
*/
//@{

void PR_Zone_Init (progs_t *pr);
void PR_Zone_Free (progs_t *pr, void *ptr);
void *PR_Zone_Malloc (progs_t *pr, int size);
void *PR_Zone_Realloc (progs_t *pr, void *ptr, int size);

//@}

/** \defgroup debug VM Debugging
	\ingroup progs
	Progs debugging support.
*/
//@{

void PR_Debug_Init (void);
void PR_Debug_Init_Cvars (void);
int PR_LoadDebug (progs_t *pr);
pr_auxfunction_t *PR_Get_Lineno_Func (progs_t *pr, pr_lineno_t *lineno);
unsigned int PR_Get_Lineno_Addr (progs_t *pr, pr_lineno_t *lineno);
unsigned int PR_Get_Lineno_Line (progs_t *pr, pr_lineno_t *lineno);
pr_lineno_t *PR_Find_Lineno (progs_t *pr, unsigned int addr);
const char *PR_Get_Source_File (progs_t *pr, pr_lineno_t *lineno);
const char *PR_Get_Source_Line (progs_t *pr, unsigned int addr);
ddef_t *PR_Get_Local_Def (progs_t *pr, int offs);
void PR_PrintStatement (progs_t *pr, dstatement_t *s, int contents);
void PR_DumpState (progs_t *pr);
void PR_StackTrace (progs_t *pr);
void PR_Profile (progs_t *pr);

extern struct cvar_s *pr_debug;
extern struct cvar_s *pr_deadbeef_ents;
extern struct cvar_s *pr_deadbeef_locals;
extern struct cvar_s *pr_boundscheck;
extern struct cvar_s *pr_faultchecks;

//@}

/** \defgroup pr_cmds Quake and Quakeworld common builtins
	\ingroup progs
	\todo This doesn't really belong in progs.
*/
//@{

char *PF_VarString (progs_t *pr, int first);
void PR_Cmds_Init (progs_t *pr);

extern const char *pr_gametype;

//@}

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

struct obj_list_s;

struct progs_s {
	edict_t   **edicts;
	int        *num_edicts;
	int        *reserved_edicts;	///< alloc will start at reserved_edicts+1
	void      (*unlink) (edict_t *ent);
	void      (*flush) (void);
	int       (*parse_field) (progs_t *pr, const char *key, const char *value);
	int       (*prune_edict) (progs_t *pr, edict_t *ent);
	void      (*free_edict) (progs_t *pr, edict_t *ent);

	int         null_bad;
	int         no_exec_limit;

	void      (*file_error) (progs_t *pr, const char *path);
	void     *(*load_file) (progs_t *pr, const char *path);
	void     *(*allocate_progs_mem) (progs_t *pr, int size);
	void      (*free_progs_mem) (progs_t *pr, void *mem);

	int       (*resolve) (progs_t *pr);

	const char *progs_name;

	dprograms_t *progs;
	int         progs_size;
	unsigned short crc;

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

	/// cleared each load
	//@{
	int         num_load_finish_funcs;
	int         max_load_finish_funcs;
	pr_load_func_t **load_finish_funcs;
	//@}

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
	pr_type_t  *pr_globals;
	int         globals_size;

	pr_type_t  *pr_return;
	pr_type_t  *pr_params[MAX_PARMS];
	pr_type_t  *pr_real_params[MAX_PARMS];
	int         pr_param_size;		///< covers both params and return

	int         pr_edict_size;		///< in bytes
	int         pr_edictareasize;	///< for bounds checking, starts at 0
	func_t      edict_parse;

	int         pr_argc;

	qboolean    pr_trace;
	int         pr_trace_depth;
	dfunction_t *pr_xfunction;
	int         pr_xstatement;

	prstack_t   pr_stack[MAX_STACK_DEPTH];
	int         pr_depth;

	int         localstack[LOCALSTACK_SIZE];
	int         localstack_used;

	pr_resource_t *resources;
	struct hashtab_s *resource_hash;

	/// obj info
	//@{
	int         selector_index;
	int         selector_index_max;
	struct obj_list_s **selector_sels;
	string_t   *selector_names;
	struct hashtab_s *selector_hash;
	struct hashtab_s *classes;
	struct hashtab_s *load_methods;
	struct obj_list_s *unresolved_classes;
	struct obj_list_s *unclaimed_categories;
	struct obj_list_s *unclaimed_proto_list;
	struct obj_list_s *module_list;
	struct obj_list_s *class_tree_list;
	//@}

	/// debug info
	//@{
	const char *debugfile;
	struct pr_debug_header_s *debug;
	struct pr_auxfunction_s *auxfunctions;
	struct pr_auxfunction_s **auxfunction_map;
	struct pr_lineno_s *linenos;
	ddef_t     *local_defs;
	//@}

	/// required globals (for OP_STATE)
	struct {
		float      *time;
		int        *self;
	} globals;
	/// required fields (for OP_STATE)
	struct {
		int         nextthink;
		int         frame;
		int         think;
		int         this;
	} fields;
};

/** \addtogroup progs_data_access
*/
//@{

/** Convert a progs offset/pointer to a C pointer.
	\param pr pointer to ::progs_t VM struct
	\param o offset into global data space
	\return C pointer represented by the parameter. 0 offset -> NULL
*/
static inline pr_type_t *
PR_GetPointer (progs_t *pr, int o)
{
	return o ? pr->pr_globals + o : 0;
}

/** Convert a C pointer to a progs offset/pointer.
	\param pr pointer to ::progs_t VM struct
	\param p C pointer to be converted.
	\return Progs offset/pointer represented by \c p. NULL -> 0 offset
*/
static inline pointer_t
PR_SetPointer (progs_t *pr, void *p)
{
	return p ? (pr_type_t *) p - pr->pr_globals : 0;
}

//@}

/** \example vm-exec.c
*/

#endif//__QF_progs_h
