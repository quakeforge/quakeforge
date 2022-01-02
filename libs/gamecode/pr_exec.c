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
	dstring_t  *string = dstring_new ();//FIXME leaks when debugging
	va_list     argptr;

	va_start (argptr, error);
	dvsprintf (string, error, argptr);
	va_end (argptr);

	if (pr->debug_handler) {
		pr->debug_handler (prd_runerror, string->str, pr->debug_data);
		// not expected to return, but if so, behave as if there was no handler
	}

	Sys_Printf ("%s\n", string->str);

	PR_DumpState (pr);

	// dump the stack so PR_Error can shutdown functions
	pr->pr_depth = 0;
	pr->localstack_used = 0;

	PR_Error (pr, "Program error: %s", string->str);
}

VISIBLE pr_stashed_params_t *
_PR_SaveParams (progs_t *pr, pr_stashed_params_t *params)
{
	int         i;
	int         size = pr->pr_param_size * sizeof (pr_type_t);

	params->param_ptrs[0] = pr->pr_params[0];
	params->param_ptrs[1] = pr->pr_params[1];
	pr->pr_params[0] = pr->pr_real_params[0];
	pr->pr_params[1] = pr->pr_real_params[1];
	for (i = 0; i < pr->pr_argc; i++) {
		memcpy (params->params + i * pr->pr_param_size,
				pr->pr_real_params[i], size);
		if (i < 2) { //XXX FIXME what the what?!?
			memcpy (pr->pr_real_params[i], params->param_ptrs[0], size);
		}
	}
	params->argc = pr->pr_argc;
	return params;
}

VISIBLE void
PR_RestoreParams (progs_t *pr, pr_stashed_params_t *params)
{
	int         i;
	int         size = pr->pr_param_size * sizeof (pr_type_t);

	pr->pr_params[0] = params->param_ptrs[0];
	pr->pr_params[1] = params->param_ptrs[1];
	pr->pr_argc = params->argc;
	for (i = 0; i < pr->pr_argc; i++) {
		memcpy (pr->pr_real_params[i],
				params->params + i * pr->pr_param_size, size);
	}
}

VISIBLE inline void
PR_PushFrame (progs_t *pr)
{
	prstack_t  *frame;

	if (pr->pr_depth == MAX_STACK_DEPTH)
		PR_RunError (pr, "stack overflow");

	frame = pr->pr_stack + pr->pr_depth++;

	frame->staddr = pr->pr_xstatement;
	frame->func   = pr->pr_xfunction;
	frame->tstr   = pr->pr_xtstr;

	pr->pr_xtstr = pr->pr_pushtstr;
	pr->pr_pushtstr = 0;
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
	// normally, this won't happen, but if a builtin pushed a temp string
	// when calling a function and the callee was another builtin that
	// did not call a progs function, then the push strings will still be
	// valid because PR_EnterFunction was never called
	// however, not if a temp string survived: better to hold on to the push
	// strings a little longer than lose one erroneously
	if (!pr->pr_xtstr && pr->pr_pushtstr) {
		pr->pr_xtstr = pr->pr_pushtstr;
		pr->pr_pushtstr = 0;
		PR_FreeTempStrings (pr);
	}

	// up stack
	frame = pr->pr_stack + --pr->pr_depth;

	pr->pr_xfunction  = frame->func;
	pr->pr_xstatement = frame->staddr;
	pr->pr_xtstr      = frame->tstr;
}

static __attribute__((pure)) long
align_offset (long offset, dparmsize_t parmsize)
{
	int         mask = (1 << parmsize.alignment) - 1;
	return (offset + mask) & ~mask;
}

static void
copy_param (pr_type_t *dst, pr_type_t *src, size_t size)
{
	while (size--) {
		memcpy (dst++, src++, sizeof (pr_type_t));
	}
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
	pr_int_t    i;
	pr_type_t  *dstParams[MAX_PARMS];
	pointer_t   paramofs = 0;

	if (pr->pr_trace && !pr->debug_handler) {
		Sys_Printf ("Entering function %s\n",
					PR_GetString (pr, f->descriptor->name));
	}

	PR_PushFrame (pr);

	if (f->numparms > 0) {
		paramofs = f->parm_start;
		for (i = 0; i < f->numparms; i++) {
			paramofs = align_offset (paramofs, f->parm_size[i]);
			dstParams[i] = pr->pr_globals + paramofs;
			paramofs += f->parm_size[i].size;
			if (pr->pr_params[i] != pr->pr_real_params[i]) {
				copy_param (pr->pr_real_params[i], pr->pr_params[i],
							f->parm_size[i].size);
				pr->pr_params[i] = pr->pr_real_params[i];
			}
		}
	} else if (f->numparms < 0) {
		paramofs = f->parm_start + 2;	// argc and argv
		for (i = 0; i < -f->numparms - 1; i++) {
			paramofs = align_offset (paramofs, f->parm_size[i]);
			dstParams[i] = pr->pr_globals + paramofs;
			paramofs += f->parm_size[i].size;
			if (pr->pr_params[i] != pr->pr_real_params[i]) {
				copy_param (pr->pr_real_params[i], pr->pr_params[i],
							f->parm_size[i].size);
				pr->pr_params[i] = pr->pr_real_params[i];
			}
		}
		dparmsize_t parmsize = { pr->pr_param_size, pr->pr_param_alignment };
		paramofs = align_offset (paramofs, parmsize );
		if (i < MAX_PARMS) {
			dstParams[i] = pr->pr_globals + paramofs;
		}
		for (; i < pr->pr_argc; i++) {
			if (pr->pr_params[i] != pr->pr_real_params[i]) {
				copy_param (pr->pr_real_params[i], pr->pr_params[i],
							parmsize.size);
				pr->pr_params[i] = pr->pr_real_params[i];
			}
		}
	}

	//Sys_Printf("%s:\n", PR_GetString(pr,f->name));
	pr->pr_xfunction = f;
	pr->pr_xstatement = f->first_statement - 1;      		// offset the st++

	// save off any locals that the new function steps on
	if (pr->localstack_used + f->locals > LOCALSTACK_SIZE)
		PR_RunError (pr, "PR_EnterFunction: locals stack overflow");

	memcpy (&pr->localstack[pr->localstack_used],
			&pr->pr_globals[f->parm_start],
			sizeof (pr_type_t) * f->locals);
	pr->localstack_used += f->locals;

	if (pr_deadbeef_locals->int_val)
		for (i = f->parm_start; i < f->parm_start + f->locals; i++)
			pr->pr_globals[i].integer_var = 0xdeadbeef;

	// copy parameters
	if (f->numparms >= 0) {
		for (i = 0; i < f->numparms; i++) {
			copy_param (dstParams[i], pr->pr_params[i], f->parm_size[i].size);
		}
	} else {
		int         copy_args;
		pr_type_t  *argc = &pr->pr_globals[f->parm_start + 0];
		pr_type_t  *argv = &pr->pr_globals[f->parm_start + 1];
		for (i = 0; i < -f->numparms - 1; i++) {
			copy_param (dstParams[i], pr->pr_params[i], f->parm_size[i].size);
		}
		copy_args = pr->pr_argc - i;
		argc->integer_var = copy_args;
		argv->integer_var = dstParams[i] - pr->pr_globals;
		if (i < MAX_PARMS) {
			memcpy (dstParams[i], pr->pr_params[i],
					(copy_args * pr->pr_param_size) * sizeof (pr_type_t));
		}
	}
}

