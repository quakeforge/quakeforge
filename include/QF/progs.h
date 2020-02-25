/*
	progs.h

	Progs VM Engine interface.

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

*/

#ifndef __QF_progs_h
#define __QF_progs_h

/** \defgroup progs QuakeC Virtual Machine (VM)
	\image html vm-mem.svg
	\image latex vm-mem.eps "VM memory map"
*/

#include "QF/pr_comp.h"
#include "QF/pr_debug.h"

struct QFile_s;

/** \ingroup progs */
///@{
typedef struct progs_s progs_t;
typedef struct pr_resource_s pr_resource_t;
typedef struct edict_s edict_t;
///@}

//============================================================================

/** \defgroup progs_misc Miscelaneous functions
	\ingroup progs
*/
///@{

/** Initialize the progs engine.

	The first call will initialize subsystems common to all progs instances.

	\param pr		The progs engine instance to initialize.
*/
void PR_Init (progs_t *pr);

/** Initialize the Cvars for the progs engine. Call before calling PR_Init().
*/
void PR_Init_Cvars (void);

void PR_Error (progs_t *pr, const char *error, ...) __attribute__((format(printf,2,3), noreturn));
void PR_RunError (progs_t *pr, const char *error, ...) __attribute__((format(printf,2,3), noreturn));

///@}

/** \defgroup progs_execution Execution
	\ingroup progs
*/
///@{

/** Ensure P_* macros point to the right place for passing parameters to progs
	functions.
	\param pr		pointer to ::progs_t VM struct

	\warning Failure to use this macro before assigning to the P_* macros can
	cause corruption of the VM data due to "register" based calling. Can be
	safely ignored for parameterless functions, or forwarding parameters
	though a builtin.

	\hideinitializer
*/
#define PR_RESET_PARAMS(pr)							\
	do {											\
		(pr)->pr_params[0] = (pr)->pr_real_params[0];	\
		(pr)->pr_params[1] = (pr)->pr_real_params[1];	\
	} while (0)

/**	\name Detouring Function Calls

	These functions allow a builtin function that uses PR_CallFunction() to
	safely insert a call to another VM function. The +initialize diversion
	required by Objective-QuakeC uses this.

		PR_PushFrame (pr);
		__auto_type params = PR_SaveParams (pr);
		... set up parameters to detour_function
		PR_ExecuteProgram (pr, detour_function)
		PR_RestoreParams (pr, params);
		PR_PopFrame (pr);

*/
///@{
typedef struct pr_stashed_params_s {
	pr_type_t  *param_ptrs[2];
	int         argc;
	pr_type_t   params[1];
} pr_stashed_params_t;

/** Save the current parameters to the provided stash.

	\warning	The memory for the parameter stash is allocated using
				alloca().

	\param pr		pointer to ::progs_t VM struct
	\return 		Pointer to a newly allocated and initialized parameter
					stash that has the current parameters saved to it.
	\hideinitializer
*/
#define PR_SaveParams(pr) \
	_PR_SaveParams((pr), \
				   alloca (field_offset (pr_stashed_params_t, \
										 params[(pr)->pr_argc \
												* (pr)->pr_param_size])))

/** [INTERNAL] Save the current parameters to the provided stash.

	\warning	Requires \a params to be correctly allocated. Use
				PR_SaveParams instead as it will create a suitable stash for
				saving the parameters.

	\param pr		pointer to ::progs_t VM struct
	\param params	location to save the parameters, must be of adequade size
					to hold \a pr_argc * \a pr_param_size words in \a params
	\return 		\a params Allows the likes of:
						__auto_type params = PR_SaveParams (pr);
*/
pr_stashed_params_t *_PR_SaveParams (progs_t *pr, pr_stashed_params_t *params);

/** Restore the parameters saved by PR_SaveParams().
	\param pr		pointer to ::progs_t VM struct
	\param params	pointer to stash created by PR_SaveParams()
*/
void PR_RestoreParams (progs_t *pr, pr_stashed_params_t *params);
///@}

/** Push an execution frame onto the VM stack. Saves current execution state.
	\param pr		pointer to ::progs_t VM struct
*/
void PR_PushFrame (progs_t *pr);

/** Pop an execution frame from the VM stack. Restores execution state. Also
	frees any temporary strings allocated in this frame (via
	PR_FreeTempStrings()).
	\param pr		pointer to ::progs_t VM struct
*/
void PR_PopFrame (progs_t *pr);

/** Run a progs function. If \p fnum is a builtin rather than a progs
	function, PR_ExecuteProgram() will call the function and then immediately
	return to the caller. Nested calls are fully supported.
	\param pr		pointer to ::progs_t VM struct
	\param fnum		number of the function to call
*/
void PR_ExecuteProgram (progs_t *pr, func_t fnum);

/** Setup to call a function. If \p fnum is a builtin rather than a progs
	function, then the function is called immediately. When called from a
	builtin function, and \p fnum is not a builtin, the progs function will
	execute upon return of control to PR_ExecuteProgram().
	\param pr		pointer to ::progs_t VM struct
	\param fnum		number of the function to call
	\return			true if \p fnum was a progs function, false if \p fnum was
					a builtin
*/
int PR_CallFunction (progs_t *pr, func_t fnum);

///@}

/** \defgroup progs_load Loading
	\ingroup progs
*/
///@{

/** Type of functions that are called at progs load.
	\param pr		pointer to ::progs_t VM struct
	\return			true for success, false for failure
*/
typedef int pr_load_func_t (progs_t *pr);

/** Initialize a ::progs_t VM struct from an already open file.
	\param pr		pointer to ::progs_t VM struct
	\param file		handle of file to read progs data from
	\param size		bytes of \p file to read

	\note \e All runtime strings (permanent or temporary) are allocated from
	the VM's dynamic memory space, so be sure \p zone is of sufficient size
	(by setting pr->zone_size prior to calling).  So far, 1MB has proven more
	than sufficient for Quakeword, even when using Ruamoko objects.
	\note If entities are used, ensure pr->max_edicts is set appropriately
	prior to calling.
*/
void PR_LoadProgsFile (progs_t *pr, struct QFile_s *file, int size);

/** Convenience wrapper for PR_LoadProgsFile() and PR_RunLoadFuncs().
	Searches for the specified file in the Quake filesystem.
	\param pr		pointer to ::progs_t VM struct
	\param progsname name of the file to load as progs data
*/
void PR_LoadProgs (progs_t *pr, const char *progsname);

