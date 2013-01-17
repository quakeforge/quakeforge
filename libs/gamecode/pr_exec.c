/*
	pr_exec.c

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

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <signal.h>
#include <stdarg.h>

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/mathlib.h"
#include "QF/progs.h"
#include "QF/sys.h"
#include "QF/zone.h"

#include "compat.h"


/*
	PR_RunError

	Aborts the currently executing function
*/
VISIBLE void
PR_RunError (progs_t * pr, const char *error, ...)
{
	dstring_t  *string = dstring_new ();
	va_list     argptr;

	va_start (argptr, error);
	dvsprintf (string, error, argptr);
	va_end (argptr);

	Sys_Printf ("%s\n", string->str);

	PR_DumpState (pr);

	// dump the stack so PR_Error can shutdown functions
	pr->pr_depth = 0;

	PR_Error (pr, "Program error: %s", string->str);
}

VISIBLE void
PR_SaveParams (progs_t *pr)
{
	int         i;
	int         size = pr->pr_param_size * sizeof (pr_type_t);

	pr->pr_param_ptrs[0] = pr->pr_params[0];
	pr->pr_param_ptrs[1] = pr->pr_params[1];
	pr->pr_params[0] = pr->pr_real_params[0];
	pr->pr_params[1] = pr->pr_real_params[1];
	for (i = 0; i < pr->pr_argc; i++) {
		memcpy (pr->pr_saved_params + i * pr->pr_param_size,
				pr->pr_real_params[i], size);
		if (i < 2)
			memcpy (pr->pr_real_params[i], pr->pr_param_ptrs[0], size);
	}
	pr->pr_saved_argc = pr->pr_argc;
}

VISIBLE void
PR_RestoreParams (progs_t *pr)
{
	int         i;
	int         size = pr->pr_param_size * sizeof (pr_type_t);

	pr->pr_params[0] = pr->pr_param_ptrs[0];
	pr->pr_params[1] = pr->pr_param_ptrs[1];
	pr->pr_argc = pr->pr_saved_argc;
	for (i = 0; i < pr->pr_argc; i++)
		memcpy (pr->pr_real_params[i],
				pr->pr_saved_params + i * pr->pr_param_size, size);
}

VISIBLE inline void
PR_PushFrame (progs_t *pr)
{
	prstack_t  *frame;

	if (pr->pr_depth == MAX_STACK_DEPTH)
		PR_RunError (pr, "stack overflow");

	frame = pr->pr_stack + pr->pr_depth++;

	frame->s    = pr->pr_xstatement;
	frame->f    = pr->pr_xfunction;
	frame->tstr = pr->pr_xtstr;

	pr->pr_xtstr = 0;
	pr->pr_xfunction = 0;
}

VISIBLE inline void
PR_PopFrame (progs_t *pr)
{
	prstack_t  *frame;

	if (pr->pr_depth <= 0)
		PR_Error (pr, "prog stack underflow");

	if (pr->pr_xtstr)
		PR_FreeTempStrings (pr);

	// up stack
	frame = pr->pr_stack + --pr->pr_depth;

	pr->pr_xfunction  = frame->f;
	pr->pr_xstatement = frame->s;
	pr->pr_xtstr      = frame->tstr;
}