static void
PR_LeaveFunction (progs_t *pr, int to_engine)
{
	bfunction_t *f = pr->pr_xfunction;

	PR_PopFrame (pr);

	if (pr->pr_trace && !pr->debug_handler) {
		Sys_Printf ("Leaving function %s\n",
					PR_GetString (pr, f->descriptor->name));
		if (to_engine) {
			Sys_Printf ("Returning to engine\n");
		} else {
			bfunction_t *rf = pr->pr_xfunction;
			if (rf) {
				Sys_Printf ("Returning to function %s\n",
							PR_GetString (pr, rf->descriptor->name));
			}
		}
	}

	// restore locals from the stack
	pr->localstack_used -= f->locals;
	if (pr->localstack_used < 0)
		PR_RunError (pr, "PR_LeaveFunction: locals stack underflow");

	memcpy (&pr->pr_globals[f->parm_start],
			&pr->localstack[pr->localstack_used],
			sizeof (pr_type_t) * f->locals);
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

#define OPA(type) (*((pr_##type##_t *) (op_a)))
#define OPB(type) (*((pr_##type##_t *) (op_b)))
#define OPC(type) (*((pr_##type##_t *) (op_c)))

/*
	This gets around the problem of needing to test for -0.0 but denormals
	causing exceptions (or wrong results for what we need) on the alpha.
*/
#define FNZ(x) ((x) & ~0x80000000u)

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
				if ((OPA(int) & 0x80000000)
					^ (OPB(int) & 0x80000000))
					OPC(int) = 0xff7fffff;
				else
					OPC(int) = 0x7f7fffff;
				return 1;
			case OP_DIV_I:
				if (OPA(int) & 0x80000000)
					OPC(int) = -0x80000000;
				else
					OPC(int) = 0x7fffffff;
				return 1;
			case OP_MOD_I:
			case OP_MOD_F:
			case OP_REM_I:
			case OP_REM_F:
				OPC(int) = 0x00000000;
				return 1;
			default:
				break;
		}
	}
	PR_DumpState (pr);
	fflush (stdout);
	return 0;
}

static void
error_handler (void *data)
{
	progs_t    *pr = (progs_t *) data;
	PR_DumpState (pr);
	fflush (stdout);
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
		if (pr->pr_trace && !pr->debug_handler) {
			Sys_Printf ("Calling builtin %s @ %p\n",
						PR_GetString (pr, f->descriptor->name), f->func);
		}
		f->func (pr);
		return 0;
	} else {
		PR_EnterFunction (pr, f);
		return 1;
	}
}

static void
check_stack_pointer (progs_t *pr, pointer_t stack, int size)
{
	if (stack < pr->stack_bottom) {
		PR_RunError (pr, "Progs stack overflow");
	}
	if (stack > pr->globals_size - size) {
		PR_RunError (pr, "Progs stack underflow");
	}
}

static inline void
pr_memset (pr_type_t *dst, int val, int count)
{
	while (count-- > 0) {
		(*dst++).integer_var = val;
	}
}