/** Register a primary function to be called after the progs code has been
	loaded. These functions are remembered across progs loads. They will be
	called in order of registration.
	\param pr		pointer to ::progs_t VM struct
	\param func		function to call
*/
void PR_AddLoadFunc (progs_t *pr, pr_load_func_t *func);

/** Register a secondary function to be called after the progs code has been
	loaded. These functions will be forgotten after each load. They will be
	called in \e reverse order of being registered.
	\param pr		pointer to ::progs_t VM struct
	\param func		function to call
*/
void PR_AddLoadFinishFunc (progs_t *pr, pr_load_func_t *func);

/** Run all load functions. Any load function returning false will abort the
	load operation.
	\param pr		pointer to ::progs_t VM struct
	\return			true for success, false for failure

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
	\param pr		pointer to ::progs_t VM struct
	\return			true for success, false for failure

	\todo should this be elsewhere?
*/
int PR_Check_Opcodes (progs_t *pr);

void PR_BoundsCheckSize (progs_t *pr, pointer_t addr, unsigned size);
void PR_BoundsCheck (progs_t *pr, int addr, etype_t type);

///@}

/** \defgroup progs_edict Edict management
	\ingroup progs
*/
///@{

struct edict_s {
	qboolean    free;
	int         entnum;			///< number of this entity
	float       freetime;		///< sv.time when the object was freed
	void       *edata;			///< external per-edict data
	pr_type_t   v[];			///< fields from progs
};

// pr_edict.c
void ED_ClearEdict (progs_t *pr, edict_t *e, int val);
edict_t *ED_Alloc (progs_t *pr);
void ED_Free (progs_t *pr, edict_t *ed);
edict_t *ED_EdictNum(progs_t *pr, pr_int_t n) __attribute__((pure));
pr_int_t ED_NumForEdict(progs_t *pr, edict_t *e) __attribute__((pure));
void ED_Count (progs_t *pr);
qboolean PR_EdictValid (progs_t *pr, pr_int_t e) __attribute__((pure));

// pr_debug.c
void ED_Print (progs_t *pr, edict_t *ed);
void ED_PrintEdicts (progs_t *pr, const char *fieldval);
void ED_PrintNum (progs_t *pr, pr_int_t ent);

// pr_parse.c
struct script_s;
struct plitem_s;
qboolean ED_ParseEpair (progs_t *pr, pr_type_t *base, pr_def_t *key,
						const char *s);
struct plitem_s *ED_EntityDict (progs_t *pr, edict_t *ed);
struct plitem_s *ED_GlobalsDict (progs_t *pr);
void ED_InitGlobals (progs_t *pr, struct plitem_s *globals);
void ED_InitEntity (progs_t *pr, struct plitem_s *entity, edict_t *ent);
struct plitem_s *ED_ConvertToPlist (struct script_s *script, int nohack);
struct plitem_s *ED_Parse (progs_t *pr, const char *data);
void ED_LoadFromFile (progs_t *pr, const char *data);
void ED_EntityParseFunction (progs_t *pr);

#define PR_edicts(p)		((byte *) *(p)->edicts)

#define NEXT_EDICT(p,e)		((edict_t *) ((byte *) e + (p)->pr_edict_size))
#define EDICT_TO_PROG(p,e)	((pr_int_t)(intptr_t)((byte *)(e) - PR_edicts (p)))
#define PROG_TO_EDICT(p,e)	((edict_t *) (PR_edicts (p) + (e)))
#define NUM_FOR_BAD_EDICT(p,e) ((e)->entnum)
#ifndef PR_PARANOID_PROGS
# define EDICT_NUM(p,n)		(PROG_TO_EDICT (p, (n) * (p)->pr_edict_size))
# define NUM_FOR_EDICT(p,e)	NUM_FOR_BAD_EDICT (p, e)
#else
# define EDICT_NUM(p,n)		ED_EdictNum (p, n)
# define NUM_FOR_EDICT(p,e)	ED_NumForEdict (p, e)
#endif

///@}

/** \defgroup pr_symbols Symbol Management
	\ingroup progs
	Lookup functions for symbol name resolution.
*/
///@{

pr_def_t *PR_FieldAtOfs (progs_t *pr, pointer_t ofs) __attribute__((pure));
pr_def_t *PR_GlobalAtOfs (progs_t *pr, pointer_t ofs) __attribute__((pure));

pr_def_t *PR_FindField (progs_t *pr, const char *name);
pr_def_t *PR_FindGlobal (progs_t *pr, const char *name);
dfunction_t *PR_FindFunction (progs_t *pr, const char *name);

int PR_ResolveGlobals (progs_t *pr);

int PR_AccessField (progs_t *pr, const char *name, etype_t type,
					const char *file, int line);

void PR_Undefined (progs_t *pr, const char *type, const char *name) __attribute__((noreturn));
///@}

//============================================================================

/** \defgroup progs_data_access Data Access
	\ingroup progs
	Macros for accessing data in the VM address space
*/

/** \defgroup prda_globals Globals
	\ingroup progs_data_access
	Typed global access macros. No checking is done against the QC type, but
	the appropriate C type will be used.
*/
///@{

/** \internal
	\param p		pointer to ::progs_t VM struct
	\param o		offset into global data space
	\param t		typename prefix (see pr_type_u)
	\return			lvalue of the appropriate type

	\hideinitializer
*/
#define G_var(p,o,t)	((p)->pr_globals[o].t##_var)

/** Access a float global. Can be assigned to.

	\par QC type:
		\c float
	\param p		pointer to ::progs_t VM struct
	\param o		offset into global data space
	\return			float lvalue

	\hideinitializer
*/
#define G_FLOAT(p,o)	G_var (p, o, float)

/** Access a double global. Can be assigned to.

	\par QC type:
		\c double
	\param p		pointer to ::progs_t VM struct
	\param o		offset into global data space
	\return			double lvalue

	\hideinitializer
*/
#define G_DOUBLE(p,o)	(*(double *) ((p)->pr_globals + o))

/** Access an integer global. Can be assigned to.

	\par QC type:
		\c integer
	\param p		pointer to ::progs_t VM struct
	\param o		offset into global data space
	\return			int lvalue

	\hideinitializer
*/
#define G_INT(p,o)		G_var (p, o, integer)