/** Setup the stackframe prior to calling a progs function. Saves all local
	data the called function will trample on and copies the parameters used
	by the function into the function's local data space.
	\param pr pointer to progs_t VM struct
	\param f pointer to the descriptor for the called function
	\note Passing a descriptor for a builtin function will result in
	undefined behavior.
*/
static void
PR_EnterFunction (progs_t *pr, bfunction_t *f)
{
	pr_int_t    i, j, c, o;
	pr_int_t    k;
	pr_int_t    count = 0;
	int         size[2] = {0, 0};
	long        paramofs = 0;
	long        offs;

	PR_PushFrame (pr);

	if (f->numparms > 0) {
		for (i = 0; i < 2 && i < f->numparms; i++) {
			paramofs += f->parm_size[i];
			size[i] = f->parm_size[i];
		}
		count = i;
	} else if (f->numparms < 0) {
		for (i = 0; i < 2 && i < -f->numparms - 1; i++) {
			paramofs += f->parm_size[i];
			size[i] = f->parm_size[i];
		}
		for (; i < 2; i++) {
			paramofs += pr->pr_param_size;
			size[i] = pr->pr_param_size;
		}
		count = i;
	}

	for (i = 0; i < count && i < pr->pr_argc; i++) {
		offs = (pr->pr_params[i] - pr->pr_globals) - f->parm_start;
		if (offs >= 0 && offs < paramofs) {
			memcpy (pr->pr_real_params[i], pr->pr_params[i],
					size[i] * sizeof (pr_type_t));
			pr->pr_params[i] = pr->pr_real_params[i];
		}
	}

	//Sys_Printf("%s:\n", PR_GetString(pr,f->s_name));
	pr->pr_xfunction = f;
	pr->pr_xstatement = f->first_statement - 1;      		// offset the st++

	// save off any locals that the new function steps on
	c = f->locals;
	if (pr->localstack_used + c > LOCALSTACK_SIZE)
		PR_RunError (pr, "PR_EnterFunction: locals stack overflow");

	memcpy (&pr->localstack[pr->localstack_used],
			&pr->pr_globals[f->parm_start],
			sizeof (pr_type_t) * c);
	pr->localstack_used += c;

	if (pr_deadbeef_locals->int_val)
		for (k = f->parm_start; k < f->parm_start + c; k++)
			pr->pr_globals[k].integer_var = 0xdeadbeef;

	// copy parameters
	o = f->parm_start;
	if (f->numparms >= 0) {
		for (i = 0; i < f->numparms; i++) {
			for (j = 0; j < f->parm_size[i]; j++) {
				memcpy (&pr->pr_globals[o], &P_INT (pr, i) + j,
						sizeof (pr_type_t));
				o++;
			}
		}
	} else {
		pr_type_t  *argc = &pr->pr_globals[o++];
		pr_type_t  *argv = &pr->pr_globals[o++];
		for (i = 0; i < -f->numparms - 1; i++) {
			for (j = 0; j < f->parm_size[i]; j++) {
				memcpy (&pr->pr_globals[o], &P_INT (pr, i) + j,
						sizeof (pr_type_t));
				o++;
			}
		}
		argc->integer_var = pr->pr_argc - i;
		argv->integer_var = o;
		if (i < MAX_PARMS) {
			memcpy (&pr->pr_globals[o], &P_INT (pr, i),
					(MAX_PARMS - i) * pr->pr_param_size * sizeof (pr_type_t));
		}
	}
}

static void
PR_LeaveFunction (progs_t *pr)
{
	int			c;
	bfunction_t *f = pr->pr_xfunction;

	PR_PopFrame (pr);

	// restore locals from the stack
	c = f->locals;
	pr->localstack_used -= c;
	if (pr->localstack_used < 0)
		PR_RunError (pr, "PR_LeaveFunction: locals stack underflow");

	memcpy (&pr->pr_globals[f->parm_start],
			&pr->localstack[pr->localstack_used], sizeof (pr_type_t) * c);
}

VISIBLE void
PR_BoundsCheckSize (progs_t *pr, pointer_t addr, unsigned size)
{
	if (addr < (pointer_t) (pr->pr_return - pr->pr_globals))
		PR_RunError (pr, "null pointer access");
	if (addr >= pr->globals_size
		|| size > (unsigned) (pr->globals_size - addr))
		PR_RunError (pr, "invalid memory access: %d (0 to %d-%d)", addr,
					 pr->globals_size, size);
	if (pr_boundscheck->int_val >= 2
		&& PR_GetPointer (pr, addr + size) > (pr_type_t *) pr->zone) {
		void       *mem = (void *) PR_GetPointer (pr, addr);
		Z_CheckPointer (pr->zone, mem, size * sizeof (pr_type_t));
	}
}

VISIBLE void
PR_BoundsCheck (progs_t *pr, int addr, etype_t type)
{
	PR_BoundsCheckSize (pr, addr, pr_type_size[type]);
}

#define OPA (*op_a)
#define OPB (*op_b)
#define OPC (*op_c)

/*
	This gets around the problem of needing to test for -0.0 but denormals
	causing exceptions (or wrong results for what we need) on the alpha.
*/
#define FNZ(x) ((x).uinteger_var && (x).uinteger_var != 0x80000000u)


static int
signal_hook (int sig, void *data)
{
	progs_t    *pr = (progs_t *) data;

	if (sig == SIGFPE && pr_faultchecks->int_val) {
		dstatement_t *st;
		pr_type_t  *op_a, *op_b, *op_c;

		st = pr->pr_statements + pr->pr_xstatement;
		op_a = pr->pr_globals + st->a;
		op_b = pr->pr_globals + st->b;
		op_c = pr->pr_globals + st->c;

		switch (st->op) {
			case OP_DIV_F:
				if ((OPA.integer_var & 0x80000000)
					^ (OPB.integer_var & 0x80000000))
					OPC.integer_var = 0xff7fffff;
				else
					OPC.integer_var = 0x7f7fffff;
				return 1;
			case OP_DIV_I:
				if (OPA.integer_var & 0x80000000)
					OPC.integer_var = -0x80000000;
				else
					OPC.integer_var = 0x7fffffff;
				return 1;
			case OP_MOD_I:
			case OP_MOD_F:
				OPC.integer_var = 0x00000000;
				return 1;
			default:
				break;
		}
	}
	PR_DumpState (pr);
	fflush (stdout);
	return 0;
}