static void
pr_exec_quakec (progs_t *pr, int exitdepth)
{
	int         profile, startprofile;
	int         fldofs;
	pr_uint_t   pointer;
	dstatement_t *st;
	pr_type_t  *ptr;
	pr_type_t   old_val = {0};

	// make a stack frame
	startprofile = profile = 0;

	st = pr->pr_statements + pr->pr_xstatement;

	if (pr->watch) {
		old_val = *pr->watch;
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

		if (pr->pr_trace) {
			if (pr->debug_handler) {
				pr->debug_handler (prd_trace, 0, pr->debug_data);
			} else {
				PR_PrintStatement (pr, st, 1);
			}
		}

		if (st->op & OP_BREAK) {
			if (pr->debug_handler) {
				pr->debug_handler (prd_breakpoint, 0, pr->debug_data);
			} else {
				PR_RunError (pr, "breakpoint hit");
			}
		}

		pr_opcode_e op = st->op & ~OP_BREAK;
		switch (op) {
			case OP_ADD_D:
				OPC(double) = OPA(double) + OPB(double);
				break;
			case OP_ADD_F:
				OPC(float) = OPA(float) + OPB(float);
				break;
			case OP_ADD_V:
				VectorAdd (&OPA(float), &OPB(float), &OPC(float));
				break;
			case OP_ADD_Q:
				QuatAdd (&OPA(float), &OPB(float), &OPC(float));
				break;
			case OP_ADD_S:
				OPC(string) = PR_CatStrings (pr,
												PR_GetString (pr,
															  OPA(string)),
												PR_GetString (pr,
															  OPB(string)));
				break;
			case OP_SUB_D:
				OPC(double) = OPA(double) - OPB(double);
				break;
			case OP_SUB_F:
				OPC(float) = OPA(float) - OPB(float);
				break;
			case OP_SUB_V:
				VectorSubtract (&OPA(float), &OPB(float),
								&OPC(float));
				break;
			case OP_SUB_Q:
				QuatSubtract (&OPA(float), &OPB(float), &OPC(float));
				break;
			case OP_MUL_D:
				OPC(double) = OPA(double) * OPB(double);
				break;
			case OP_MUL_F:
				OPC(float) = OPA(float) * OPB(float);
				break;
			case OP_MUL_V:
				OPC(float) = DotProduct (&OPA(float), &OPB(float));
				break;
			case OP_MUL_DV:
				{
					// avoid issues with the likes of x = x.x * x;
					// makes for faster code, too
					double      scale = OPA(double);
					VectorScale (&OPB(float), scale, &OPC(float));
				}
				break;
			case OP_MUL_VD:
				{
					// avoid issues with the likes of x = x * x.x;
					// makes for faster code, too
					double      scale = OPB(double);
					VectorScale (&OPA(float), scale, &OPC(float));
				}
				break;
			case OP_MUL_FV:
				{
					// avoid issues with the likes of x = x.x * x;
					// makes for faster code, too
					float       scale = OPA(float);
					VectorScale (&OPB(float), scale, &OPC(float));
				}
				break;
			case OP_MUL_VF:
				{
					// avoid issues with the likes of x = x * x.x;
					// makes for faster code, too
					float       scale = OPB(float);
					VectorScale (&OPA(float), scale, &OPC(float));
				}
				break;
			case OP_MUL_Q:
				QuatMult (&OPA(float), &OPB(float), &OPC(float));
				break;
			case OP_MUL_QV:
				QuatMultVec (&OPA(float), &OPB(float), &OPC(float));
				break;
			case OP_MUL_DQ:
				{
					// avoid issues with the likes of x = x.s * x;
					// makes for faster code, too
					double      scale = OPA(double);
					QuatScale (&OPB(float), scale, &OPC(float));
				}
				break;
			case OP_MUL_QD:
				{
					// avoid issues with the likes of x = x * x.s;
					// makes for faster code, too
					double      scale = OPB(double);
					QuatScale (&OPA(float), scale, &OPC(float));
				}
				break;
			case OP_MUL_FQ:
				{
					// avoid issues with the likes of x = x.s * x;
					// makes for faster code, too
					float       scale = OPA(float);
					QuatScale (&OPB(float), scale, &OPC(float));
				}
				break;
			case OP_MUL_QF:
				{
					// avoid issues with the likes of x = x * x.s;
					// makes for faster code, too
					float       scale = OPB(float);
					QuatScale (&OPA(float), scale, &OPC(float));
				}
				break;
			case OP_CONJ_Q:
				QuatConj (&OPA(float), &OPC(float));
				break;
			case OP_DIV_D:
				OPC(double) = OPA(double) / OPB(double);
				break;
			case OP_DIV_F:
				OPC(float) = OPA(float) / OPB(float);
				break;
			case OP_BITAND:
				OPC(float) = (int) OPA(float) & (int) OPB(float);
				break;
			case OP_BITOR:
				OPC(float) = (int) OPA(float) | (int) OPB(float);
				break;
			case OP_BITXOR_F:
				OPC(float) = (int) OPA(float) ^ (int) OPB(float);
				break;
			case OP_BITNOT_F:
				OPC(float) = ~ (int) OPA(float);
				break;
			case OP_SHL_F:
				OPC(float) = (int) OPA(float) << (int) OPB(float);
				break;
			case OP_SHR_F:
				OPC(float) = (int) OPA(float) >> (int) OPB(float);
				break;
			case OP_SHL_I:
				OPC(int) = OPA(int) << OPB(int);
				break;
			case OP_SHR_I:
				OPC(int) = OPA(int) >> OPB(int);
				break;
			case OP_SHR_U:
				OPC(uint) = OPA(uint) >> OPB(int);
				break;
			case OP_GE_F:
				OPC(float) = OPA(float) >= OPB(float);
				break;
			case OP_LE_F:
				OPC(float) = OPA(float) <= OPB(float);
				break;
			case OP_GT_F:
				OPC(float) = OPA(float) > OPB(float);
				break;
			case OP_LT_F:
				OPC(float) = OPA(float) < OPB(float);
				break;
			case OP_AND:	// OPA and OPB have to be float for -0.0
				OPC(int) = FNZ (OPA(uint)) && FNZ (OPB(uint));
				break;
			case OP_OR:		// OPA and OPB have to be float for -0.0
				OPC(int) = FNZ (OPA(uint)) || FNZ (OPB(uint));
				break;
			case OP_NOT_F:
				OPC(int) = !FNZ (OPA(uint));
				break;
			case OP_NOT_V:
				OPC(int) = VectorIsZero (&OPA(float));
				break;
			case OP_NOT_Q:
				OPC(int) = QuatIsZero (&OPA(float));
				break;
			case OP_NOT_S:
				OPC(int) = !OPA(string) ||
					!*PR_GetString (pr, OPA(string));
				break;
			case OP_NOT_FN:
				OPC(int) = !OPA(uint);
				break;
			case OP_NOT_ENT:
				OPC(int) = !OPA(uint);
				break;
			case OP_EQ_F:
				OPC(int) = OPA(float) == OPB(float);
				break;
			case OP_EQ_V:
				OPC(int) = VectorCompare (&OPA(float),
												 &OPB(float));
				break;
			case OP_EQ_Q:
				OPC(int) = QuatCompare (&OPA(float), &OPB(float));
				break;
			case OP_EQ_E:
				OPC(int) = OPA(int) == OPB(int);
				break;
			case OP_EQ_FN:
				OPC(int) = OPA(uint) == OPB(uint);
				break;
			case OP_NE_F:
				OPC(int) = OPA(float) != OPB(float);
				break;
			case OP_NE_V:
				OPC(int) = !VectorCompare (&OPA(float),
												  &OPB(float));
				break;
			case OP_NE_Q:
				OPC(int) = !QuatCompare (&OPA(float), &OPB(float));
				break;
			case OP_LE_S:
			case OP_GE_S:
			case OP_LT_S:
			case OP_GT_S:
			case OP_NE_S:
			case OP_EQ_S:
				{
					int cmp = strcmp (PR_GetString (pr, OPA(string)),
									  PR_GetString (pr, OPB(string)));
					switch (st->op) {
						case OP_LE_S: cmp = (cmp <= 0); break;
						case OP_GE_S: cmp = (cmp >= 0); break;
						case OP_LT_S: cmp = (cmp < 0); break;
						case OP_GT_S: cmp = (cmp > 0); break;
						case OP_NE_S: break;
						case OP_EQ_S: cmp = !cmp; break;
						default:      break;
					}
					OPC(int) = cmp;
				}
				break;
			case OP_NE_E:
				OPC(int) = OPA(int) != OPB(int);
				break;
			case OP_NE_FN:
				OPC(int) = OPA(uint) != OPB(uint);
				break;

			// ==================
			case OP_STORE_F:
			case OP_STORE_ENT:
			case OP_STORE_FLD:			// integers
			case OP_STORE_S:
			case OP_STORE_FN:			// pointers
			case OP_STORE_I:
			case OP_STORE_P:
				OPB(int) = OPA(int);
				break;
			case OP_STORE_V:
				VectorCopy (&OPA(float), &OPB(float));
				break;
			case OP_STORE_Q:
				QuatCopy (&OPA(float), &OPB(float));
				break;
			case OP_STORE_D:
				OPB(double) = OPA(double);
				break;

			case OP_STOREP_F:
			case OP_STOREP_ENT:
			case OP_STOREP_FLD:		// integers
			case OP_STOREP_S:
			case OP_STOREP_FN:		// pointers
			case OP_STOREP_I:
			case OP_STOREP_P:
				pointer = OPB(int);
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_integer);
				}
				ptr = pr->pr_globals + pointer;
				ptr->integer_var = OPA(int);
				break;
			case OP_STOREP_V:
				pointer = OPB(int);
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_vector);
				}
				ptr = pr->pr_globals + pointer;
				VectorCopy (&OPA(float), &ptr->vector_var);
				break;
			case OP_STOREP_Q:
				pointer = OPB(int);
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_quat);
				}
				ptr = pr->pr_globals + pointer;
				QuatCopy (&OPA(float), &ptr->quat_var);
				break;
			case OP_STOREP_D:
				pointer = OPB(int);
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_double);
				}
				ptr = pr->pr_globals + pointer;
				*(double *) ptr = OPA(double);
				break;

			case OP_ADDRESS:
				if (pr_boundscheck->int_val) {
					if (OPA(uint) >= pr->pr_edict_area_size)
						PR_RunError (pr, "Progs attempted to address an out "
									 "of bounds edict");
					if (OPA(uint) == 0 && pr->null_bad)
						PR_RunError (pr, "assignment to world entity");
					if (OPB(uint) >= pr->progs->entityfields)
						PR_RunError (pr, "Progs attempted to address an "
									 "invalid field in an edict");
				}
				fldofs = OPA(uint) + OPB(int);
				OPC(int) = &pr->pr_edict_area[fldofs] - pr->pr_globals;
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
			case OP_ADDRESS_D:
				OPC(int) = st->a;
				break;

			case OP_LOAD_F:
			case OP_LOAD_FLD:
			case OP_LOAD_ENT:
			case OP_LOAD_S:
			case OP_LOAD_FN:
			case OP_LOAD_I:
			case OP_LOAD_P:
				if (pr_boundscheck->int_val) {
					if (OPA(uint) >= pr->pr_edict_area_size)
						PR_RunError (pr, "Progs attempted to read an out of "
									 "bounds edict number");
					if (OPB(uint) >= pr->progs->entityfields)
						PR_RunError (pr, "Progs attempted to read an invalid "
									 "field in an edict");
				}
				fldofs = OPA(uint) + OPB(int);
				OPC(int) = pr->pr_edict_area[fldofs].integer_var;
				break;
			case OP_LOAD_V:
				if (pr_boundscheck->int_val) {
					if (OPA(uint) >= pr->pr_edict_area_size)
						PR_RunError (pr, "Progs attempted to read an out of "
									 "bounds edict number");
					if (OPB(uint) + 2 >= pr->progs->entityfields)
						PR_RunError (pr, "Progs attempted to read an invalid "
									 "field in an edict");
				}
				fldofs = OPA(uint) + OPB(int);
				memcpy (op_c, &pr->pr_edict_area[fldofs], 3 * sizeof (*op_c));
				break;
			case OP_LOAD_Q:
				if (pr_boundscheck->int_val) {
					if (OPA(uint) >= pr->pr_edict_area_size)
						PR_RunError (pr, "Progs attempted to read an out of "
									 "bounds edict number");
					if (OPB(uint) + 3 >= pr->progs->entityfields)
						PR_RunError (pr, "Progs attempted to read an invalid "
									 "field in an edict");
				}
				fldofs = OPA(uint) + OPB(int);
				memcpy (op_c, &pr->pr_edict_area[fldofs], 4 * sizeof (*op_c));
				break;
			case OP_LOAD_D:
				if (pr_boundscheck->int_val) {
					if (OPA(uint) >= pr->pr_edict_area_size)
						PR_RunError (pr, "Progs attempted to read an out of "
									 "bounds edict number");
					if (OPB(uint) + 1 >= pr->progs->entityfields)
						PR_RunError (pr, "Progs attempted to read an invalid "
									 "field in an edict");
				}
				fldofs = OPA(uint) + OPB(int);
				memcpy (op_c, &pr->pr_edict_area[fldofs], sizeof (double));
				break;

			case OP_LOADB_F:
			case OP_LOADB_S:
			case OP_LOADB_ENT:
			case OP_LOADB_FLD:
			case OP_LOADB_FN:
			case OP_LOADB_I:
			case OP_LOADB_P:
				pointer = OPA(int) + OPB(int);
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_integer);
				}
				ptr = pr->pr_globals + pointer;
				OPC(int) = ptr->integer_var;
				break;
			case OP_LOADB_V:
				pointer = OPA(int) + OPB(int);
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_vector);
				}
				ptr = pr->pr_globals + pointer;
				VectorCopy (&ptr->vector_var, &OPC(float));
				break;
			case OP_LOADB_Q:
				pointer = OPA(int) + OPB(int);
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_quat);
				}
				ptr = pr->pr_globals + pointer;
				QuatCopy (&ptr->quat_var, &OPC(float));
				break;
			case OP_LOADB_D:
				pointer = OPA(int) + OPB(int);
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_double);
				}
				ptr = pr->pr_globals + pointer;
				OPC(double) = *(double *) ptr;
				break;

			case OP_LOADBI_F:
			case OP_LOADBI_S:
			case OP_LOADBI_ENT:
			case OP_LOADBI_FLD:
			case OP_LOADBI_FN:
			case OP_LOADBI_I:
			case OP_LOADBI_P:
				pointer = OPA(int) + (short) st->b;
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_integer);
				}
				ptr = pr->pr_globals + pointer;
				OPC(int) = ptr->integer_var;
				break;
			case OP_LOADBI_V:
				pointer = OPA(int) + (short) st->b;
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_vector);
				}
				ptr = pr->pr_globals + pointer;
				VectorCopy (&ptr->vector_var, &OPC(float));
				break;
			case OP_LOADBI_Q:
				pointer = OPA(int) + (short) st->b;
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_quat);
				}
				ptr = pr->pr_globals + pointer;
				QuatCopy (&ptr->quat_var, &OPC(float));
				break;
			case OP_LOADBI_D:
				pointer = OPA(int) + (short) st->b;
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_quat);
				}
				ptr = pr->pr_globals + pointer;
				OPC(double) = *(double *) ptr;
				break;

			case OP_LEA:
				pointer = OPA(int) + OPB(int);
				OPC(int) = pointer;
				break;

			case OP_LEAI:
				pointer = OPA(int) + (short) st->b;
				OPC(int) = pointer;
				break;

			case OP_STOREB_F:
			case OP_STOREB_S:
			case OP_STOREB_ENT:
			case OP_STOREB_FLD:
			case OP_STOREB_FN:
			case OP_STOREB_I:
			case OP_STOREB_P:
				pointer = OPB(int) + OPC(int);
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_integer);
				}
				ptr = pr->pr_globals + pointer;
				ptr->integer_var = OPA(int);
				break;
			case OP_STOREB_V:
				pointer = OPB(int) + OPC(int);
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_vector);
				}
				ptr = pr->pr_globals + pointer;
				VectorCopy (&OPA(float), &ptr->vector_var);
				break;
			case OP_STOREB_Q:
				pointer = OPB(int) + OPC(int);
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_quat);
				}
				ptr = pr->pr_globals + pointer;
				QuatCopy (&OPA(float), &ptr->quat_var);
				break;
			case OP_STOREB_D:
				pointer = OPB(int) + OPC(int);
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_quat);
				}
				ptr = pr->pr_globals + pointer;
				*(double *) ptr = OPA(double);
				break;

			case OP_STOREBI_F:
			case OP_STOREBI_S:
			case OP_STOREBI_ENT:
			case OP_STOREBI_FLD:
			case OP_STOREBI_FN:
			case OP_STOREBI_I:
			case OP_STOREBI_P:
				pointer = OPB(int) + (short) st->c;
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_integer);
				}
				ptr = pr->pr_globals + pointer;
				ptr->integer_var = OPA(int);
				break;
			case OP_STOREBI_V:
				pointer = OPB(int) + (short) st->c;
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_vector);
				}
				ptr = pr->pr_globals + pointer;
				VectorCopy (&OPA(float), &ptr->vector_var);
				break;
			case OP_STOREBI_Q:
				pointer = OPB(int) + (short) st->c;
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_quat);
				}
				ptr = pr->pr_globals + pointer;
				QuatCopy (&OPA(float), &ptr->quat_var);
				break;
			case OP_STOREBI_D:
				pointer = OPB(int) + (short) st->c;
				if (pr_boundscheck->int_val) {
					PR_BoundsCheck (pr, pointer, ev_quat);
				}
				ptr = pr->pr_globals + pointer;
				*(double *) ptr = OPA(double);
				break;

			case OP_PUSH_F:
			case OP_PUSH_FLD:
			case OP_PUSH_ENT:
			case OP_PUSH_S:
			case OP_PUSH_FN:
			case OP_PUSH_I:
			case OP_PUSH_P:
				{
					pointer_t   stack = *pr->globals.stack - 1;
					pr_type_t  *stk = pr->pr_globals + stack;
					if (pr_boundscheck->int_val) {
						check_stack_pointer (pr, stack, 1);
					}
					stk->integer_var = OPA(int);
					*pr->globals.stack = stack;
				}
				break;
			case OP_PUSH_V:
				{
					pointer_t   stack = *pr->globals.stack - 3;
					pr_type_t  *stk = pr->pr_globals + stack;
					if (pr_boundscheck->int_val) {
						check_stack_pointer (pr, stack, 3);
					}
					memcpy (stk, op_a, 3 * sizeof (*op_c));
					*pr->globals.stack = stack;
				}
				break;
			case OP_PUSH_Q:
				{
					pointer_t   stack = *pr->globals.stack - 4;
					pr_type_t  *stk = pr->pr_globals + stack;
					if (pr_boundscheck->int_val) {
						check_stack_pointer (pr, stack, 4);
					}
					memcpy (stk, op_a, 4 * sizeof (*op_c));
					*pr->globals.stack = stack;
				}
				break;

			case OP_PUSHB_F:
			case OP_PUSHB_S:
			case OP_PUSHB_ENT:
			case OP_PUSHB_FLD:
			case OP_PUSHB_FN:
			case OP_PUSHB_I:
			case OP_PUSHB_P:
				{
					pointer_t   stack = *pr->globals.stack - 1;
					pr_type_t  *stk = pr->pr_globals + stack;

					pointer = OPA(int) + OPB(int);
					ptr = pr->pr_globals + pointer;

					if (pr_boundscheck->int_val) {
						check_stack_pointer (pr, stack, 1);
						PR_BoundsCheck (pr, pointer, ev_integer);
					}

					stk->integer_var = ptr->integer_var;
					*pr->globals.stack = stack;
				}
				break;
			case OP_PUSHB_V:
				{
					pointer_t   stack = *pr->globals.stack - 3;
					pr_type_t  *stk = pr->pr_globals + stack;

					pointer = OPA(int) + OPB(int);
					ptr = pr->pr_globals + pointer;

					if (pr_boundscheck->int_val) {
						check_stack_pointer (pr, stack, 3);
						PR_BoundsCheck (pr, pointer, ev_integer);
					}

					VectorCopy (&ptr->vector_var, &stk->vector_var);
					*pr->globals.stack = stack;
				}
				break;
			case OP_PUSHB_Q:
				{
					pointer_t   stack = *pr->globals.stack - 4;
					pr_type_t  *stk = pr->pr_globals + stack;

					pointer = OPA(int) + OPB(int);
					ptr = pr->pr_globals + pointer;

					if (pr_boundscheck->int_val) {
						check_stack_pointer (pr, stack, 4);
						PR_BoundsCheck (pr, pointer, ev_quat);
					}

					QuatCopy (&ptr->quat_var, &stk->quat_var);
					*pr->globals.stack = stack;
				}
				break;

			case OP_PUSHBI_F:
			case OP_PUSHBI_S:
			case OP_PUSHBI_ENT:
			case OP_PUSHBI_FLD:
			case OP_PUSHBI_FN:
			case OP_PUSHBI_I:
			case OP_PUSHBI_P:
				{
					pointer_t   stack = *pr->globals.stack - 1;
					pr_type_t  *stk = pr->pr_globals + stack;

					pointer = OPA(int) + st->b;
					ptr = pr->pr_globals + pointer;

					if (pr_boundscheck->int_val) {
						check_stack_pointer (pr, stack, 1);
						PR_BoundsCheck (pr, pointer, ev_integer);
					}

					stk->integer_var = ptr->integer_var;
					*pr->globals.stack = stack;
				}
				break;
			case OP_PUSHBI_V:
				{
					pointer_t   stack = *pr->globals.stack - 3;
					pr_type_t  *stk = pr->pr_globals + stack;

					pointer = OPA(int) + st->b;
					ptr = pr->pr_globals + pointer;

					if (pr_boundscheck->int_val) {
						check_stack_pointer (pr, stack, 3);
						PR_BoundsCheck (pr, pointer, ev_integer);
					}

					VectorCopy (&ptr->vector_var, &stk->vector_var);
					*pr->globals.stack = stack;
				}
				break;
			case OP_PUSHBI_Q:
				{
					pointer_t   stack = *pr->globals.stack - 4;
					pr_type_t  *stk = pr->pr_globals + stack;

					pointer = OPA(int) + st->b;
					ptr = pr->pr_globals + pointer;

					if (pr_boundscheck->int_val) {
						check_stack_pointer (pr, stack, 4);
						PR_BoundsCheck (pr, pointer, ev_quat);
					}

					QuatCopy (&ptr->quat_var, &stk->quat_var);
					*pr->globals.stack = stack;
				}
				break;

			case OP_POP_F:
			case OP_POP_FLD:
			case OP_POP_ENT:
			case OP_POP_S:
			case OP_POP_FN:
			case OP_POP_I:
			case OP_POP_P:
				{
					pointer_t   stack = *pr->globals.stack;
					pr_type_t  *stk = pr->pr_globals + stack;
					if (pr_boundscheck->int_val) {
						check_stack_pointer (pr, stack, 1);
					}
					OPA(int) = stk->integer_var;
					*pr->globals.stack = stack + 1;
				}
				break;
			case OP_POP_V:
				{
					pointer_t   stack = *pr->globals.stack;
					pr_type_t  *stk = pr->pr_globals + stack;
					if (pr_boundscheck->int_val) {
						check_stack_pointer (pr, stack, 3);
					}
					memcpy (op_a, stk, 3 * sizeof (*op_c));
					*pr->globals.stack = stack + 3;
				}
				break;
			case OP_POP_Q:
				{
					pointer_t   stack = *pr->globals.stack;
					pr_type_t  *stk = pr->pr_globals + stack;
					if (pr_boundscheck->int_val) {
						check_stack_pointer (pr, stack, 4);
					}
					memcpy (op_a, stk, 4 * sizeof (*op_c));
					*pr->globals.stack = stack + 4;
				}
				break;

			case OP_POPB_F:
			case OP_POPB_S:
			case OP_POPB_ENT:
			case OP_POPB_FLD:
			case OP_POPB_FN:
			case OP_POPB_I:
			case OP_POPB_P:
				{
					pointer_t   stack = *pr->globals.stack;
					pr_type_t  *stk = pr->pr_globals + stack;

					pointer = OPA(int) + OPB(int);
					ptr = pr->pr_globals + pointer;

					if (pr_boundscheck->int_val) {
						check_stack_pointer (pr, stack, 1);
						PR_BoundsCheck (pr, pointer, ev_integer);
					}

					ptr->integer_var = stk->integer_var;
					*pr->globals.stack = stack + 1;
				}
				break;
			case OP_POPB_V:
				{
					pointer_t   stack = *pr->globals.stack;
					pr_type_t  *stk = pr->pr_globals + stack;

					pointer = OPA(int) + OPB(int);
					ptr = pr->pr_globals + pointer;

					if (pr_boundscheck->int_val) {
						check_stack_pointer (pr, stack, 3);
						PR_BoundsCheck (pr, pointer, ev_integer);
					}

					VectorCopy (&stk->vector_var, &ptr->vector_var);
					*pr->globals.stack = stack + 3;
				}
				break;
			case OP_POPB_Q:
				{
					pointer_t   stack = *pr->globals.stack;
					pr_type_t  *stk = pr->pr_globals + stack;

					pointer = OPA(int) + OPB(int);
					ptr = pr->pr_globals + pointer;

					if (pr_boundscheck->int_val) {
						check_stack_pointer (pr, stack, 4);
						PR_BoundsCheck (pr, pointer, ev_quat);
					}

					QuatCopy (&stk->quat_var, &ptr->quat_var);
					*pr->globals.stack = stack + 4;
				}
				break;

			case OP_POPBI_F:
			case OP_POPBI_S:
			case OP_POPBI_ENT:
			case OP_POPBI_FLD:
			case OP_POPBI_FN:
			case OP_POPBI_I:
			case OP_POPBI_P:
				{
					pointer_t   stack = *pr->globals.stack;
					pr_type_t  *stk = pr->pr_globals + stack;

					pointer = OPA(int) + st->b;
					ptr = pr->pr_globals + pointer;

					if (pr_boundscheck->int_val) {
						check_stack_pointer (pr, stack, 1);
						PR_BoundsCheck (pr, pointer, ev_integer);
					}

					ptr->integer_var = stk->integer_var;
					*pr->globals.stack = stack + 1;
				}
				break;
			case OP_POPBI_V:
				{
					pointer_t   stack = *pr->globals.stack;
					pr_type_t  *stk = pr->pr_globals + stack;

					pointer = OPA(int) + st->b;
					ptr = pr->pr_globals + pointer;

					if (pr_boundscheck->int_val) {
						check_stack_pointer (pr, stack, 3);
						PR_BoundsCheck (pr, pointer, ev_integer);
					}

					VectorCopy (&stk->vector_var, &ptr->vector_var);
					*pr->globals.stack = stack + 3;
				}
				break;
			case OP_POPBI_Q:
				{
					pointer_t   stack = *pr->globals.stack;
					pr_type_t  *stk = pr->pr_globals + stack;

					pointer = OPA(int) + st->b;
					ptr = pr->pr_globals + pointer;

					if (pr_boundscheck->int_val) {
						check_stack_pointer (pr, stack, 4);
						PR_BoundsCheck (pr, pointer, ev_quat);
					}

					QuatCopy (&stk->quat_var, &ptr->quat_var);
					*pr->globals.stack = stack + 4;
				}
				break;

			// ==================
			case OP_IFNOT:
				if (!OPA(int)) {
					pr->pr_xstatement += (short)st->b - 1;	// offset the st++
					st = pr->pr_statements + pr->pr_xstatement;
				}
				break;
			case OP_IF:
				if (OPA(int)) {
					pr->pr_xstatement += (short)st->b - 1;	// offset the st++
					st = pr->pr_statements + pr->pr_xstatement;
				}
				break;
			case OP_IFBE:
				if (OPA(int) <= 0) {
					pr->pr_xstatement += (short)st->b - 1;	// offset the st++
					st = pr->pr_statements + pr->pr_xstatement;
				}
				break;
			case OP_IFB:
				if (OPA(int) < 0) {
					pr->pr_xstatement += (short)st->b - 1;	// offset the st++
					st = pr->pr_statements + pr->pr_xstatement;
				}
				break;
			case OP_IFAE:
				if (OPA(int) >= 0) {
					pr->pr_xstatement += (short)st->b - 1;	// offset the st++
					st = pr->pr_statements + pr->pr_xstatement;
				}
				break;
			case OP_IFA:
				if (OPA(int) > 0) {
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
					&& (OPA(uint) >= pr->progs->numstatements)) {
					PR_RunError (pr, "Invalid jump destination");
				}
				pr->pr_xstatement = OPA(uint) - 1;	// offset the st++
				st = pr->pr_statements + pr->pr_xstatement;
				break;
			case OP_JUMPB:
				pointer = st->a + OPB(int);
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
				pr->pr_params[1] = op_c;
				goto op_rcall;
			case OP_RCALL1:
				pr->pr_params[1] = pr->pr_real_params[1];