/** Access an unsigned integer global. Can be assigned to.

	\par QC type:
		\c uinteger
	\param p		pointer to ::progs_t VM struct
	\param o		offset into global data space
	\return			unsigned int lvalue

	\hideinitializer
*/
#define G_UINT(p,o)		G_var (p, o, uinteger)

/** Access a vector global. Can be assigned to.

	\par QC type:
		\c vector
	\param p		pointer to ::progs_t VM struct
	\param o		offset into global data space
	\return			vec3_t lvalue

	\hideinitializer
*/
#define G_VECTOR(p,o)	G_var (p, o, vector)

/** Access a quaternion global. Can be assigned to.

	\par QC type:
		\c quaternion
	\param p		pointer to ::progs_t VM struct
	\param o		offset into global data space
	\return			quat_t lvalue

	\hideinitializer
*/
#define G_QUAT(p,o)		G_var (p, o, quat)

/** Access a string index global. Can be assigned to.

	\par QC type:
		\c string
	\param p		pointer to ::progs_t VM struct
	\param o		offset into global data space
	\return			string_t lvalue

	\hideinitializer
*/
#define G_STRING(p,o)	G_var (p, o, string)

/** Access a function global. Can be assigned to.

	\par QC type:
		\c void()
	\param p		pointer to ::progs_t VM struct
	\param o		offset into global data space
	\return			func_t lvalue

	\hideinitializer
*/
#define G_FUNCTION(p,o)	G_var (p, o, func)

/** Access a pointer global. Can be assigned to.

	\par QC type:
		\c void *
	\param p		pointer to ::progs_t VM struct
	\param o		offset into global data space
	\return			pointer_t lvalue

	\hideinitializer
*/
#define G_POINTER(p,o)	G_var (p, o, pointer)


/** Access an entity global.

	\par QC type:
		\c entity
	\param p		pointer to ::progs_t VM struct
	\param o		offset into global data space
	\return			C pointer to the entity

	\hideinitializer
*/
#define G_EDICT(p,o)	((edict_t *)(PR_edicts (p) + G_INT (p, o)))

/** Access an entity global.

	\par QC type:
		\c entity
	\param p		pointer to ::progs_t VM struct
	\param o		offset into global data space
	\return			the entity number

	\hideinitializer
*/
#define G_EDICTNUM(p,o)	NUM_FOR_EDICT(p, G_EDICT (p, o))

/** Access a string global, converting it to a C string. Kills the program
	if the string is invalid.

	\par QC type:
		\c string
	\param p		pointer to ::progs_t VM struct
	\param o		offset into global data space
	\return			const char * to the string

	\hideinitializer
*/
#define G_GSTRING(p,o)	PR_GetString (p, G_STRING (p, o))

/** Access a dstring global. Kills the program if the dstring is invalid.

	\par QC type:
		\c string
	\param p		pointer to ::progs_t VM struct
	\param o		offset into global data space
	\return			dstring_t * to the dstring

	\hideinitializer
*/
#define G_DSTRING(p,o)	PR_GetMutableString (p, G_STRING (p, o))

/** Access a pointer parameter.

	\par QC type:
		\c void *
	\param p		pointer to ::progs_t VM struct
	\param o		offset into global data space
	\return			C pointer represented by the parameter. 0 offset -> NULL

	\hideinitializer
*/
#define G_GPOINTER(p,o)	PR_GetPointer (p, o)

/** Access a structure global. Can be assigned to.

	\par QC type:
		\c void *
	\param p		pointer to ::progs_t VM struct
	\param t		C type of the structure
	\param o		offset into global data space
	\return			structure lvalue. use & to make a pointer of the
					appropriate type.

	\hideinitializer
*/
#define G_STRUCT(p,t,o)	(*(t *)G_GPOINTER (p, o))
///@}

/** \defgroup prda_parameters Parameters
	\ingroup progs_data_access
	Typed parameter access macros. No checking is done against the QC type, but
	the appropriate C type will be used.
*/
///@{

/** \internal
	\param p		pointer to ::progs_t VM struct
	\param n		parameter number (0-7)
	\param t		typename prefix (see pr_type_u)
	\return			lvalue of the appropriate type

	\hideinitializer
*/
#define P_var(p,n,t)	((p)->pr_params[n]->t##_var)

/** Access a float parameter. Can be assigned to.

	\par QC type:
		\c float
	\param p		pointer to ::progs_t VM struct
	\param n		parameter number (0-7)
	\return			float lvalue

	\hideinitializer
*/
#define P_FLOAT(p,n)	P_var (p, n, float)

/** Access a double parameter. Can be assigned to.

	\par QC type:
		\c double
	\param p		pointer to ::progs_t VM struct
	\param n		parameter number (0-7)
	\return			double lvalue

	\hideinitializer
*/
#define P_DOUBLE(p,n)	(*(double *) ((p)->pr_params[n]))

/** Access an integer parameter. Can be assigned to.

	\par QC type:
		\c integer
	\param p		pointer to ::progs_t VM struct
	\param n		parameter number (0-7)
	\return			int lvalue

	\hideinitializer
*/
#define P_INT(p,n)		P_var (p, n, integer)

/** Access an unsigned integer parameter. Can be assigned to.

	\par QC type:
		\c uinteger
	\param p		pointer to ::progs_t VM struct
	\param n		parameter number (0-7)
	\return			unsigned int lvalue

	\hideinitializer
*/
#define P_UINT(p,n)		P_var (p, n, uinteger)

/** Access a vector parameter. Can be used any way a vec3_t variable can.

	\par QC type:
		\c vector
	\param p		pointer to ::progs_t VM struct
	\param n		parameter number (0-7)
	\return			vec3_t lvalue

	\hideinitializer
*/
#define P_VECTOR(p,n)	P_var (p, n, vector)

/** Access a quaterion parameter. Can be used any way a quat_t variable can.

	\par QC type:
		\c quaterion
	\param p		pointer to ::progs_t VM struct
	\param n		parameter number (0-7)
	\return			quat_t lvalue

	\hideinitializer
*/
#define P_QUAT(p,n)		P_var (p, n, quat)

/** Access a string index parameter. Can be assigned to.

	\par QC type:
		\c string
	\param p		pointer to ::progs_t VM struct
	\param n		parameter number (0-7)
	\return			string_t lvalue

	\hideinitializer
*/
#define P_STRING(p,n)	P_var (p, n, string)