VISIBLE int
PR_CallFunction (progs_t *pr, func_t fnum)
{
	bfunction_t *f;

	if (!fnum)
		PR_RunError (pr, "NULL function");
	f = pr->function_table + fnum;
	if (f->first_statement < 0) {
		// negative statements are built in functions
		f->func (pr);
		return 0;
	} else {
		PR_EnterFunction (pr, f);
		return 1;
	}
}

/*
	PR_ExecuteProgram

	The interpretation main loop
*/
VISIBLE void
PR_ExecuteProgram (progs_t * pr, func_t fnum)
{
	int         exitdepth, profile, startprofile;
	int         fldofs;
	pr_uint_t   pointer;
	dstatement_t *st;
	pr_type_t  *ptr;
	pr_type_t   old_val = {0}, *watch = 0;

	// make a stack frame
	exitdepth = pr->pr_depth;
	startprofile = profile = 0;

	Sys_PushSignalHook (signal_hook, pr);

	if (!PR_CallFunction (pr, fnum)) {
		// called a builtin instead of progs code
		Sys_PopSignalHook ();
		return;
	}
	st = pr->pr_statements + pr->pr_xstatement;

	if (pr->watch) {
		watch = pr->watch;
		old_val = *watch;
	}

	while (1) {
		pr_type_t  *op_a, *op_b, *op_c;

		st++;
		++pr->pr_xstatement;
		if (pr->pr_xstatement != st - pr->pr_statements)
			PR_RunError (pr, "internal error");
		if (++profile > 1000000 && !pr->no_exec_limit) {
			PR_RunError (pr, "runaway loop error");
		}

		op_a = pr->pr_globals + st->a;
		op_b = pr->pr_globals + st->b;
		op_c = pr->pr_globals + st->c;

		if (pr->pr_trace)
			PR_PrintStatement (pr, st, 1);

		switch (st->op) {
			case OP_ADD_F:
				OPC.float_var = OPA.float_var + OPB.float_var;
				break;
			case OP_ADD_V:
				VectorAdd (OPA.vector_var, OPB.vector_var, OPC.vector_var);
				break;
			case OP_ADD_Q:
				QuatAdd (OPA.quat_var, OPB.quat_var, OPC.quat_var);
				break;
			case OP_ADD_S:
				OPC.string_var = PR_CatStrings (pr,
												PR_GetString (pr,
															  OPA.string_var),
												PR_GetString (pr,
															  OPB.string_var));
				break;
			case OP_SUB_F:
				OPC.float_var = OPA.float_var - OPB.float_var;
				break;
			case OP_SUB_V:
				VectorSubtract (OPA.vector_var, OPB.vector_var, OPC.vector_var);
				break;
			case OP_SUB_Q:
				QuatSubtract (OPA.quat_var, OPB.quat_var, OPC.quat_var);
				break;
			case OP_MUL_F:
				OPC.float_var = OPA.float_var * OPB.float_var;
				break;
			case OP_MUL_V:
				OPC.float_var = DotProduct (OPA.vector_var, OPB.vector_var);
				break;
			case OP_MUL_FV:
				{
					// avoid issues with the likes of x = x.x * x;
					// makes for faster code, too
					float       scale = OPA.float_var;
					VectorScale (OPB.vector_var, scale, OPC.vector_var);
				}
				break;
			case OP_MUL_VF:
				{
					// avoid issues with the likes of x = x * x.x;
					// makes for faster code, too
					float       scale = OPB.float_var;
					VectorScale (OPA.vector_var, scale, OPC.vector_var);
				}
				break;
			case OP_MUL_Q:
				QuatMult (OPA.quat_var, OPB.quat_var, OPC.quat_var);
				break;
			case OP_MUL_QV:
				QuatMultVec (OPA.quat_var, OPB.vector_var, OPC.vector_var);
				break;
			case OP_MUL_FQ:
				{
					// avoid issues with the likes of x = x.s * x;
					// makes for faster code, too
					float       scale = OPA.float_var;
					QuatScale (OPB.quat_var, scale, OPC.quat_var);
				}
				break;
			case OP_MUL_QF:
				{
					// avoid issues with the likes of x = x * x.s;
					// makes for faster code, too
					float       scale = OPB.float_var;
					QuatScale (OPA.quat_var, scale, OPC.quat_var);
				}
				break;
			case OP_CONJ_Q:
				QuatConj (OPA.quat_var, OPC.quat_var);
				break;
			case OP_DIV_F:
				OPC.float_var = OPA.float_var / OPB.float_var;
				break;
			case OP_BITAND:
				OPC.float_var = (int) OPA.float_var & (int) OPB.float_var;
				break;
			case OP_BITOR:
				OPC.float_var = (int) OPA.float_var | (int) OPB.float_var;
				break;
			case OP_BITXOR_F:
				OPC.float_var = (int) OPA.float_var ^ (int) OPB.float_var;
				break;
			case OP_BITNOT_F:
				OPC.float_var = ~ (int) OPA.float_var;
				break;
			case OP_SHL_F:
				OPC.float_var = (int) OPA.float_var << (int) OPB.float_var;
				break;
			case OP_SHR_F:
				OPC.float_var = (int) OPA.float_var >> (int) OPB.float_var;
				break;
			case OP_SHL_I:
				OPC.integer_var = OPA.integer_var << OPB.integer_var;
				break;
			case OP_SHR_I:
				OPC.integer_var = OPA.integer_var >> OPB.integer_var;
				break;
			case OP_SHR_U:
				OPC.uinteger_var = OPA.uinteger_var >> OPB.integer_var;
				break;
			case OP_GE_F:
				OPC.float_var = OPA.float_var >= OPB.float_var;
				break;
			case OP_LE_F:
				OPC.float_var = OPA.float_var <= OPB.float_var;
				break;
			case OP_GT_F:
				OPC.float_var = OPA.float_var > OPB.float_var;
				break;
			case OP_LT_F:
				OPC.float_var = OPA.float_var < OPB.float_var;
				break;
			case OP_AND:	// OPA and OPB have to be float for -0.0
				OPC.integer_var = FNZ (OPA) && FNZ (OPB);
				break;
			case OP_OR:		// OPA and OPB have to be float for -0.0
				OPC.integer_var = FNZ (OPA) || FNZ (OPB);
				break;
			case OP_NOT_F:
				OPC.integer_var = !FNZ (OPA);
				break;
			case OP_NOT_V:
				OPC.integer_var = VectorIsZero (OPA.vector_var);
				break;
			case OP_NOT_Q:
				OPC.integer_var = QuatIsZero (OPA.quat_var);
				break;
			case OP_NOT_S:
				OPC.integer_var = !OPA.string_var ||
					!*PR_GetString (pr, OPA.string_var);
				break;
			case OP_NOT_FN:
				OPC.integer_var = !OPA.func_var;
				break;
			case OP_NOT_ENT:
				OPC.integer_var = !OPA.entity_var;
				break;
			case OP_EQ_F:
				OPC.integer_var = OPA.float_var == OPB.float_var;
				break;
			case OP_EQ_V:
				OPC.integer_var = VectorCompare (OPA.vector_var,
												 OPB.vector_var);
				break;
			case OP_EQ_Q:
				OPC.integer_var = QuatCompare (OPA.quat_var, OPB.quat_var);
				break;
			case OP_EQ_E:
				OPC.integer_var = OPA.integer_var == OPB.integer_var;
				break;
			case OP_EQ_FN:
				OPC.integer_var = OPA.func_var == OPB.func_var;
				break;
			case OP_NE_F:
				OPC.integer_var = OPA.float_var != OPB.float_var;
				break;
			case OP_NE_V:
				OPC.integer_var = !VectorCompare (OPA.vector_var,
												  OPB.vector_var);
				break;
			case OP_NE_Q:
				OPC.integer_var = !QuatCompare (OPA.quat_var, OPB.quat_var);
				break;
			case OP_LE_S:
			case OP_GE_S:
			case OP_LT_S:
			case OP_GT_S:
			case OP_NE_S:
			case OP_EQ_S:
				{
					int cmp = strcmp (PR_GetString (pr, OPA.string_var),
									  PR_GetString (pr, OPB.string_var));
					switch (st->op) {
						case OP_LE_S: cmp = (cmp <= 0); break;
						case OP_GE_S: cmp = (cmp >= 0); break;
						case OP_LT_S: cmp = (cmp < 0); break;
						case OP_GT_S: cmp = (cmp > 0); break;
						case OP_NE_S: break;
						case OP_EQ_S: cmp = !cmp; break;
						default:      break;
					}
					OPC.integer_var = cmp;
				}
				break;
			case OP_NE_E:
				OPC.integer_var = OPA.integer_var != OPB.integer_var;
				break;
			case OP_NE_FN:
				OPC.integer_var = OPA.func_var != OPB.func_var;
				break;

			// ==================
			case OP_STORE_F:
			case OP_STORE_ENT:
			case OP_STORE_FLD:			// integers
			case OP_STORE_S:
			case OP_STORE_FN:			// pointers
			case OP_STORE_I:
			case OP_STORE_P:
				OPB.integer_var = OPA.integer_var;
				break;
			case OP_STORE_V:
				VectorCopy (OPA.vector_var, OPB.vector_var);
				break;
			case OP_STORE_Q:
				QuatCopy (OPA.quat_var, OPB.quat_var);
				break;

			case OP_STOREP_F:
			case OP_STOREP_ENT:
			case OP_STOREP_FLD:		// integers
			case OP_STOREP_S:
			case OP_STOREP_FN:		// pointers
			case OP_STOREP_I:
			case OP_STOREP_P:
				pointer = OPB.integer_var;
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_integer);
				}
				ptr = pr->pr_globals + pointer;
				ptr->integer_var = OPA.integer_var;
				break;
			case OP_STOREP_V:
				pointer = OPB.integer_var;
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_vector);
				}
				ptr = pr->pr_globals + pointer;
				VectorCopy (OPA.vector_var, ptr->vector_var);
				break;
			case OP_STOREP_Q:
				pointer = OPB.integer_var;
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_quat);
				}
				ptr = pr->pr_globals + pointer;
				QuatCopy (OPA.quat_var, ptr->quat_var);
				break;

			case OP_ADDRESS:
				if (pr_boundscheck->int_val) {
					if (OPA.entity_var < 0
						|| OPA.entity_var >= pr->pr_edict_area_size)
						PR_RunError (pr, "Progs attempted to address an out "
									 "of bounds edict");
					if (OPA.entity_var == 0 && pr->null_bad)
						PR_RunError (pr, "assignment to world entity");
					if (OPB.uinteger_var >= pr->progs->entityfields)
						PR_RunError (pr, "Progs attempted to address an "
									 "invalid field in an edict");
				}
				fldofs = OPA.entity_var + OPB.integer_var;
				OPC.integer_var = &pr->pr_edict_area[fldofs] - pr->pr_globals;
				break;
			case OP_ADDRESS_VOID:
			case OP_ADDRESS_F:
			case OP_ADDRESS_V:
			case OP_ADDRESS_Q:
			case OP_ADDRESS_S:
			case OP_ADDRESS_ENT:
			case OP_ADDRESS_FLD:
			case OP_ADDRESS_FN:
			case OP_ADDRESS_I:
			case OP_ADDRESS_P:
				OPC.integer_var = st->a;
				break;

			case OP_LOAD_F:
			case OP_LOAD_FLD:
			case OP_LOAD_ENT:
			case OP_LOAD_S:
			case OP_LOAD_FN:
			case OP_LOAD_I:
			case OP_LOAD_P:
				if (pr_boundscheck->int_val) {
					if (OPA.entity_var < 0
						|| OPA.entity_var >= pr->pr_edict_area_size)
						PR_RunError (pr, "Progs attempted to read an out of "
									 "bounds edict number");
					if (OPB.uinteger_var >= pr->progs->entityfields)
						PR_RunError (pr, "Progs attempted to read an invalid "
									 "field in an edict");
				}
				fldofs = OPA.entity_var + OPB.integer_var;
				OPC.integer_var = pr->pr_edict_area[fldofs].integer_var;
				break;
			case OP_LOAD_V:
				if (pr_boundscheck->int_val) {
					if (OPA.entity_var < 0
						|| OPA.entity_var >= pr->pr_edict_area_size)
						PR_RunError (pr, "Progs attempted to read an out of "
									 "bounds edict number");
					if (OPB.uinteger_var + 2 >= pr->progs->entityfields)
						PR_RunError (pr, "Progs attempted to read an invalid "
									 "field in an edict");
				}
				fldofs = OPA.entity_var + OPB.integer_var;
				memcpy (&OPC, &pr->pr_edict_area[fldofs], 3 * sizeof (OPC));
				break;
			case OP_LOAD_Q:
				if (pr_boundscheck->int_val) {
					if (OPA.entity_var < 0
						|| OPA.entity_var >= pr->pr_edict_area_size)
						PR_RunError (pr, "Progs attempted to read an out of "
									 "bounds edict number");
					if (OPB.uinteger_var + 3 >= pr->progs->entityfields)
						PR_RunError (pr, "Progs attempted to read an invalid "
									 "field in an edict");
				}
				fldofs = OPA.entity_var + OPB.integer_var;
				memcpy (&OPC, &pr->pr_edict_area[fldofs], 4 * sizeof (OPC));
				break;

			case OP_LOADB_F:
			case OP_LOADB_S:
			case OP_LOADB_ENT:
			case OP_LOADB_FLD:
			case OP_LOADB_FN:
			case OP_LOADB_I:
			case OP_LOADB_P:
				pointer = OPA.integer_var + OPB.integer_var;
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_integer);
				}
				ptr = pr->pr_globals + pointer;
				OPC.integer_var = ptr->integer_var;
				break;
			case OP_LOADB_V:
				pointer = OPA.integer_var + OPB.integer_var;
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_vector);
				}
				ptr = pr->pr_globals + pointer;
				VectorCopy (ptr->vector_var, OPC.vector_var);
				break;
			case OP_LOADB_Q:
				pointer = OPA.integer_var + OPB.integer_var;
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_quat);
				}
				ptr = pr->pr_globals + pointer;
				QuatCopy (ptr->quat_var, OPC.quat_var);
				break;

			case OP_LOADBI_F:
			case OP_LOADBI_S:
			case OP_LOADBI_ENT:
			case OP_LOADBI_FLD:
			case OP_LOADBI_FN:
			case OP_LOADBI_I:
			case OP_LOADBI_P:
				pointer = OPA.integer_var + (short) st->b;
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_integer);
				}
				ptr = pr->pr_globals + pointer;
				OPC.integer_var = ptr->integer_var;
				break;
			case OP_LOADBI_V:
				pointer = OPA.integer_var + (short) st->b;
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_vector);
				}
				ptr = pr->pr_globals + pointer;
				VectorCopy (ptr->vector_var, OPC.vector_var);
				break;
			case OP_LOADBI_Q:
				pointer = OPA.integer_var + (short) st->b;
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_quat);
				}
				ptr = pr->pr_globals + pointer;
				QuatCopy (ptr->quat_var, OPC.quat_var);
				break;

			case OP_LEA:
				pointer = OPA.integer_var + OPB.integer_var;
				OPC.integer_var = pointer;
				break;

			case OP_LEAI:
				pointer = OPA.integer_var + (short) st->b;
				OPC.integer_var = pointer;
				break;

			case OP_STOREB_F:
			case OP_STOREB_S:
			case OP_STOREB_ENT:
			case OP_STOREB_FLD:
			case OP_STOREB_FN:
			case OP_STOREB_I:
			case OP_STOREB_P:
				pointer = OPB.integer_var + OPC.integer_var;
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_integer);
				}
				ptr = pr->pr_globals + pointer;
				ptr->integer_var = OPA.integer_var;
				break;
			case OP_STOREB_V:
				pointer = OPB.integer_var + OPC.integer_var;
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_vector);
				}
				ptr = pr->pr_globals + pointer;
				VectorCopy (OPA.vector_var, ptr->vector_var);
				break;
			case OP_STOREB_Q:
				pointer = OPB.integer_var + OPC.integer_var;
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_quat);
				}
				ptr = pr->pr_globals + pointer;
				QuatCopy (OPA.quat_var, ptr->quat_var);
				break;

			case OP_STOREBI_F:
			case OP_STOREBI_S:
			case OP_STOREBI_ENT:
			case OP_STOREBI_FLD:
			case OP_STOREBI_FN:
			case OP_STOREBI_I:
			case OP_STOREBI_P:
				pointer = OPB.integer_var + (short) st->c;
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_integer);
				}
				ptr = pr->pr_globals + pointer;
				ptr->integer_var = OPA.integer_var;
				break;
			case OP_STOREBI_V:
				pointer = OPB.integer_var + (short) st->c;
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_vector);
				}
				ptr = pr->pr_globals + pointer;
				VectorCopy (OPA.vector_var, ptr->vector_var);
				break;
			case OP_STOREBI_Q:
				pointer = OPB.integer_var + (short) st->c;
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_quat);
				}
				ptr = pr->pr_globals + pointer;
				QuatCopy (OPA.quat_var, ptr->quat_var);
				break;

			// ==================
			case OP_IFNOT:
				if (!OPA.integer_var) {
					pr->pr_xstatement += (short)st->b - 1;	// offset the st++
					st = pr->pr_statements + pr->pr_xstatement;
				}
				break;
			case OP_IF:
				if (OPA.integer_var) {
					pr->pr_xstatement += (short)st->b - 1;	// offset the st++
					st = pr->pr_statements + pr->pr_xstatement;
				}
				break;
			case OP_IFBE:
				if (OPA.integer_var <= 0) {
					pr->pr_xstatement += (short)st->b - 1;	// offset the st++
					st = pr->pr_statements + pr->pr_xstatement;
				}
				break;
			case OP_IFB:
				if (OPA.integer_var < 0) {
					pr->pr_xstatement += (short)st->b - 1;	// offset the st++
					st = pr->pr_statements + pr->pr_xstatement;
				}
				break;
			case OP_IFAE:
				if (OPA.integer_var >= 0) {
					pr->pr_xstatement += (short)st->b - 1;	// offset the st++
					st = pr->pr_statements + pr->pr_xstatement;
				}
				break;
			case OP_IFA:
				if (OPA.integer_var > 0) {
					pr->pr_xstatement += (short)st->b - 1;	// offset the st++
					st = pr->pr_statements + pr->pr_xstatement;
				}
				break;
			case OP_GOTO:
				pr->pr_xstatement += (short)st->a - 1;		// offset the st++
				st = pr->pr_statements + pr->pr_xstatement;
				break;
			case OP_JUMP:
				if (pr_boundscheck->int_val
					&& (OPA.uinteger_var >= pr->progs->numstatements)) {
					PR_RunError (pr, "Invalid jump destination");
				}
				pr->pr_xstatement = OPA.uinteger_var;
				st = pr->pr_statements + pr->pr_xstatement;
				break;
			case OP_JUMPB:
				pointer = st->a + OPB.integer_var;
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_integer);
				}
				ptr = pr->pr_globals + pointer;
				pointer = ptr->integer_var;
				if (pr_boundscheck->int_val
					&& (pointer >= pr->progs->numstatements)) {
					PR_RunError (pr, "Invalid jump destination");
				}
				pr->pr_xstatement = pointer - 1;			// offset the st++
				st = pr->pr_statements + pr->pr_xstatement;
				break;

			case OP_RCALL2:
			case OP_RCALL3:
			case OP_RCALL4:
			case OP_RCALL5:
			case OP_RCALL6:
			case OP_RCALL7:
			case OP_RCALL8:
				pr->pr_params[1] = &OPC;
				goto op_rcall;
			case OP_RCALL1:
				pr->pr_params[1] = pr->pr_real_params[1];