op_rcall:
				pr->pr_params[0] = op_b;
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
				PR_CallFunction (pr, OPA(uint));
				st = pr->pr_statements + pr->pr_xstatement;
				break;
			case OP_DONE:
			case OP_RETURN:
				if (!st->a)
					memset (&R_INT (pr), 0,
							pr->pr_param_size * sizeof (*op_a));
				else if (&R_INT (pr) != &OPA(int))
					memcpy (&R_INT (pr), op_a,
							pr->pr_param_size * sizeof (*op_a));
				// fallthrough
			case OP_RETURN_V:
				pr->pr_xfunction->profile += profile - startprofile;
				startprofile = profile;
				PR_LeaveFunction (pr, pr->pr_depth == exitdepth);
				st = pr->pr_statements + pr->pr_xstatement;
				if (pr->pr_depth == exitdepth) {
					if (pr->pr_trace && pr->pr_depth <= pr->pr_trace_depth)
						pr->pr_trace = false;
					// all done
					goto exit_program;
				}
				break;
			case OP_STATE:
				{
					int         self = *pr->globals.self;
					int         nextthink = pr->fields.nextthink + self;
					int         frame = pr->fields.frame + self;
					int         think = pr->fields.think + self;
					float       time = *pr->globals.time + 0.1;
					pr->pr_edict_area[nextthink].float_var = time;
					pr->pr_edict_area[frame].float_var = OPA(float);
					pr->pr_edict_area[think].func_var = OPB(uint);
				}
				break;
			case OP_STATE_F:
				{
					int         self = *pr->globals.self;
					int         nextthink = pr->fields.nextthink + self;
					int         frame = pr->fields.frame + self;
					int         think = pr->fields.think + self;
					float       time = *pr->globals.time + OPC(float);
					pr->pr_edict_area[nextthink].float_var = time;
					pr->pr_edict_area[frame].float_var = OPA(float);
					pr->pr_edict_area[think].func_var = OPB(uint);
				}
				break;
			case OP_ADD_I:
				OPC(int) = OPA(int) + OPB(int);
				break;
			case OP_SUB_I:
				OPC(int) = OPA(int) - OPB(int);
				break;
			case OP_MUL_I:
				OPC(int) = OPA(int) * OPB(int);
				break;
			case OP_DIV_I:
				OPC(int) = OPA(int) / OPB(int);
				break;
			case OP_MOD_I:
				{
					// implement true modulo for integers:
					//  5 mod  3 = 2
					// -5 mod  3 = 1
					//  5 mod -3 = -1
					// -5 mod -3 = -2
					int         a = OPA(int);
					int         b = OPB(int);
					int         c = a % b;
					// % is really remainder and so has the same sign rules
					// as division: -5 % 3 = -2, so need to add b (3 here)
					// if c's sign is incorrect, but only if c is non-zero
					int         mask = (a ^ b) >> 31;
					mask &= ~(!!c + 0) + 1;	// +0 to convert bool to int (gcc)
					OPC(int) = c + (mask & b);
				}
				break;
			case OP_REM_I:
				OPC(int) = OPA(int) % OPB(int);
				break;
			case OP_MOD_D:
				{
					double      a = OPA(double);
					double      b = OPB(double);
					// floating point modulo is so much easier :P
					OPC(double) = a - b * floor (a / b);
				}
				break;
			case OP_REM_D:
				{
					double      a = OPA(double);
					double      b = OPB(double);
					OPC(double) = a - b * trunc (a / b);
				}
				break;
			case OP_MOD_F:
				{
					float       a = OPA(float);
					float       b = OPB(float);
					OPC(float) = a - b * floorf (a / b);
				}
				break;
			case OP_REM_F:
				{
					float       a = OPA(float);
					float       b = OPB(float);
					OPC(float) = a - b * truncf (a / b);
				}
				break;
			case OP_CONV_IF:
				OPC(float) = OPA(int);
				break;
			case OP_CONV_FI:
				OPC(int) = OPA(float);
				break;
			case OP_BITAND_I:
				OPC(int) = OPA(int) & OPB(int);
				break;
			case OP_BITOR_I:
				OPC(int) = OPA(int) | OPB(int);
				break;
			case OP_BITXOR_I:
				OPC(int) = OPA(int) ^ OPB(int);
				break;
			case OP_BITNOT_I:
				OPC(int) = ~OPA(int);
				break;

			case OP_GE_I:
			case OP_GE_P:
				OPC(int) = OPA(int) >= OPB(int);
				break;
			case OP_GE_U:
				OPC(int) = OPA(uint) >= OPB(uint);
				break;
			case OP_LE_I:
			case OP_LE_P:
				OPC(int) = OPA(int) <= OPB(int);
				break;
			case OP_LE_U:
				OPC(int) = OPA(uint) <= OPB(uint);
				break;
			case OP_GT_I:
			case OP_GT_P:
				OPC(int) = OPA(int) > OPB(int);
				break;
			case OP_GT_U:
				OPC(int) = OPA(uint) > OPB(uint);
				break;
			case OP_LT_I:
			case OP_LT_P:
				OPC(int) = OPA(int) < OPB(int);
				break;
			case OP_LT_U:
				OPC(int) = OPA(uint) < OPB(uint);
				break;

			case OP_AND_I:
				OPC(int) = OPA(int) && OPB(int);
				break;
			case OP_OR_I:
				OPC(int) = OPA(int) || OPB(int);
				break;
			case OP_NOT_I:
			case OP_NOT_P:
				OPC(int) = !OPA(int);
				break;
			case OP_EQ_I:
			case OP_EQ_P:
				OPC(int) = OPA(int) == OPB(int);
				break;
			case OP_NE_I:
			case OP_NE_P:
				OPC(int) = OPA(int) != OPB(int);
				break;

			case OP_MOVEI:
				memmove (op_c, op_a, st->b * 4);
				break;
			case OP_MOVEP:
				if (pr_boundscheck->int_val) {
					PR_BoundsCheckSize (pr, OPC(int), OPB(uint));
					PR_BoundsCheckSize (pr, OPA(int), OPB(uint));
				}
				memmove (pr->pr_globals + OPC(int),
						 pr->pr_globals + OPA(int),
						 OPB(uint) * 4);
				break;
			case OP_MOVEPI:
				if (pr_boundscheck->int_val) {
					PR_BoundsCheckSize (pr, OPC(int), st->b);
					PR_BoundsCheckSize (pr, OPA(int), st->b);
				}
				memmove (pr->pr_globals + OPC(int),
						 pr->pr_globals + OPA(int),
						 st->b * 4);
				break;
			case OP_MEMSETI:
				pr_memset (op_c, OPA(int), st->b);
				break;
			case OP_MEMSETP:
				if (pr_boundscheck->int_val) {
					PR_BoundsCheckSize (pr, OPC(pointer), OPB(int));
				}
				pr_memset (pr->pr_globals + OPC(pointer), OPA(int),
						   OPB(int));
				break;
			case OP_MEMSETPI:
				if (pr_boundscheck->int_val) {
					PR_BoundsCheckSize (pr, OPC(pointer), st->b);
				}
				pr_memset (pr->pr_globals + OPC(pointer), OPA(int),
						   st->b);
				break;
			case OP_GE_D:
				OPC(float) = OPA(double) >= OPB(double);
				break;
			case OP_LE_D:
				OPC(float) = OPA(double) <= OPB(double);
				break;
			case OP_GT_D:
				OPC(float) = OPA(double) > OPB(double);
				break;
			case OP_LT_D:
				OPC(float) = OPA(double) < OPB(double);
				break;
			case OP_NOT_D:
				OPC(int) = (op_a[0].integer_var
								   || (op_a[1].integer_var & ~0x80000000u));
				break;
			case OP_EQ_D:
				OPC(int) = OPA(double) == OPB(double);
				break;
			case OP_NE_D:
				OPC(int) = OPA(double) != OPB(double);
				break;
			case OP_CONV_ID:
				OPC(double) = OPA(int);
				break;
			case OP_CONV_DI:
				OPC(int) = OPA(double);
				break;
			case OP_CONV_FD:
				OPC(double) = OPA(float);
				break;
			case OP_CONV_DF:
				OPC(float) = OPA(double);
				break;