/** Access a function parameter. Can be assigned to.

	\par QC type:
		\c void()
	\param p		pointer to ::progs_t VM struct
	\param n		parameter number (0-7)
	\return			func_t lvalue

	\hideinitializer
*/
#define P_FUNCTION(p,n)	P_var (p, n, func)

/** Access a pointer parameter. Can be assigned to.

	\par QC type:
		\c void *
	\param p		pointer to ::progs_t VM struct
	\param n		parameter number (0-7)
	\return			pointer_t lvalue

	\hideinitializer
*/
#define P_POINTER(p,n)	P_var (p, n, pointer)


/** Access an entity parameter.

	\par QC type:
		\c entity
	\param p		pointer to ::progs_t VM struct
	\param n		parameter number (0-7)
	\return			C pointer to the entity

	\hideinitializer
*/
#define P_EDICT(p,n)	((edict_t *)(PR_edicts (p) + P_INT (p, n)))

/** Access an entity parameter.

	\par QC type:
		\c entity
	\param p		pointer to ::progs_t VM struct
	\param n		parameter number (0-7)
	\return			the entity number

	\hideinitializer
*/
#define P_EDICTNUM(p,n)	NUM_FOR_EDICT (p, P_EDICT (p, n))

/** Access a string parameter, converting it to a C string. Kills the program
	if the string is invalid.

	\par QC type:
		\c string
	\param p		pointer to ::progs_t VM struct
	\param n		parameter number (0-7)
	\return			const char * to the string

	\hideinitializer
*/
#define P_GSTRING(p,n)	PR_GetString (p, P_STRING (p, n))

/** Access a dstring parameter. Kills the program if the dstring is invalid.

	\par QC type:
		\c string
	\param p		pointer to ::progs_t VM struct
	\param n		parameter number (0-7)
	\return			dstring_t * to the dstring

	\hideinitializer
*/
#define P_DSTRING(p,n)	PR_GetMutableString (p, P_STRING (p, n))

/** Access a pointer parameter.

	\par QC type:
		\c void *
	\param p		pointer to ::progs_t VM struct
	\param n		parameter number (0-7)
	\return			C pointer represented by the parameter. 0 offset -> NULL

	\hideinitializer
*/
#define P_GPOINTER(p,n)	PR_GetPointer (p, P_POINTER (p, n))

/** Access a structure pointer parameter.

	\par QC type:
		\c void *
	\param p		pointer to ::progs_t VM struct
	\param t		C type of the structure
	\param n		parameter number (0-7)
	\return			structure lvalue. use & to make a pointer of the
					appropriate type.

	\hideinitializer
*/
#define P_STRUCT(p,t,n)	(*(t *)P_GPOINTER (p, n))
///@}

/** \defgroup prda_return Return Values
	\ingroup progs_data_access
	These macros are used to access the value returned by an interpreted VM
	function, and to return values from engine functions into progs space
	(that is, builtins).
	\warning No checking is performed against progs types; for example, if you
	ask for an \c int from a function that returned a \c float, you're asking
	for trouble.
*/
///@{

/** \internal
	\param p		pointer to ::progs_t VM struct
	\param t		typename prefix (see pr_type_u)
	\return			lvalue of the appropriate type

	\hideinitializer
*/
#define R_var(p,t)		((p)->pr_return->t##_var)

/** Access the VM function return value as a \c float

	\par QC type:
		\c float
	\param p		pointer to ::progs_t VM struct
	\return			float lvalue

	\hideinitializer
*/
#define R_FLOAT(p)		R_var (p, float)

/** Access the VM function return value as a \c double

	\par QC type:
		\c double
	\param p		pointer to ::progs_t VM struct
	\return			double lvalue

	\hideinitializer
*/
#define R_DOUBLE(p)		(*(double *) ((p)->pr_return))

/** Access the VM function return value as a \c ::pr_int_t (AKA int32_t)

	\par QC type:
		\c integer
	\param p		pointer to ::progs_t VM struct
	\return			::pr_int_t lvalue

	\hideinitializer
*/
#define R_INT(p)		R_var (p, integer)

/** Access the VM function return value as a \c ::pr_uint_t (AKA uint32_t)

	\par QC type:
		\c uinteger
	\param p		pointer to ::progs_t VM struct
	\return			::pr_int_t lvalue

	\hideinitializer
*/
#define R_UINT(p)		R_var (p, uinteger)

/** Access the VM function return value as a \c ::vec3_t vector.

	\par QC type:
		\c vector
	\param p		pointer to ::progs_t VM struct
	\return			::vec3_t lvalue

	\hideinitializer
*/
#define R_VECTOR(p)		R_var (p, vector)

/** Access the VM function return value as a \c ::quat_t quaternion.

	\par QC type:
		\c quaternion
	\param p		pointer to ::progs_t VM struct
	\return			::quat_t lvalue

	\hideinitializer
*/
#define R_QUAT(p)		R_var (p, quat)

/** Access the VM function return value as a ::string_t (a VM string reference).

	\par QC type:
		\c string
	\param p		pointer to ::progs_t VM struct
	\return			::string_t lvalue

	\hideinitializer
*/
#define R_STRING(p)		R_var (p, string)

/** Access the VM function return value as a ::func_t (a VM function reference)

	\par QC type:
		\c void()
	\param p		pointer to ::progs_t VM struct
	\return			::func_t lvalue

	\hideinitializer
*/
#define R_FUNCTION(p)	R_var (p, func)

/** Access the VM function return value as a ::pointer_t (a VM "pointer")

	\par QC type:
		\c void *
	\param p		pointer to ::progs_t VM struct
	\return			::pointer_t lvalue

	\hideinitializer
*/
#define R_POINTER(p)	R_var (p, pointer)


/** Set the return value to the given C string. The returned string will
	eventually be garbage collected (see PR_SetReturnString()).

	\par QC type:
		\c string
	\param p		pointer to ::progs_t VM struct
	\param s		C string to be returned

	\hideinitializer
*/
#define RETURN_STRING(p,s)		(R_STRING (p) = PR_SetReturnString((p), s))

/** Set the return value to the given C entity pointer. The pointer is
	converted into a progs entity address.

	\par QC type:
		\c entity
	\param p		pointer to ::progs_t VM struct
	\param e		C entity pointer to be returned

	\hideinitializer
*/
#define RETURN_EDICT(p,e)		(R_STRING (p) = EDICT_TO_PROG(p, e))