op_rcall:
				pr->pr_params[0] = &OPB;
				pr->pr_argc = st->op - OP_RCALL1 + 1;
				goto op_call;
			case OP_CALL0:
			case OP_CALL1:
			case OP_CALL2:
			case OP_CALL3:
			case OP_CALL4:
			case OP_CALL5:
			case OP_CALL6:
			case OP_CALL7:
			case OP_CALL8:
				PR_RESET_PARAMS (pr);
				pr->pr_argc = st->op - OP_CALL0;
op_call:
				pr->pr_xfunction->profile += profile - startprofile;
				startprofile = profile;
				PR_CallFunction (pr, OPA.func_var);
				st = pr->pr_statements + pr->pr_xstatement;
				break;
			case OP_DONE:
			case OP_RETURN:
				if (!st->a)
					memset (&R_INT (pr), 0,
							pr->pr_param_size * sizeof (OPA));
				else if (&R_INT (pr) != &OPA.integer_var)
					memcpy (&R_INT (pr), &OPA,
							pr->pr_param_size * sizeof (OPA));
				// fallthrough
			case OP_RETURN_V:
				pr->pr_xfunction->profile += profile - startprofile;
				startprofile = profile;
				PR_LeaveFunction (pr);
				st = pr->pr_statements + pr->pr_xstatement;
				if (pr->pr_depth == exitdepth) {
					if (pr->pr_trace && pr->pr_depth <= pr->pr_trace_depth)
						pr->pr_trace = false;
					Sys_PopSignalHook ();
					return;					// all done
				}
				break;
			case OP_STATE:
				{
					int         self = *pr->globals.self;
					int         nextthink = pr->fields.nextthink;
					int         frame = pr->fields.frame;
					int         think = pr->fields.think;
					fldofs = self + nextthink;
					pr->pr_edict_area[fldofs].float_var = *pr->globals.time +
															0.1;
					fldofs = self + frame;
					pr->pr_edict_area[fldofs].float_var = OPA.float_var;
					fldofs = self + think;
					pr->pr_edict_area[fldofs].func_var = OPB.func_var;
				}
				break;
			case OP_STATE_F:
				{
					int         self = *pr->globals.self;
					int         nextthink = pr->fields.nextthink;
					int         frame = pr->fields.frame;
					int         think = pr->fields.think;
					fldofs = self + nextthink;
					pr->pr_edict_area[fldofs].float_var = *pr->globals.time +
															OPC.float_var;
					fldofs = self + frame;
					pr->pr_edict_area[fldofs].float_var = OPA.float_var;
					fldofs = self + think;
					pr->pr_edict_area[fldofs].func_var = OPB.func_var;
				}
				break;
			case OP_ADD_I:
				OPC.integer_var = OPA.integer_var + OPB.integer_var;
				break;
			case OP_SUB_I:
				OPC.integer_var = OPA.integer_var - OPB.integer_var;
				break;
			case OP_MUL_I:
				OPC.integer_var = OPA.integer_var * OPB.integer_var;
				break;