// LordHavoc: to be enabled when Progs version 7 (or whatever it will be numbered) is finalized
/*
			case OP_BOUNDCHECK:
				if (OPA(int) < 0 || OPA(int) >= st->b) {
					PR_RunError (pr, "Progs boundcheck failed at line number "
					"%d, value is < 0 or >= %d", st->b, st->c);
				}
				break;

*/
			default:
				PR_RunError (pr, "Bad opcode %i", st->op);
		}
		if (pr->watch && pr->watch->integer_var != old_val.integer_var) {
			if (!pr->wp_conditional
				|| pr->watch->integer_var == pr->wp_val.integer_var) {
				if (pr->debug_handler) {
					pr->debug_handler (prd_watchpoint, 0, pr->debug_data);
				} else {
					PR_RunError (pr, "watchpoint hit: %d -> %d",
								 old_val.integer_var, pr->watch->integer_var);
				}
			}
			old_val.integer_var = pr->watch->integer_var;
		}
	}
exit_program:
}

/*
	PR_ExecuteProgram

	The interpretation main loop
*/
VISIBLE void
PR_ExecuteProgram (progs_t *pr, func_t fnum)
{
	Sys_PushSignalHook (signal_hook, pr);
	Sys_PushErrorHandler (error_handler, pr);

	if (pr->debug_handler) {
		pr->debug_handler (prd_subenter, &fnum, pr->debug_data);
	}

	int         exitdepth = pr->pr_depth;
	if (!PR_CallFunction (pr, fnum)) {
		// called a builtin instead of progs code
		goto exit_program;
	}
	if (1) {
		pr_exec_quakec (pr, exitdepth);
	}
exit_program:
	if (pr->debug_handler) {
		pr->debug_handler (prd_subexit, 0, pr->debug_data);
	}
	pr->pr_argc = 0;
	Sys_PopErrorHandler ();
	Sys_PopSignalHook ();
}