/** Set the return value to the given C pointer. NULL is converted to 0.

	\par QC type:
		\c void *
	\param p		pointer to ::progs_t VM struct
	\param ptr		C entity pointer to be returned

	\hideinitializer
*/
#define RETURN_POINTER(p,ptr)	(R_POINTER (p) = PR_SetPointer (p, ptr))

/** Set the return value to the given vector.

	\par QC type:
		\c vector
	\param p		pointer to ::progs_t VM struct
	\param v		vector to be returned

	\hideinitializer
*/
#define RETURN_VECTOR(p,v)		VectorCopy (v, R_VECTOR (p))

/** Set the return value to the given quaterion.

	\par QC type:
		\c vector
	\param p		pointer to ::progs_t VM struct
	\param q		quaternion to be returned

	\hideinitializer
*/
#define RETURN_QUAT(p,q)		VectorCopy (q, R_QUAT (p))
///@}

/** \defgroup prda_entity_fields Entity Fields
	\ingroup progs_data_access
	Typed entity field access macros. No checking is done against the QC type,
	but the appropriate C type will be used.
*/
///@{

/** \internal
	\param e		pointer to the entity
	\param o		field offset into entity data space
	\param t		typename prefix (see pr_type_u)
	\return			lvalue of the appropriate type

	\hideinitializer
*/
#define E_var(e,o,t)	((e)->v[o].t##_var)


/** Access a float entity field. Can be assigned to.

	\par QC type:
		\c float
	\param e		pointer to the entity
	\param o		field offset into entity data space
	\return			float lvalue

	\hideinitializer
*/
#define E_FLOAT(e,o)	E_var (e, o, float)

/** Access a double entity field. Can be assigned to.

	\par QC type:
		\c double
	\param e		pointer to the entity
	\param o		field offset into entity data space
	\return			double lvalue

	\hideinitializer
*/
#define E_DOUBLE(e,o)	(*(double *) ((e)->v + o))

/** Access an integer entity field. Can be assigned to.

	\par QC type:
		\c integer
	\param e		pointer to the entity
	\param o		field offset into entity data space
	\return			int lvalue

	\hideinitializer
*/
#define E_INT(e,o)		E_var (e, o, integer)

/** Access an unsigned integer entity field. Can be assigned to.

	\par QC type:
		\c uinteger
	\param e		pointer to the entity
	\param o		field offset into entity data space
	\return			unsigned int lvalue

	\hideinitializer
*/
#define E_UINT(e,o)		E_var (e, o, uinteger)

/** Access a vector entity field. Can be used any way a vec3_t variable can.

	\par QC type:
		\c vector
	\param e		pointer to the entity
	\param o		field offset into entity data space
	\return			vec3_t lvalue

	\hideinitializer
*/
#define E_VECTOR(e,o)	E_var (e, o, vector)

/** Access a quaternion entity field. Can be used any way a quat_t variable
	can.

	\par QC type:
		\c quaterion
	\param e		pointer to the entity
	\param o		field offset into entity data space
	\return			quat_t lvalue

	\hideinitializer
*/
#define E_QUAT(e,o)		E_var (e, o, quat)

/** Access a string index entity field. Can be assigned to.

	\par QC type:
		\c string
	\param e		pointer to the entity
	\param o		field offset into entity data space
	\return			string_t lvalue

	\hideinitializer
*/
#define E_STRING(e,o)	E_var (e, o, string)

/** Access a function entity field. Can be assigned to.

	\par QC type:
		\c void()
	\param e		pointer to the entity
	\param o		field offset into entity data space
	\return			func_t lvalue

	\hideinitializer
*/
#define E_FUNCTION(e,o)	E_var (e, o, func)

/** Access a pointer entity field. Can be assigned to.

	\par QC type:
		\c void *
	\param e		pointer to the entity
	\param o		field offset into entity data space
	\return			pointer_t lvalue

	\hideinitializer
*/
#define E_POINTER(e,o)	E_var (e, o, pointer)


/** Access a string entity field, converting it to a C string. Kills the
	program if the string is invalid.

	\par QC type:
		\c string
	\param p		pointer to ::progs_t VM struct
	\param e		pointer to the entity
	\param o		field offset into entity data space
	\return			const char * to the string

	\hideinitializer
*/
#define E_GSTRING(p,e,o) (PR_GetString (p, E_STRING (e, o)))

/** Access a dstring entity field. Kills the program if the dstring is invalid.

	\par QC type:
		\c string
	\param p		pointer to ::progs_t VM struct
	\param e		pointer to the entity
	\param o		field offset into entity data space
	\return			dstring_t * to the dstring

	\hideinitializer
*/
#define E_DSTRING(p,e,o) (PR_GetMutableString (p, E_STRING (e, o)))
///@}

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
///@{

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
	/// QC name of the builtin. Must be an exact match for automatically
	/// resolved builtins (func = \#0 in QC). NULL indicates end of array for
	/// PR_RegisterBuiltins()
	const char *name;
	/// Pointer to the C implementation of the builtin function.
	builtin_proc proc;
	/// The number of the builtin for \#N in QC. -1 for automatic allocation.
	/// 0 or >= ::PR_AUTOBUILTIN is invalid.
	pr_int_t    binum;
} builtin_t;

/** Duplicate the dfunction_t descriptor with the addition of a pointer to the
	builtin function. Avoids a level of indirection when calling a builtin
	function.
*/
typedef struct {
	pr_int_t    first_statement;
	pr_int_t    parm_start;
	pr_int_t    locals;
	pr_int_t    profile;
	pr_int_t    numparms;
	dparmsize_t parm_size[MAX_PARMS];
	dfunction_t *descriptor;
	builtin_proc func;
} bfunction_t;

/** Register a set of builtin functions with the VM. Different VMs within the
	same program can have different builtin sets. May be called multiple times
	for the same VM, but redefining a builtin is an error.
	\param pr		pointer to ::progs_t VM struct
	\param builtins	array of builtin_t builtins
*/
void PR_RegisterBuiltins (progs_t *pr, builtin_t *builtins);

/** Lookup a builtin function referred by name.
	\param pr		pointer to ::progs_t VM struct
	\param name		name of the builtin function to lookup
	\return			pointer to the builtin function entry, or NULL if not found
*/
builtin_t *PR_FindBuiltin (progs_t *pr, const char *name);