/*
			case OP_DIV_VF:
				{
					float       temp = 1.0f / OPB.float_var;
					VectorScale (OPA.vector_var, temp, OPC.vector_var);
				}
				break;
*/
			case OP_DIV_I:
				OPC.integer_var = OPA.integer_var / OPB.integer_var;
				break;
			case OP_MOD_I:
				OPC.integer_var = OPA.integer_var % OPB.integer_var;
				break;
			case OP_MOD_F:
				OPC.float_var = (int) OPA.float_var % (int) OPB.float_var;
				break;
			case OP_CONV_IF:
				OPC.float_var = OPA.integer_var;
				break;
			case OP_CONV_FI:
				OPC.integer_var = OPA.float_var;
				break;
			case OP_BITAND_I:
				OPC.integer_var = OPA.integer_var & OPB.integer_var;
				break;
			case OP_BITOR_I:
				OPC.integer_var = OPA.integer_var | OPB.integer_var;
				break;
			case OP_BITXOR_I:
				OPC.integer_var = OPA.integer_var ^ OPB.integer_var;
				break;
			case OP_BITNOT_I:
				OPC.integer_var = ~OPA.integer_var;
				break;

			case OP_GE_I:
			case OP_GE_P:
				OPC.integer_var = OPA.integer_var >= OPB.integer_var;
				break;
			case OP_GE_U:
				OPC.integer_var = OPA.uinteger_var >= OPB.uinteger_var;
				break;
			case OP_LE_I:
			case OP_LE_P:
				OPC.integer_var = OPA.integer_var <= OPB.integer_var;
				break;
			case OP_LE_U:
				OPC.integer_var = OPA.uinteger_var <= OPB.uinteger_var;
				break;
			case OP_GT_I:
			case OP_GT_P:
				OPC.integer_var = OPA.integer_var > OPB.integer_var;
				break;
			case OP_GT_U:
				OPC.integer_var = OPA.uinteger_var > OPB.uinteger_var;
				break;
			case OP_LT_I:
			case OP_LT_P:
				OPC.integer_var = OPA.integer_var < OPB.integer_var;
				break;
			case OP_LT_U:
				OPC.integer_var = OPA.uinteger_var < OPB.uinteger_var;
				break;

			case OP_AND_I:
				OPC.integer_var = OPA.integer_var && OPB.integer_var;
				break;
			case OP_OR_I:
				OPC.integer_var = OPA.integer_var || OPB.integer_var;
				break;
			case OP_NOT_I:
			case OP_NOT_P:
				OPC.integer_var = !OPA.integer_var;
				break;
			case OP_EQ_I:
			case OP_EQ_P:
				OPC.integer_var = OPA.integer_var == OPB.integer_var;
				break;
			case OP_NE_I:
			case OP_NE_P:
				OPC.integer_var = OPA.integer_var != OPB.integer_var;
				break;

			case OP_MOVEI:
				memmove (&OPC, &OPA, st->b * 4);
				break;
			case OP_MOVEP:
				if (pr_boundscheck->int_val) {
					PR_BoundsCheckSize (pr, OPC.integer_var, OPB.uinteger_var);
					PR_BoundsCheckSize (pr, OPA.integer_var, OPB.uinteger_var);
				}
				memmove (pr->pr_globals + OPC.integer_var,
						 pr->pr_globals + OPA.integer_var,
						 OPB.uinteger_var * 4);
				break;
			case OP_MOVEPI:
				if (pr_boundscheck->int_val) {
					PR_BoundsCheckSize (pr, OPC.integer_var, st->b);
					PR_BoundsCheckSize (pr, OPA.integer_var, st->b);
				}
				memmove (pr->pr_globals + OPC.integer_var,
						 pr->pr_globals + OPA.integer_var,
						 st->b * 4);
				break;

// LordHavoc: to be enabled when Progs version 7 (or whatever it will be numbered) is finalized
/*
			case OP_BOUNDCHECK:
				if (OPA.integer_var < 0 || OPA.integer_var >= st->b) {
					PR_RunError (pr, "Progs boundcheck failed at line number "
					"%d, value is < 0 or >= %d", st->b, st->c);
				}
				break;

*/
			default:
				PR_RunError (pr, "Bad opcode %i", st->op);
		}
		if (watch && watch->integer_var != old_val.integer_var
			&& (!pr->wp_conditional
				|| watch->integer_var == pr->wp_val.integer_var))
			PR_RunError (pr, "watchpoint hit: %d -> %d", old_val.integer_var,
						 watch->integer_var);
	}
}