/** Lookup a builtin function by builtin number.
	\param pr		pointer to ::progs_t VM struct
	\param num		number of the builtin function to lookup
	\return			pointer to the builtin function entry, or NULL if not found
*/
builtin_t *PR_FindBuiltinNum (progs_t *pr, pr_int_t num);

/** Fixup all automatically resolved builtin functions (func = #0 in QC).
	Performs any necessary builtin function number mapping. Also builds the
	bfunction_t table. Called automatically during progs load.
	\param pr		pointer to ::progs_t VM struct
	\return			true for success, false for failure
*/
int PR_RelocateBuiltins (progs_t *pr);

///@}

/** \defgroup pr_strings String Management
	\ingroup progs
	Strings management functions.

	All strings accessable by the VM are stored within the VM address space.
	These functions provide facilities to set permanent, dynamic and
	temporary strings, as well as mutable strings using dstrings.

	Permanent strings are either supplied by the progs (+ve string index) or
	set by the main program using PR_SetString(). Permanent strings can never
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
///@{

/** Initialize the string management subsystem.

	\param pr		The VM of which the string management subsystem will be
					initialized;
*/
void PR_Strings_Init (progs_t *pr);

/** Check the validity of a string index.
	\param pr		pointer to ::progs_t VM struct
	\param num		string index to be validated
	\return			true if the index is valid, false otherwise
*/
qboolean PR_StringValid (progs_t *pr, string_t num) __attribute__((pure));

/** Convert a string index to a C string.
	\param pr		pointer to ::progs_t VM struct
	\param num		string index to be converted
	\return			C pointer to the string.
*/
const char *PR_GetString(progs_t *pr, string_t num) __attribute__((pure));

/** Retrieve the dstring_t associated with a mutable string.
	\param pr		pointer to ::progs_t VM struct
	\param num		string index of the mutable string
	\return			the dstring implementing the mutable string
*/
struct dstring_s *PR_GetMutableString(progs_t *pr, string_t num) __attribute__((pure));

/** Make a permanent progs string from the given C string. Will not create a
	duplicate permanent string (temporary and mutable strings are not checked).
	\param pr		pointer to ::progs_t VM struct
	\param s		C string to be made into a permanent progs string
	\return			string index of the progs string
*/
string_t PR_SetString(progs_t *pr, const char *s);

/** Make a temporary progs string that will survive across function returns.
	Will not duplicate a permanent string. If a new progs string is created,
	it will be freed after ::PR_RS_SLOTS calls to this function. ie, up to
	::PR_RS_SLOTS return strings can exist at a time.
	\param pr		pointer to ::progs_t VM struct
	\param s		C string to be returned to the progs code
	\return			string index of the progs string
*/
string_t PR_SetReturnString(progs_t *pr, const char *s);

/** Make a temporary progs string that will be freed when the current progs
	stack frame is exited. Will not duplicate a permantent string.
	\param pr		pointer to ::progs_t VM struct
	\param s		C string
	\return			string index of the progs string
*/
string_t PR_SetTempString(progs_t *pr, const char *s);

/** Make a temporary progs string that is the concatenation of two C strings.
	\param pr		pointer to ::progs_t VM struct
	\param a		C string
	\param b		C string
	\return			string index of the progs string that represents the
					concatenation of strings a and b
*/
string_t PR_CatStrings (progs_t *pr, const char *a, const char *b);

/** Convert a mutable string to a temporary string.
	\param pr		pointer to ::progs_t VM struct
	\param str		string index of the mutable string to be converted
*/
void PR_MakeTempString(progs_t *pr, string_t str);

/** Create a new mutable string.
	\param pr		pointer to ::progs_t VM struct
	\return			string index of the newly created mutable string
*/
string_t PR_NewMutableString (progs_t *pr);

/** Make a dynamic progs string from the given C string. Will not create a
	duplicate permanent string (temporary, dynamic and mutable strings are
	not checked).
	\param pr		pointer to ::progs_t VM struct
	\param s		C string to be made into a permanent progs string
	\return			string index of the progs string
*/
string_t PR_SetDynamicString (progs_t *pr, const char *s);

/** Destroy a mutable, dynamic or temporary string.
	\param pr		pointer to ::progs_t VM struct
	\param str		string index of the string to be destroyed
*/
void PR_FreeString (progs_t *pr, string_t str);

/** Free all the temporary strings allocated in the current stack frame.
	\param pr		pointer to ::progs_t VM struct
*/
void PR_FreeTempStrings (progs_t *pr);

/** Formatted printing similar to C's vsprintf, but using QC types.
	The format string is a string of characters (other than \c \%) to be
	printed that includes optional format specifiers, one for each arg to be
	printed.
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
		<li>\c 'p'	\c void * Print a pointer value. ("%#x")
		<li>\c 's'	\c string Print a string value. ("%s")
		<li>\c 'v'	\c vector Print a vector value. ("'%g %g %g'")
		<li>\c 'q'	\c quaternion Print a quaternion value. ("'%g %g %g %g'")
		<li>\c 'x'	\c uinteger Print an unsigned integer value. ("%x")
		</ul>
	</ul>

	For details, refer to the C documentation for printf.

	\param pr		pointer to ::progs_t VM struct
	\param result	dstring to which printing is done
	\param name		name to be reported in case of errors
	\param format	the format string
	\param count	number of args
	\param args		array of pointers to the values to be printed
*/
void PR_Sprintf (progs_t *pr, struct dstring_s *result, const char *name,
				 const char *format, int count, pr_type_t **args);

///@}

/** \defgroup pr_resources Resource Management
	\ingroup progs
	Builtin module private data management.
*/
///@{

/** Initialize the resource management fields.

	\param pr		The VM of which the resource fields will be initialized.
*/
void PR_Resources_Init (progs_t *pr);

/** Clear all resources before loading a new progs.

	Calls the clear() callback of all registered resources.

	\param pr		The VM of which the resources will be cleared.
*/
void PR_Resources_Clear (progs_t *pr);

/** Register a resource with a VM.

	\param pr		The VM to which the resource will be associated.
	\param name		The name of the resource. Used for retrieving the
					resource.
	\param data		The resource data.
					callback.
	\param clear	Callback for performing any necessary cleanup. Called
					by PR_Resources_Clear(). The parameters are the current
					VM (\p pr) and \p data.
	\note			The name should be unique to the VM, but no checking is
					done. If the name is not unique, the previous registered
					resource will break. The name of the sub-system
					registering the resource is a suitable name, and will
					probably be unique.
*/
void PR_Resources_Register (progs_t *pr, const char *name, void *data,
							void (*clear)(progs_t *, void *));

/** Retrieve a resource registered with the VM.

	\param pr		The VM from which the resource will be retrieved.
	\param name		The name of the resource.
	\return			The resource, or NULL if \p name does not match any
					resource registered with the VM.
*/
void *PR_Resources_Find (progs_t *pr, const char *name);

/** \name Resource Map support

	These macros can be used to create functions for mapping C resources
	to QuakeC integer handles.

	Valid handles are always negative.

	\note			\p map is the resource map itself, not a pointer to the
					resource map.
*/
///@{

/** Type delcaration for the resource map.

	\param type		The type of the resource. The size must be at least
					as large as \c sizeof(type *).
*/
#define PR_RESMAP(type) struct { type *_free; type **_map; unsigned _size; }

/** Allocate a new resource from the resource map.

	\param type		The type of the resource. Must match the \c type parameter
					used for PR_RESMAP.
	\param map		The resource map.
	\return			A pointer to the new resource, or null if no more could
					be allocated.
*/
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

/** Free a resource returning it to the resource map.

	\param type		The type of the resource. Must match the \c type parameter
					used for PR_RESMAP.
	\param map		The resource map.
	\param t		Pointer to the resource to be freed.
*/
#define PR_RESFREE(type,map,t)								\
	memset (t, 0, sizeof (type));							\
	*(type **) t = map._free;								\
	map._free = t

/** Free all resources in the resource map.

	Any memory allocated to the resource must be freed before freeing the
	resource.

	\param type		The type of the resource. Must match the \c type parameter
					used for PR_RESMAP.
	\param map		The resource map.
*/
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

/** Retrieve a resource from the resource map using a handle.

	\param map		The resource map.
	\param col		The handle.
	\return			A pointer to the resource, or NULL if the handle is
					invalid.
*/
#define PR_RESGET(map,col)									\
	unsigned    row = ~col / 1024;							\
	col = ~col % 1024;										\
	if (row >= map._size)									\
		return 0;											\
	return &map._map[row][col]

/** Convert a resource pointer to a handle.

	\param map		The resource map.
	\param ptr		The resource pointer.
	\return			The handle or 0 if the pointer is invalid.
*/
#define PR_RESINDEX(map,ptr)								\
	unsigned    i;											\
	for (i = 0; i < map._size; i++) {						\
		long d = ptr - map._map[i];							\
		if (d >= 0 && d < 1024)								\
			return ~(i * 1024 + d);							\
	}														\
	return 0
///@}

///@}

/** \defgroup pr_zone VM memory management.
	\ingroup progs

	Used to allocate and free memory in the VM address space.
*/
///@{

void PR_Zone_Init (progs_t *pr);
void PR_Zone_Free (progs_t *pr, void *ptr);
void *PR_Zone_Malloc (progs_t *pr, pr_int_t size);
void *PR_Zone_Realloc (progs_t *pr, void *ptr, pr_int_t size);

///@}

/** \defgroup debug VM Debugging
	\ingroup progs
	Progs debugging support.
*/
/// \addtogroup debug
///@{

struct qfot_type_s;

/**	Callback for viewing progs data

	\param type		C pointer to the type definition by which to view the
					data.
	\param value	C pointer to the data to be viewed.
	\param data		User data.
*/
typedef void (*type_view_func) (struct qfot_type_s *type, pr_type_t *value,
								void *data);

/**	Set of callbacks for viewing progs data

	Each possible type has its own callback. Basic types (those for which the
	VM has specific instructions) all have separate callbacks, one for each
	type, but the callbacks for compound types are expected to some
	interpretation on their own, such as displaying a simple identifier or
	the entire contents of the data.
*/
typedef struct type_view_s {
	type_view_func void_view;
	type_view_func string_view;
	type_view_func float_view;
	type_view_func vector_view;
	type_view_func entity_view;
	type_view_func field_view;
	type_view_func func_view;
	type_view_func pointer_view;
	type_view_func quat_view;
	type_view_func integer_view;
	type_view_func uinteger_view;
	type_view_func short_view;
	type_view_func double_view;

	type_view_func struct_view;
	type_view_func union_view;
	type_view_func enum_view;
	type_view_func array_view;
	type_view_func class_view;
	type_view_func alias_view;
} type_view_t;

void PR_Debug_Init (progs_t *pr);
void PR_Debug_Init_Cvars (void);
int PR_LoadDebug (progs_t *pr);
void PR_Debug_Watch (progs_t *pr, const char *expr);
void PR_Debug_Print (progs_t *pr, const char *expr);
pr_auxfunction_t *PR_Get_Lineno_Func (progs_t *pr, pr_lineno_t *lineno) __attribute__((pure));
pr_uint_t PR_Get_Lineno_Addr (progs_t *pr, pr_lineno_t *lineno) __attribute__((pure));
pr_uint_t PR_Get_Lineno_Line (progs_t *pr, pr_lineno_t *lineno) __attribute__((pure));
pr_lineno_t *PR_Find_Lineno (progs_t *pr, pr_uint_t addr) __attribute__((pure));
const char *PR_Get_Source_File (progs_t *pr, pr_lineno_t *lineno) __attribute__((pure));
const char *PR_Get_Source_Line (progs_t *pr, pr_uint_t addr);
pr_def_t *PR_Get_Param_Def (progs_t *pr, dfunction_t *func, unsigned parm) __attribute__((pure));
pr_def_t *PR_Get_Local_Def (progs_t *pr, pointer_t offs) __attribute__((pure));
void PR_PrintStatement (progs_t *pr, dstatement_t *s, int contents);
void PR_DumpState (progs_t *pr);
void PR_StackTrace (progs_t *pr);
void PR_Profile (progs_t *pr);

extern struct cvar_s *pr_debug;
extern struct cvar_s *pr_deadbeef_ents;
extern struct cvar_s *pr_deadbeef_locals;
extern struct cvar_s *pr_boundscheck;
extern struct cvar_s *pr_faultchecks;

///@}

/** \defgroup pr_cmds Quake and Quakeworld common builtins
	\ingroup progs
	\todo This really doesn't belong in progs.
*/
///@{

char *PF_VarString (progs_t *pr, int first);
void PR_Cmds_Init (progs_t *pr);

extern const char *pr_gametype;

///@}

//============================================================================

#define MAX_STACK_DEPTH		64
#define LOCALSTACK_SIZE		4096
#define PR_RS_SLOTS			16

typedef struct strref_s strref_t;

typedef struct {
	pr_int_t    s;					///< Return statement.
	bfunction_t *f;					///< Calling function.
	strref_t   *tstr;				///< Linked list of temporary strings.
} prstack_t;

struct obj_list_s;

struct progs_s {
	int       (*parse_field) (progs_t *pr, const char *key, const char *value);

	int         null_bad;
	int         no_exec_limit;

	void      (*file_error) (progs_t *pr, const char *path);
	void     *(*load_file) (progs_t *pr, const char *path, off_t *size);
	void     *(*allocate_progs_mem) (progs_t *pr, int size);
	void      (*free_progs_mem) (progs_t *pr, void *mem);

	int       (*resolve) (progs_t *pr);

	const char *progs_name;

	dprograms_t *progs;
	int         progs_size;
	unsigned short crc;
	int         denorm_found;

	struct memzone_s *zone;
	int         zone_size;			///< set by user

	/// \name builtin functions
	///@{
	struct hashtab_s *builtin_hash;
	struct hashtab_s *builtin_num_hash;
	unsigned    bi_next;
	unsigned  (*bi_map) (progs_t *pr, unsigned binum);
	///@}

	/// \name symbol management
	///@{
	struct hashtab_s *function_hash;
	struct hashtab_s *global_hash;
	struct hashtab_s *field_hash;
	///@}

	/// \name load hooks
	///@{
	int         num_load_funcs;
	int         max_load_funcs;
	pr_load_func_t **load_funcs;

	/// cleared each load
	///@{
	int         num_load_finish_funcs;
	int         max_load_finish_funcs;
	pr_load_func_t **load_finish_funcs;
	///@}
	///@}

	/// \name string management
	///@{
	struct prstr_resources_s *pr_string_resources;
	strref_t   *pr_xtstr;
	int         float_promoted;	///< for PR_Sprintf
	///@}

	/// \name memory map
	///@{
	dfunction_t *pr_functions;
	bfunction_t *function_table;
	char       *pr_strings;
	int         pr_stringsize;
	pr_def_t   *pr_globaldefs;
	pr_def_t   *pr_fielddefs;
	dstatement_t *pr_statements;
	pr_type_t  *pr_globals;
	unsigned    globals_size;
	///@}

	/// \name parameter block
	///@{
	pr_type_t  *pr_return;
	pr_type_t  *pr_params[MAX_PARMS];
	pr_type_t  *pr_real_params[MAX_PARMS];
	int         pr_param_size;		///< covers both params and return
	int         pr_param_alignment;	///< covers both params and return
	///@}

	/// \name edicts
	/// \todo FIXME should this be outside the VM?
	///@{
	edict_t   **edicts;
	int         max_edicts;			///< set by user
	int        *num_edicts;
	int        *reserved_edicts;	///< alloc will start at reserved_edicts+1
	void      (*unlink) (edict_t *ent);
	void      (*flush) (void);
	int       (*prune_edict) (progs_t *pr, edict_t *ent);
	void      (*free_edict) (progs_t *pr, edict_t *ent);
	int         pr_edict_size;		///< in bytes
	int         pr_edictareasize;	///< for bounds checking, starts at 0
	func_t      edict_parse;
	///@}

	/// \name execution state
	///@{
	int         pr_argc;

	qboolean    pr_trace;
	int         pr_trace_depth;
	bfunction_t *pr_xfunction;
	int         pr_xstatement;

	prstack_t   pr_stack[MAX_STACK_DEPTH];
	int         pr_depth;

	/// \name progs visible stack
	/// Usable by the progs for any purpose. Will not be accessible unless
	/// a .stack global is found. Space is allocated from the top of the stack
	/// (as is common for hardware). The push and pop instructions will not
	/// be considered valid if there is no .stack global.
	/// \note The return address and saved locals will not ever be on this
	/// stack.
	///@{
	pr_type_t  *stack;
	pointer_t   stack_bottom;
	int         stack_size;			///< set by user
	///@}

	int         localstack[LOCALSTACK_SIZE];
	int         localstack_used;
	///@}

	/// \name resources
	///@{
	pr_resource_t *resources;
	struct hashtab_s *resource_hash;
	///@}

	/// \name obj info
	///@{
	unsigned    selector_index;
	unsigned    selector_index_max;
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
	///@}

	/// \name debug info
	///@{
	const char *debugfile;
	struct pr_debug_header_s *debug;
	struct pr_auxfunction_s *auxfunctions;
	struct pr_auxfunction_s **auxfunction_map;
	struct pr_lineno_s *linenos;
	pr_def_t   *local_defs;
	pr_type_t  *watch;
	int         wp_conditional;
	pr_type_t   wp_val;
	///@}

	/// \name globals and fields needed by the VM
	///@{
	struct {
		float      *time;		///< required for OP_STATE
		pr_int_t   *self;		///< required for OP_STATE
		pointer_t  *stack;		///< required for OP_(PUSH|POP)*
	} globals;
	struct {
		pr_int_t    nextthink;	///< required for OP_STATE
		pr_int_t    frame;		///< required for OP_STATE
		pr_int_t    think;		///< required for OP_STATE
		pr_int_t    this;		///< optional for entity<->object linking
	} fields;
	///@}
};

/** \addtogroup progs_data_access
*/
///@{

/** Convert a progs offset/pointer to a C pointer.
	\param pr		pointer to ::progs_t VM struct
	\param o		offset into global data space
	\return			C pointer represented by the parameter. 0 offset -> NULL
*/
static inline pr_type_t *
PR_GetPointer (const progs_t *pr, pointer_t o)
{
	return o ? pr->pr_globals + o : 0;
}

/** Convert a C pointer to a progs offset/pointer.
	\param pr		pointer to ::progs_t VM struct
	\param p		C pointer to be converted.
	\return			Progs offset/pointer represented by \c p. NULL -> 0 offset
*/
static inline pointer_t
PR_SetPointer (const progs_t *pr, const void *p)
{
	return p ? (const pr_type_t *) p - pr->pr_globals : 0;
}

///@}

/** \example vm-exec.c
*/

#endif//__QF_progs_h
