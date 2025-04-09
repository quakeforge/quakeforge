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

#include "QF/simd/vec2d.h"
#include "QF/simd/vec2f.h"
#include "QF/simd/vec2i.h"
#include "QF/simd/vec4d.h"
#include "QF/simd/vec4f.h"
#include "QF/simd/vec4i.h"
#include "compat.h"

const char *prdebug_names[] = {
	[prd_none] = "none",
	[prd_trace] = "trace",
	[prd_breakpoint] = "breakpoint",
	[prd_watchpoint] = "watchpoint",
	[prd_subenter] = "subenter",
	[prd_subexit] = "subexit",
	[prd_begin] = "begin",
	[prd_terminate] = "terminate",
	[prd_runerror] = "runerror",
	[prd_error] = "error",
};

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

	memcpy (params->param_ptrs, pr->pr_params, sizeof (pr->pr_params));
	params->return_ptr = pr->pr_return;

	pr->pr_params[0] = pr->pr_real_params[0];
	pr->pr_params[1] = pr->pr_real_params[1];
	pr->pr_return = pr->pr_return_buffer;

	for (i = 0; i < pr->pr_argc; i++) {
		memcpy (params->params + i * pr->pr_param_size,
				pr->pr_real_params[i], size);
	}
	params->argc = pr->pr_argc;

	return params;
}

VISIBLE void
PR_RestoreParams (progs_t *pr, pr_stashed_params_t *params)
{
	int         i;
	int         size = pr->pr_param_size * sizeof (pr_type_t);

	memcpy (pr->pr_params, params->param_ptrs, sizeof (pr->pr_params));
	pr->pr_return = params->return_ptr;

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

	if (pr->pr_depth == PR_MAX_STACK_DEPTH)
		PR_RunError (pr, "stack overflow");

	frame = pr->pr_stack + pr->pr_depth++;

	frame->staddr = pr->pr_xstatement;
	if (pr->globals.stack) {
		frame->stack_ptr = *pr->globals.stack;
	}
	frame->bases  = pr->pr_bases;
	frame->func   = pr->pr_xfunction;
	frame->tstr   = pr->pr_xtstr;
	frame->return_ptr = pr->pr_return;

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

	pr->pr_return     = frame->return_ptr;
	pr->pr_xfunction  = frame->func;
	pr->pr_xstatement = frame->staddr;
	pr->pr_xtstr      = frame->tstr;
	pr->pr_bases      = frame->bases;
	// restore data stack (discard any locals)
	if (pr->globals.stack) {
		*pr->globals.stack = frame->stack_ptr;
	}
}

static __attribute__((pure)) long
align_offset (long offset, dparmsize_t paramsize)
{
	int         mask = (1 << paramsize.alignment) - 1;
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
	pr_type_t  *dstParams[PR_MAX_PARAMS];
	pr_ptr_t    paramofs = 0;

	if (pr->pr_trace && !pr->debug_handler) {
		Sys_Printf ("Entering function %s\n",
					PR_GetString (pr, f->descriptor->name));
	}

	PR_PushFrame (pr);

	//Sys_Printf("%s:\n", PR_GetString(pr,f->name));
	pr->pr_xfunction = f;
	pr->pr_xstatement = f->first_statement - 1;      		// offset the st++

	if (pr->progs->version == PROG_VERSION) {
		return;
	}

	if (f->numparams > 0) {
		paramofs = f->params_start;
		for (i = 0; i < f->numparams; i++) {
			paramofs = align_offset (paramofs, f->param_size[i]);
			dstParams[i] = pr->pr_globals + paramofs;
			paramofs += f->param_size[i].size;
			if (pr->pr_params[i] != pr->pr_real_params[i]) {
				copy_param (pr->pr_real_params[i], pr->pr_params[i],
							f->param_size[i].size);
				pr->pr_params[i] = pr->pr_real_params[i];
			}
		}
	} else if (f->numparams < 0) {
		paramofs = f->params_start + 2;	// argc and argv
		for (i = 0; i < -f->numparams - 1; i++) {
			paramofs = align_offset (paramofs, f->param_size[i]);
			dstParams[i] = pr->pr_globals + paramofs;
			paramofs += f->param_size[i].size;
			if (pr->pr_params[i] != pr->pr_real_params[i]) {
				copy_param (pr->pr_real_params[i], pr->pr_params[i],
							f->param_size[i].size);
				pr->pr_params[i] = pr->pr_real_params[i];
			}
		}
		dparmsize_t paramsize = { pr->pr_param_size, pr->pr_param_alignment };
		paramofs = align_offset (paramofs, paramsize );
		if (i < PR_MAX_PARAMS) {
			dstParams[i] = pr->pr_globals + paramofs;
		}
		for (; i < pr->pr_argc; i++) {
			if (pr->pr_params[i] != pr->pr_real_params[i]) {
				copy_param (pr->pr_real_params[i], pr->pr_params[i],
							paramsize.size);
				pr->pr_params[i] = pr->pr_real_params[i];
			}
		}
	}

	// save off any locals that the new function steps on
	if (pr->localstack_used + f->locals > PR_LOCAL_STACK_SIZE)
		PR_RunError (pr, "PR_EnterFunction: locals stack overflow");

	memcpy (&pr->localstack[pr->localstack_used],
			&pr->pr_globals[f->params_start],
			sizeof (pr_type_t) * f->locals);
	pr->localstack_used += f->locals;

	if (pr_deadbeef_locals) {
		for (pr_uint_t i = f->params_start;
			 i < f->params_start + f->locals; i++) {
			pr->pr_globals[i].value = 0xdeadbeef;
		}
	}

	// copy parameters
	if (f->numparams >= 0) {
		for (i = 0; i < f->numparams; i++) {
			copy_param (dstParams[i], pr->pr_params[i], f->param_size[i].size);
		}
	} else {
		int         copy_args;
		pr_type_t  *argc = &pr->pr_globals[f->params_start + 0];
		pr_type_t  *argv = &pr->pr_globals[f->params_start + 1];
		for (i = 0; i < -f->numparams - 1; i++) {
			copy_param (dstParams[i], pr->pr_params[i], f->param_size[i].size);
		}
		copy_args = pr->pr_argc - i;
		PR_PTR (int, argc) = copy_args;
		PR_PTR (ptr, argv) = PR_SetPointer (pr, dstParams[i]);
		if (i < PR_MAX_PARAMS) {
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

	if (pr->progs->version == PROG_VERSION) {
		return;
	}

	// restore locals from the stack
	pr->localstack_used -= f->locals;
	if (pr->localstack_used < 0)
		PR_RunError (pr, "PR_LeaveFunction: locals stack underflow");

	memcpy (&pr->pr_globals[f->params_start],
			&pr->localstack[pr->localstack_used],
			sizeof (pr_type_t) * f->locals);
}

VISIBLE void
PR_BoundsCheckSize (progs_t *pr, pr_ptr_t addr, unsigned size)
{
	if (addr < pr->null_size)
		PR_RunError (pr, "null pointer access");
	if (addr >= pr->globals_size
		|| size > (unsigned) (pr->globals_size - addr))
		PR_RunError (pr, "invalid memory access: %d (0 to %d-%d)", addr,
					 pr->globals_size, size);
	if (pr_boundscheck >= 2
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

	if (sig == SIGFPE && pr_faultchecks) {
		dstatement_t *st;
		pr_type_t  *op_a, *op_b, *op_c;

		st = pr->pr_statements + pr->pr_xstatement;
		op_a = pr->pr_globals + st->a;
		op_b = pr->pr_globals + st->b;
		op_c = pr->pr_globals + st->c;

		switch (st->op) {
			case OP_DIV_F_v6p:
				if ((OPA(int) & 0x80000000)
					^ (OPB(int) & 0x80000000))
					OPC(int) = 0xff7fffff;
				else
					OPC(int) = 0x7f7fffff;
				return 1;
			case OP_DIV_I_v6p:
				if (OPA(int) & 0x80000000)
					OPC(int) = -0x80000000;
				else
					OPC(int) = 0x7fffffff;
				return 1;
			case OP_MOD_I_v6p:
			case OP_MOD_F_v6p:
			case OP_REM_I_v6p:
			case OP_REM_F_v6p:
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
PR_CallFunction (progs_t *pr, pr_func_t fnum, pr_type_t *return_ptr)
{
	bfunction_t *f;

	if (!fnum)
		PR_RunError (pr, "NULL function");
	if (!return_ptr || return_ptr == pr->pr_globals) {
		return_ptr = pr->pr_return_buffer;
	}
	f = pr->function_table + fnum;
	if (f->first_statement < 0) {
		// negative statements are built in functions
		if (pr->progs->version == PROG_VERSION) {
			PR_SetupParams (pr, 0, 0);
		}
		if (pr->pr_trace && !pr->debug_handler) {
			Sys_Printf ("Calling builtin %s @ %p\n",
						PR_GetString (pr, f->descriptor->name), f->func);
		}
		pr_type_t  *saved_return = pr->pr_return;
		int         builtin_depth = pr->pr_depth;
		pr->pr_return = return_ptr;
		f->func (pr, f->data);
		f->profile++;	// to show number times the builtin is called
		if (builtin_depth == pr->pr_depth) {
			pr->pr_return = saved_return;
		} else if (builtin_depth < pr->pr_depth) {
			pr->pr_stack[builtin_depth].return_ptr = saved_return;
		}
		return 0;
	} else {
		PR_EnterFunction (pr, f);
		pr->pr_return = return_ptr;
		return 1;
	}
}

static void
check_stack_pointer (progs_t *pr, pr_ptr_t stack, int size)
{
	if (stack & 3) {
		PR_RunError (pr, "Progs stack not aligned");
	}
	if (stack < pr->stack_bottom) {
		PR_RunError (pr, "Progs stack overflow");
	}
	if (stack > pr->globals_size - size) {
		PR_RunError (pr, "Progs stack underflow");
	}
}

VISIBLE pr_type_t *
PR_SetupParams (progs_t *pr, int num_params, int min_alignment)
{
	if (pr->progs->version < PROG_VERSION) {
		if (num_params > PR_MAX_PARAMS) {
			PR_Error (pr, "attempt to settup more than %d params",
					  PR_MAX_PARAMS);
		}
		pr->pr_params[0] = pr->pr_real_params[0];
		pr->pr_params[1] = pr->pr_real_params[1];
		pr->pr_argc = num_params;
		return pr->pr_real_params[0];
	}
	int         offset = num_params * 4;
	if (min_alignment < 4) {
		min_alignment = 4;
	}
	pr_ptr_t    mask = ~(min_alignment - 1);
	pr_ptr_t    stack = (*pr->globals.stack - offset) & mask;
	if (pr_boundscheck) {
		check_stack_pointer (pr, stack, 0);
	}
	*pr->globals.stack = stack;
	pr->pr_params[0] = pr->pr_globals + stack;
	num_params = max (num_params, PR_MAX_PARAMS);
	for (int i = 1; i < num_params; i++) {
		pr->pr_params[i] = pr->pr_params[0] + i * 4;
	}
	return pr->pr_params[0];
}

static inline void
pr_memset (pr_type_t *dst, int val, pr_uint_t count)
{
	while (count-- > 0) {
		(*dst++).value = val;
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

		op_a = pr->pr_globals + (pr_ushort_t) st->a;
		op_b = pr->pr_globals + (pr_ushort_t) st->b;
		op_c = pr->pr_globals + (pr_ushort_t) st->c;

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

		pr_opcode_v6p_e op = st->op & ~OP_BREAK;
		switch (op) {
			case OP_ADD_D_v6p:
				OPC(double) = OPA(double) + OPB(double);
				break;
			case OP_ADD_F_v6p:
				OPC(float) = OPA(float) + OPB(float);
				break;
			case OP_ADD_V_v6p:
				VectorAdd (&OPA(float), &OPB(float), &OPC(float));
				break;
			case OP_ADD_Q_v6p:
				QuatAdd (&OPA(float), &OPB(float), &OPC(float));
				break;
			case OP_ADD_S_v6p:
				OPC(string) = PR_CatStrings (pr,
												PR_GetString (pr,
															  OPA(string)),
												PR_GetString (pr,
															  OPB(string)));
				break;
			case OP_SUB_D_v6p:
				OPC(double) = OPA(double) - OPB(double);
				break;
			case OP_SUB_F_v6p:
				OPC(float) = OPA(float) - OPB(float);
				break;
			case OP_SUB_V_v6p:
				VectorSubtract (&OPA(float), &OPB(float),
								&OPC(float));
				break;
			case OP_SUB_Q_v6p:
				QuatSubtract (&OPA(float), &OPB(float), &OPC(float));
				break;
			case OP_MUL_D_v6p:
				OPC(double) = OPA(double) * OPB(double);
				break;
			case OP_MUL_F_v6p:
				OPC(float) = OPA(float) * OPB(float);
				break;
			case OP_MUL_V_v6p:
				OPC(float) = DotProduct (&OPA(float), &OPB(float));
				break;
			case OP_MUL_DV_v6p:
				{
					// avoid issues with the likes of x = x.x * x;
					// makes for faster code, too
					double      scale = OPA(double);
					VectorScale (&OPB(float), scale, &OPC(float));
				}
				break;
			case OP_MUL_VD_v6p:
				{
					// avoid issues with the likes of x = x * x.x;
					// makes for faster code, too
					double      scale = OPB(double);
					VectorScale (&OPA(float), scale, &OPC(float));
				}
				break;
			case OP_MUL_FV_v6p:
				{
					// avoid issues with the likes of x = x.x * x;
					// makes for faster code, too
					float       scale = OPA(float);
					VectorScale (&OPB(float), scale, &OPC(float));
				}
				break;
			case OP_MUL_VF_v6p:
				{
					// avoid issues with the likes of x = x * x.x;
					// makes for faster code, too
					float       scale = OPB(float);
					VectorScale (&OPA(float), scale, &OPC(float));
				}
				break;
			case OP_MUL_Q_v6p:
				QuatMult (&OPA(float), &OPB(float), &OPC(float));
				break;
			case OP_MUL_QV_v6p:
				QuatMultVec (&OPA(float), &OPB(float), &OPC(float));
				break;
			case OP_MUL_DQ_v6p:
				{
					// avoid issues with the likes of x = x.s * x;
					// makes for faster code, too
					double      scale = OPA(double);
					QuatScale (&OPB(float), scale, &OPC(float));
				}
				break;
			case OP_MUL_QD_v6p:
				{
					// avoid issues with the likes of x = x * x.s;
					// makes for faster code, too
					double      scale = OPB(double);
					QuatScale (&OPA(float), scale, &OPC(float));
				}
				break;
			case OP_MUL_FQ_v6p:
				{
					// avoid issues with the likes of x = x.s * x;
					// makes for faster code, too
					float       scale = OPA(float);
					QuatScale (&OPB(float), scale, &OPC(float));
				}
				break;
			case OP_MUL_QF_v6p:
				{
					// avoid issues with the likes of x = x * x.s;
					// makes for faster code, too
					float       scale = OPB(float);
					QuatScale (&OPA(float), scale, &OPC(float));
				}
				break;
			case OP_CONJ_Q_v6p:
				QuatConj (&OPA(float), &OPC(float));
				break;
			case OP_DIV_D_v6p:
				OPC(double) = OPA(double) / OPB(double);
				break;
			case OP_DIV_F_v6p:
				OPC(float) = OPA(float) / OPB(float);
				break;
			case OP_BITAND_v6p:
				OPC(float) = (int) OPA(float) & (int) OPB(float);
				break;
			case OP_BITOR_v6p:
				OPC(float) = (int) OPA(float) | (int) OPB(float);
				break;
			case OP_BITXOR_F_v6p:
				OPC(float) = (int) OPA(float) ^ (int) OPB(float);
				break;
			case OP_BITNOT_F_v6p:
				OPC(float) = ~ (int) OPA(float);
				break;
			case OP_SHL_F_v6p:
				OPC(float) = (int) OPA(float) << (int) OPB(float);
				break;
			case OP_SHR_F_v6p:
				OPC(float) = (int) OPA(float) >> (int) OPB(float);
				break;
			case OP_SHL_I_v6p:
				OPC(int) = OPA(int) << OPB(int);
				break;
			case OP_SHR_I_v6p:
				OPC(int) = OPA(int) >> OPB(int);
				break;
			case OP_SHR_U_v6p:
				OPC(uint) = OPA(uint) >> OPB(int);
				break;
			case OP_GE_F_v6p:
				OPC(float) = OPA(float) >= OPB(float);
				break;
			case OP_LE_F_v6p:
				OPC(float) = OPA(float) <= OPB(float);
				break;
			case OP_GT_F_v6p:
				OPC(float) = OPA(float) > OPB(float);
				break;
			case OP_LT_F_v6p:
				OPC(float) = OPA(float) < OPB(float);
				break;
			case OP_AND_v6p:	// OPA and OPB have to be float for -0.0
				OPC(int) = FNZ (OPA(uint)) && FNZ (OPB(uint));
				break;
			case OP_OR_v6p:		// OPA and OPB have to be float for -0.0
				OPC(int) = FNZ (OPA(uint)) || FNZ (OPB(uint));
				break;
			case OP_NOT_F_v6p:
				OPC(int) = !FNZ (OPA(uint));
				break;
			case OP_NOT_V_v6p:
				OPC(int) = VectorIsZero (&OPA(float));
				break;
			case OP_NOT_Q_v6p:
				OPC(int) = QuatIsZero (&OPA(float));
				break;
			case OP_NOT_S_v6p:
				OPC(int) = !OPA(string) || !*PR_GetString (pr, OPA(string));
				break;
			case OP_NOT_FN_v6p:
				OPC(int) = !OPA(func);
				break;
			case OP_NOT_ENT_v6p:
				OPC(int) = !OPA(entity);
				break;
			case OP_EQ_F_v6p:
				OPC(int) = OPA(float) == OPB(float);
				break;
			case OP_EQ_V_v6p:
				OPC(int) = VectorCompare (&OPA(float), &OPB(float));
				break;
			case OP_EQ_Q_v6p:
				OPC(int) = QuatCompare (&OPA(float), &OPB(float));
				break;
			case OP_EQ_E_v6p:
				OPC(int) = OPA(field) == OPB(field);
				break;
			case OP_EQ_FN_v6p:
				OPC(int) = OPA(func) == OPB(func);
				break;
			case OP_NE_F_v6p:
				OPC(int) = OPA(float) != OPB(float);
				break;
			case OP_NE_V_v6p:
				OPC(int) = !VectorCompare (&OPA(float), &OPB(float));
				break;
			case OP_NE_Q_v6p:
				OPC(int) = !QuatCompare (&OPA(float), &OPB(float));
				break;
			case OP_LE_S_v6p:
			case OP_GE_S_v6p:
			case OP_LT_S_v6p:
			case OP_GT_S_v6p:
			case OP_NE_S_v6p:
			case OP_EQ_S_v6p:
				{
					int cmp = strcmp (PR_GetString (pr, OPA(string)),
									  PR_GetString (pr, OPB(string)));
					switch (st->op) {
						case OP_LE_S_v6p: cmp = (cmp <= 0); break;
						case OP_GE_S_v6p: cmp = (cmp >= 0); break;
						case OP_LT_S_v6p: cmp = (cmp < 0); break;
						case OP_GT_S_v6p: cmp = (cmp > 0); break;
						case OP_NE_S_v6p: break;
						case OP_EQ_S_v6p: cmp = !cmp; break;
						default:      break;
					}
					OPC(int) = cmp;
				}
				break;
			case OP_NE_E_v6p:
				OPC(int) = OPA(entity) != OPB(entity);
				break;
			case OP_NE_FN_v6p:
				OPC(int) = OPA(func) != OPB(func);
				break;

			// ==================
			case OP_STORE_F_v6p:
			case OP_STORE_ENT_v6p:
			case OP_STORE_FLD_v6p:			// integers
			case OP_STORE_S_v6p:
			case OP_STORE_FN_v6p:			// pointers
			case OP_STORE_I_v6p:
			case OP_STORE_P_v6p:
				OPB(int) = OPA(int);
				break;
			case OP_STORE_V_v6p:
				VectorCopy (&OPA(float), &OPB(float));
				break;
			case OP_STORE_Q_v6p:
				QuatCopy (&OPA(float), &OPB(float));
				break;
			case OP_STORE_D_v6p:
				OPB(double) = OPA(double);
				break;

			case OP_STOREP_F_v6p:
			case OP_STOREP_ENT_v6p:
			case OP_STOREP_FLD_v6p:		// integers
			case OP_STOREP_S_v6p:
			case OP_STOREP_FN_v6p:		// pointers
			case OP_STOREP_I_v6p:
			case OP_STOREP_P_v6p:
				pointer = OPB(ptr);
				if (pr_boundscheck) {
					PR_BoundsCheck (pr, pointer, ev_int);
				}
				ptr = pr->pr_globals + pointer;
				ptr->value = OPA(int);
				break;
			case OP_STOREP_V_v6p:
				pointer = OPB(ptr);
				if (pr_boundscheck) {
					PR_BoundsCheck (pr, pointer, ev_vector);
				}
				ptr = pr->pr_globals + pointer;
				VectorCopy (&OPA(float), &PR_PTR (float, ptr));
				break;
			case OP_STOREP_Q_v6p:
				pointer = OPB(ptr);
				if (pr_boundscheck) {
					PR_BoundsCheck (pr, pointer, ev_quaternion);
				}
				ptr = pr->pr_globals + pointer;
				QuatCopy (&OPA(float), &PR_PTR (float, ptr));
				break;
			case OP_STOREP_D_v6p:
				pointer = OPB(ptr);
				if (pr_boundscheck) {
					PR_BoundsCheck (pr, pointer, ev_double);
				}
				ptr = pr->pr_globals + pointer;
				*(double *) ptr = OPA(double);
				break;

			case OP_ADDRESS_v6p:
				if (pr_boundscheck) {
					if (OPA(entity) >= pr->pr_edict_area_size)
						PR_RunError (pr, "Progs attempted to address an out "
									 "of bounds edict");
					if (OPA(entity) == 0 && pr->null_bad)
						PR_RunError (pr, "assignment to world entity");
					if (OPB(field) >= pr->progs->entityfields)
						PR_RunError (pr, "Progs attempted to address an "
									 "invalid field in an edict");
				}
				fldofs = OPA(entity) + OPB(field);
				OPC(ptr) = &pr->pr_edict_area[fldofs] - pr->pr_globals;
				break;
			case OP_ADDRESS_VOID_v6p:
			case OP_ADDRESS_F_v6p:
			case OP_ADDRESS_V_v6p:
			case OP_ADDRESS_Q_v6p:
			case OP_ADDRESS_S_v6p:
			case OP_ADDRESS_ENT_v6p:
			case OP_ADDRESS_FLD_v6p:
			case OP_ADDRESS_FN_v6p:
			case OP_ADDRESS_I_v6p:
			case OP_ADDRESS_P_v6p:
			case OP_ADDRESS_D_v6p:
				OPC(int) = (pr_ushort_t) st->a;
				break;

			case OP_LOAD_F_v6p:
			case OP_LOAD_FLD_v6p:
			case OP_LOAD_ENT_v6p:
			case OP_LOAD_S_v6p:
			case OP_LOAD_FN_v6p:
			case OP_LOAD_I_v6p:
			case OP_LOAD_P_v6p:
				if (pr_boundscheck) {
					if (OPA(entity) >= pr->pr_edict_area_size)
						PR_RunError (pr, "Progs attempted to read an out of "
									 "bounds edict number");
					if (OPB(field) >= pr->progs->entityfields)
						PR_RunError (pr, "Progs attempted to read an invalid "
									 "field in an edict");
				}
				fldofs = OPA(entity) + OPB(field);
				OPC(int) = PR_PTR (int, &pr->pr_edict_area[fldofs]);
				break;
			case OP_LOAD_V_v6p:
				if (pr_boundscheck) {
					if (OPA(entity) >= pr->pr_edict_area_size)
						PR_RunError (pr, "Progs attempted to read an out of "
									 "bounds edict number");
					if (OPB(field) + 2 >= pr->progs->entityfields)
						PR_RunError (pr, "Progs attempted to read an invalid "
									 "field in an edict");
				}
				fldofs = OPA(entity) + OPB(field);
				memcpy (op_c, &pr->pr_edict_area[fldofs], 3 * sizeof (*op_c));
				break;
			case OP_LOAD_Q_v6p:
				if (pr_boundscheck) {
					if (OPA(entity) >= pr->pr_edict_area_size)
						PR_RunError (pr, "Progs attempted to read an out of "
									 "bounds edict number");
					if (OPB(field) + 3 >= pr->progs->entityfields)
						PR_RunError (pr, "Progs attempted to read an invalid "
									 "field in an edict");
				}
				fldofs = OPA(entity) + OPB(field);
				memcpy (op_c, &pr->pr_edict_area[fldofs], 4 * sizeof (*op_c));
				break;
			case OP_LOAD_D_v6p:
				if (pr_boundscheck) {
					if (OPA(entity) >= pr->pr_edict_area_size)
						PR_RunError (pr, "Progs attempted to read an out of "
									 "bounds edict number");
					if (OPB(field) + 1 >= pr->progs->entityfields)
						PR_RunError (pr, "Progs attempted to read an invalid "
									 "field in an edict");
				}
				fldofs = OPA(entity) + OPB(field);
				memcpy (op_c, &pr->pr_edict_area[fldofs], sizeof (double));
				break;

			case OP_LOADB_F_v6p:
			case OP_LOADB_S_v6p:
			case OP_LOADB_ENT_v6p:
			case OP_LOADB_FLD_v6p:
			case OP_LOADB_FN_v6p:
			case OP_LOADB_I_v6p:
			case OP_LOADB_P_v6p:
				pointer = OPA(entity) + OPB(field);
				if (pr_boundscheck) {
					PR_BoundsCheck (pr, pointer, ev_int);
				}
				ptr = pr->pr_globals + pointer;
				OPC(int) = ptr->value;
				break;
			case OP_LOADB_V_v6p:
				pointer = OPA(entity) + OPB(field);
				if (pr_boundscheck) {
					PR_BoundsCheck (pr, pointer, ev_vector);
				}
				VectorCopy (G_VECTOR (pr, pointer), &OPC(float));
				break;
			case OP_LOADB_Q_v6p:
				pointer = OPA(entity) + OPB(field);
				if (pr_boundscheck) {
					PR_BoundsCheck (pr, pointer, ev_quaternion);
				}
				QuatCopy (G_QUAT (pr, pointer), &OPC(float));
				break;
			case OP_LOADB_D_v6p:
				pointer = OPA(entity) + OPB(field);
				if (pr_boundscheck) {
					PR_BoundsCheck (pr, pointer, ev_double);
				}
				ptr = pr->pr_globals + pointer;
				OPC(double) = *(double *) ptr;
				break;

			case OP_LOADBI_F_v6p:
			case OP_LOADBI_S_v6p:
			case OP_LOADBI_ENT_v6p:
			case OP_LOADBI_FLD_v6p:
			case OP_LOADBI_FN_v6p:
			case OP_LOADBI_I_v6p:
			case OP_LOADBI_P_v6p:
				pointer = OPA(ptr) + st->b;
				if (pr_boundscheck) {
					PR_BoundsCheck (pr, pointer, ev_int);
				}
				ptr = pr->pr_globals + pointer;
				OPC(int) = ptr->value;
				break;
			case OP_LOADBI_V_v6p:
				pointer = OPA(ptr) + st->b;
				if (pr_boundscheck) {
					PR_BoundsCheck (pr, pointer, ev_vector);
				}
				ptr = pr->pr_globals + pointer;
				VectorCopy (G_VECTOR (pr, pointer), &OPC(float));
				break;
			case OP_LOADBI_Q_v6p:
				pointer = OPA(ptr) + st->b;
				if (pr_boundscheck) {
					PR_BoundsCheck (pr, pointer, ev_quaternion);
				}
				ptr = pr->pr_globals + pointer;
				QuatCopy (G_QUAT (pr, pointer), &OPC(float));
				break;
			case OP_LOADBI_D_v6p:
				pointer = OPA(ptr) + st->b;
				if (pr_boundscheck) {
					PR_BoundsCheck (pr, pointer, ev_quaternion);
				}
				ptr = pr->pr_globals + pointer;
				OPC(double) = *(double *) ptr;
				break;

			case OP_LEA_v6p:
				pointer = OPA(ptr) + OPB(int);
				OPC(ptr) = pointer;
				break;

			case OP_LEAI_v6p:
				pointer = OPA(ptr) + st->b;
				OPC(ptr) = pointer;
				break;

			case OP_STOREB_F_v6p:
			case OP_STOREB_S_v6p:
			case OP_STOREB_ENT_v6p:
			case OP_STOREB_FLD_v6p:
			case OP_STOREB_FN_v6p:
			case OP_STOREB_I_v6p:
			case OP_STOREB_P_v6p:
				pointer = OPB(ptr) + OPC(int);
				if (pr_boundscheck) {
					PR_BoundsCheck (pr, pointer, ev_int);
				}
				ptr = pr->pr_globals + pointer;
				ptr->value = OPA(int);
				break;
			case OP_STOREB_V_v6p:
				pointer = OPB(ptr) + OPC(int);
				if (pr_boundscheck) {
					PR_BoundsCheck (pr, pointer, ev_vector);
				}
				ptr = pr->pr_globals + pointer;
				VectorCopy (&OPA(float), G_VECTOR (pr, pointer));
				break;
			case OP_STOREB_Q_v6p:
				pointer = OPB(ptr) + OPC(int);
				if (pr_boundscheck) {
					PR_BoundsCheck (pr, pointer, ev_quaternion);
				}
				ptr = pr->pr_globals + pointer;
				QuatCopy (&OPA(float), G_QUAT (pr, pointer));
				break;
			case OP_STOREB_D_v6p:
				pointer = OPB(ptr) + OPC(int);
				if (pr_boundscheck) {
					PR_BoundsCheck (pr, pointer, ev_quaternion);
				}
				ptr = pr->pr_globals + pointer;
				*(double *) ptr = OPA(double);
				break;

			case OP_STOREBI_F_v6p:
			case OP_STOREBI_S_v6p:
			case OP_STOREBI_ENT_v6p:
			case OP_STOREBI_FLD_v6p:
			case OP_STOREBI_FN_v6p:
			case OP_STOREBI_I_v6p:
			case OP_STOREBI_P_v6p:
				pointer = OPB(ptr) + st->c;
				if (pr_boundscheck) {
					PR_BoundsCheck (pr, pointer, ev_int);
				}
				ptr = pr->pr_globals + pointer;
				ptr->value = OPA(int);
				break;
			case OP_STOREBI_V_v6p:
				pointer = OPB(ptr) + st->c;
				if (pr_boundscheck) {
					PR_BoundsCheck (pr, pointer, ev_vector);
				}
				ptr = pr->pr_globals + pointer;
				VectorCopy (&OPA(float), G_VECTOR (pr, pointer));
				break;
			case OP_STOREBI_Q_v6p:
				pointer = OPB(ptr) + st->c;
				if (pr_boundscheck) {
					PR_BoundsCheck (pr, pointer, ev_quaternion);
				}
				ptr = pr->pr_globals + pointer;
				QuatCopy (&OPA(float), G_QUAT (pr, pointer));
				break;
			case OP_STOREBI_D_v6p:
				pointer = OPB(ptr) + st->c;
				if (pr_boundscheck) {
					PR_BoundsCheck (pr, pointer, ev_quaternion);
				}
				ptr = pr->pr_globals + pointer;
				*(double *) ptr = OPA(double);
				break;

			case OP_PUSH_F_v6p:
			case OP_PUSH_FLD_v6p:
			case OP_PUSH_ENT_v6p:
			case OP_PUSH_S_v6p:
			case OP_PUSH_FN_v6p:
			case OP_PUSH_I_v6p:
			case OP_PUSH_P_v6p:
				{
					pr_ptr_t    stack = *pr->globals.stack - 1;
					pr_type_t  *stk = pr->pr_globals + stack;
					if (pr_boundscheck) {
						check_stack_pointer (pr, stack, 1);
					}
					stk->value = OPA(int);
					*pr->globals.stack = stack;
				}
				break;
			case OP_PUSH_V_v6p:
				{
					pr_ptr_t    stack = *pr->globals.stack - 3;
					pr_type_t  *stk = pr->pr_globals + stack;
					if (pr_boundscheck) {
						check_stack_pointer (pr, stack, 3);
					}
					memcpy (stk, op_a, 3 * sizeof (*op_c));
					*pr->globals.stack = stack;
				}
				break;
			case OP_PUSH_Q_v6p:
				{
					pr_ptr_t    stack = *pr->globals.stack - 4;
					pr_type_t  *stk = pr->pr_globals + stack;
					if (pr_boundscheck) {
						check_stack_pointer (pr, stack, 4);
					}
					memcpy (stk, op_a, 4 * sizeof (*op_c));
					*pr->globals.stack = stack;
				}
				break;

			case OP_PUSHB_F_v6p:
			case OP_PUSHB_S_v6p:
			case OP_PUSHB_ENT_v6p:
			case OP_PUSHB_FLD_v6p:
			case OP_PUSHB_FN_v6p:
			case OP_PUSHB_I_v6p:
			case OP_PUSHB_P_v6p:
				{
					pr_ptr_t    stack = *pr->globals.stack - 1;
					pr_type_t  *stk = pr->pr_globals + stack;

					pointer = OPA(ptr) + OPB(int);
					ptr = pr->pr_globals + pointer;

					if (pr_boundscheck) {
						check_stack_pointer (pr, stack, 1);
						PR_BoundsCheck (pr, pointer, ev_int);
					}

					stk->value = ptr->value;
					*pr->globals.stack = stack;
				}
				break;
			case OP_PUSHB_V_v6p:
				{
					pr_ptr_t    stack = *pr->globals.stack - 3;

					pointer = OPA(ptr) + OPB(int);
					ptr = pr->pr_globals + pointer;

					if (pr_boundscheck) {
						check_stack_pointer (pr, stack, 3);
						PR_BoundsCheck (pr, pointer, ev_int);
					}

					VectorCopy (G_VECTOR (pr, pointer), G_VECTOR (pr, stack));
					*pr->globals.stack = stack;
				}
				break;
			case OP_PUSHB_Q_v6p:
				{
					pr_ptr_t    stack = *pr->globals.stack - 4;

					pointer = OPA(ptr) + OPB(int);
					ptr = pr->pr_globals + pointer;

					if (pr_boundscheck) {
						check_stack_pointer (pr, stack, 4);
						PR_BoundsCheck (pr, pointer, ev_quaternion);
					}

					QuatCopy (G_QUAT (pr, pointer), G_QUAT (pr, stack));
					*pr->globals.stack = stack;
				}
				break;

			case OP_PUSHBI_F_v6p:
			case OP_PUSHBI_S_v6p:
			case OP_PUSHBI_ENT_v6p:
			case OP_PUSHBI_FLD_v6p:
			case OP_PUSHBI_FN_v6p:
			case OP_PUSHBI_I_v6p:
			case OP_PUSHBI_P_v6p:
				{
					pr_ptr_t    stack = *pr->globals.stack - 1;
					pr_type_t  *stk = pr->pr_globals + stack;

					pointer = OPA(ptr) + (pr_ushort_t) st->b;
					ptr = pr->pr_globals + pointer;

					if (pr_boundscheck) {
						check_stack_pointer (pr, stack, 1);
						PR_BoundsCheck (pr, pointer, ev_int);
					}

					stk->value = ptr->value;
					*pr->globals.stack = stack;
				}
				break;
			case OP_PUSHBI_V_v6p:
				{
					pr_ptr_t    stack = *pr->globals.stack - 3;

					pointer = OPA(ptr) + (pr_ushort_t) st->b;
					ptr = pr->pr_globals + pointer;

					if (pr_boundscheck) {
						check_stack_pointer (pr, stack, 3);
						PR_BoundsCheck (pr, pointer, ev_int);
					}

					VectorCopy (G_VECTOR (pr, pointer), G_VECTOR (pr, stack));
					*pr->globals.stack = stack;
				}
				break;
			case OP_PUSHBI_Q_v6p:
				{
					pr_ptr_t    stack = *pr->globals.stack - 4;

					pointer = OPA(ptr) + (pr_ushort_t) st->b;
					ptr = pr->pr_globals + pointer;

					if (pr_boundscheck) {
						check_stack_pointer (pr, stack, 4);
						PR_BoundsCheck (pr, pointer, ev_quaternion);
					}

					QuatCopy (G_QUAT (pr, pointer), G_QUAT (pr, stack));
					*pr->globals.stack = stack;
				}
				break;

			case OP_POP_F_v6p:
			case OP_POP_FLD_v6p:
			case OP_POP_ENT_v6p:
			case OP_POP_S_v6p:
			case OP_POP_FN_v6p:
			case OP_POP_I_v6p:
			case OP_POP_P_v6p:
				{
					pr_ptr_t    stack = *pr->globals.stack;
					pr_type_t  *stk = pr->pr_globals + stack;
					if (pr_boundscheck) {
						check_stack_pointer (pr, stack, 1);
					}
					OPA(int) = stk->value;
					*pr->globals.stack = stack + 1;
				}
				break;
			case OP_POP_V_v6p:
				{
					pr_ptr_t    stack = *pr->globals.stack;
					pr_type_t  *stk = pr->pr_globals + stack;
					if (pr_boundscheck) {
						check_stack_pointer (pr, stack, 3);
					}
					memcpy (op_a, stk, 3 * sizeof (*op_c));
					*pr->globals.stack = stack + 3;
				}
				break;
			case OP_POP_Q_v6p:
				{
					pr_ptr_t    stack = *pr->globals.stack;
					pr_type_t  *stk = pr->pr_globals + stack;
					if (pr_boundscheck) {
						check_stack_pointer (pr, stack, 4);
					}
					memcpy (op_a, stk, 4 * sizeof (*op_c));
					*pr->globals.stack = stack + 4;
				}
				break;

			case OP_POPB_F_v6p:
			case OP_POPB_S_v6p:
			case OP_POPB_ENT_v6p:
			case OP_POPB_FLD_v6p:
			case OP_POPB_FN_v6p:
			case OP_POPB_I_v6p:
			case OP_POPB_P_v6p:
				{
					pr_ptr_t    stack = *pr->globals.stack;
					pr_type_t  *stk = pr->pr_globals + stack;

					pointer = OPA(ptr) + OPB(int);
					ptr = pr->pr_globals + pointer;

					if (pr_boundscheck) {
						check_stack_pointer (pr, stack, 1);
						PR_BoundsCheck (pr, pointer, ev_int);
					}

					ptr->value = stk->value;
					*pr->globals.stack = stack + 1;
				}
				break;
			case OP_POPB_V_v6p:
				{
					pr_ptr_t    stack = *pr->globals.stack;

					pointer = OPA(ptr) + OPB(int);
					ptr = pr->pr_globals + pointer;

					if (pr_boundscheck) {
						check_stack_pointer (pr, stack, 3);
						PR_BoundsCheck (pr, pointer, ev_int);
					}

					VectorCopy (G_VECTOR (pr, stack), G_VECTOR (pr, pointer));
					*pr->globals.stack = stack + 3;
				}
				break;
			case OP_POPB_Q_v6p:
				{
					pr_ptr_t    stack = *pr->globals.stack;

					pointer = OPA(ptr) + OPB(int);
					ptr = pr->pr_globals + pointer;

					if (pr_boundscheck) {
						check_stack_pointer (pr, stack, 4);
						PR_BoundsCheck (pr, pointer, ev_quaternion);
					}

					QuatCopy (G_QUAT (pr, stack), G_QUAT (pr, pointer));
					*pr->globals.stack = stack + 4;
				}
				break;

			case OP_POPBI_F_v6p:
			case OP_POPBI_S_v6p:
			case OP_POPBI_ENT_v6p:
			case OP_POPBI_FLD_v6p:
			case OP_POPBI_FN_v6p:
			case OP_POPBI_I_v6p:
			case OP_POPBI_P_v6p:
				{
					pr_ptr_t    stack = *pr->globals.stack;
					pr_type_t  *stk = pr->pr_globals + stack;

					pointer = OPA(ptr) + (pr_ushort_t) st->b;
					ptr = pr->pr_globals + pointer;

					if (pr_boundscheck) {
						check_stack_pointer (pr, stack, 1);
						PR_BoundsCheck (pr, pointer, ev_int);
					}

					ptr->value = stk->value;
					*pr->globals.stack = stack + 1;
				}
				break;
			case OP_POPBI_V_v6p:
				{
					pr_ptr_t    stack = *pr->globals.stack;

					pointer = OPA(ptr) + (pr_ushort_t) st->b;
					ptr = pr->pr_globals + pointer;

					if (pr_boundscheck) {
						check_stack_pointer (pr, stack, 3);
						PR_BoundsCheck (pr, pointer, ev_int);
					}

					VectorCopy (G_VECTOR (pr, stack), G_VECTOR (pr, pointer));
					*pr->globals.stack = stack + 3;
				}
				break;
			case OP_POPBI_Q_v6p:
				{
					pr_ptr_t    stack = *pr->globals.stack;

					pointer = OPA(ptr) + (pr_ushort_t) st->b;
					ptr = pr->pr_globals + pointer;

					if (pr_boundscheck) {
						check_stack_pointer (pr, stack, 4);
						PR_BoundsCheck (pr, pointer, ev_quaternion);
					}

					QuatCopy (G_QUAT (pr, stack), G_QUAT (pr, pointer));
					*pr->globals.stack = stack + 4;
				}
				break;

			// ==================
			case OP_IFNOT_v6p:
				if (!OPA(int)) {
					pr->pr_xstatement += st->b - 1;	// offset the st++
					st = pr->pr_statements + pr->pr_xstatement;
				}
				break;
			case OP_IF_v6p:
				if (OPA(int)) {
					pr->pr_xstatement += st->b - 1;	// offset the st++
					st = pr->pr_statements + pr->pr_xstatement;
				}
				break;
			case OP_IFBE_v6p:
				if (OPA(int) <= 0) {
					pr->pr_xstatement += st->b - 1;	// offset the st++
					st = pr->pr_statements + pr->pr_xstatement;
				}
				break;
			case OP_IFB_v6p:
				if (OPA(int) < 0) {
					pr->pr_xstatement += st->b - 1;	// offset the st++
					st = pr->pr_statements + pr->pr_xstatement;
				}
				break;
			case OP_IFAE_v6p:
				if (OPA(int) >= 0) {
					pr->pr_xstatement += st->b - 1;	// offset the st++
					st = pr->pr_statements + pr->pr_xstatement;
				}
				break;
			case OP_IFA_v6p:
				if (OPA(int) > 0) {
					pr->pr_xstatement += st->b - 1;	// offset the st++
					st = pr->pr_statements + pr->pr_xstatement;
				}
				break;
			case OP_GOTO_v6p:
				pr->pr_xstatement += st->a - 1;		// offset the st++
				st = pr->pr_statements + pr->pr_xstatement;
				break;
			case OP_JUMP_v6p:
				if (pr_boundscheck
					&& (OPA(uint) >= pr->progs->statements.count)) {
					PR_RunError (pr, "Invalid jump destination");
				}
				pr->pr_xstatement = OPA(uint) - 1;	// offset the st++
				st = pr->pr_statements + pr->pr_xstatement;
				break;
			case OP_JUMPB_v6p:
				pointer = (pr_ushort_t) st->a + OPB(int);
				if (pr_boundscheck) {
					PR_BoundsCheck (pr, pointer, ev_int);
				}
				pointer = G_POINTER (pr, pointer);
				if (pr_boundscheck
					&& (pointer >= pr->progs->statements.count)) {
					PR_RunError (pr, "Invalid jump destination");
				}
				pr->pr_xstatement = pointer - 1;			// offset the st++
				st = pr->pr_statements + pr->pr_xstatement;
				break;

			case OP_RCALL2_v6p:
			case OP_RCALL3_v6p:
			case OP_RCALL4_v6p:
			case OP_RCALL5_v6p:
			case OP_RCALL6_v6p:
			case OP_RCALL7_v6p:
			case OP_RCALL8_v6p:
				pr->pr_params[1] = op_c;
				goto op_rcall;
			case OP_RCALL1_v6p:
				pr->pr_params[1] = pr->pr_real_params[1];
op_rcall:
				pr->pr_params[0] = op_b;
				pr->pr_argc = st->op - OP_RCALL1_v6p + 1;
				goto op_call;
			case OP_CALL0_v6p:
			case OP_CALL1_v6p:
			case OP_CALL2_v6p:
			case OP_CALL3_v6p:
			case OP_CALL4_v6p:
			case OP_CALL5_v6p:
			case OP_CALL6_v6p:
			case OP_CALL7_v6p:
			case OP_CALL8_v6p:
				pr->pr_params[0] = pr->pr_real_params[0];
				pr->pr_params[1] = pr->pr_real_params[1];
				pr->pr_argc = st->op - OP_CALL0_v6p;
op_call:
				pr->pr_xfunction->profile += profile - startprofile;
				startprofile = profile;
				PR_CallFunction (pr, OPA(func), pr->pr_return);
				st = pr->pr_statements + pr->pr_xstatement;
				break;
			case OP_DONE_v6p:
			case OP_RETURN_v6p:
				if (!st->a)
					memset (&R_INT (pr), 0,
							pr->pr_param_size * sizeof (*op_a));
				else if (&R_INT (pr) != &OPA(int))
					memcpy (&R_INT (pr), op_a,
							pr->pr_param_size * sizeof (*op_a));
				// fallthrough
			case OP_RETURN_V_v6p:
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
			case OP_STATE_v6p:
				{
					int         self = *pr->globals.self;
					int         nextthink = pr->fields.nextthink + self;
					int         frame = pr->fields.frame + self;
					int         think = pr->fields.think + self;
					float       time = *pr->globals.ftime + 0.1;
					PR_PTR (float, &pr->pr_edict_area[nextthink]) = time;
					PR_PTR (float, &pr->pr_edict_area[frame]) = OPA(float);
					PR_PTR (func, &pr->pr_edict_area[think]) = OPB(func);
				}
				break;
			case OP_STATE_F_v6p:
				{
					int         self = *pr->globals.self;
					int         nextthink = pr->fields.nextthink + self;
					int         frame = pr->fields.frame + self;
					int         think = pr->fields.think + self;
					float       time = *pr->globals.ftime + OPC(float);
					PR_PTR (float, &pr->pr_edict_area[nextthink]) = time;
					PR_PTR (float, &pr->pr_edict_area[frame]) = OPA(float);
					PR_PTR (func, &pr->pr_edict_area[think]) = OPB(func);
				}
				break;
			case OP_ADD_I_v6p:
				OPC(int) = OPA(int) + OPB(int);
				break;
			case OP_SUB_I_v6p:
				OPC(int) = OPA(int) - OPB(int);
				break;
			case OP_MUL_I_v6p:
				OPC(int) = OPA(int) * OPB(int);
				break;
			case OP_DIV_I_v6p:
				OPC(int) = OPA(int) / OPB(int);
				break;
			case OP_MOD_I_v6p:
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
			case OP_REM_I_v6p:
				OPC(int) = OPA(int) % OPB(int);
				break;
			case OP_MOD_D_v6p:
				{
					double      a = OPA(double);
					double      b = OPB(double);
					// floating point modulo is so much easier :P
					OPC(double) = a - b * floor (a / b);
				}
				break;
			case OP_REM_D_v6p:
				{
					double      a = OPA(double);
					double      b = OPB(double);
					OPC(double) = a - b * trunc (a / b);
				}
				break;
			case OP_MOD_F_v6p:
				{
					float       a = OPA(float);
					float       b = OPB(float);
					OPC(float) = a - b * floorf (a / b);
				}
				break;
			case OP_REM_F_v6p:
				{
					float       a = OPA(float);
					float       b = OPB(float);
					OPC(float) = a - b * truncf (a / b);
				}
				break;
			case OP_CONV_IF_v6p:
				OPC(float) = OPA(int);
				break;
			case OP_CONV_FI_v6p:
				OPC(int) = OPA(float);
				break;
			case OP_BITAND_I_v6p:
				OPC(int) = OPA(int) & OPB(int);
				break;
			case OP_BITOR_I_v6p:
				OPC(int) = OPA(int) | OPB(int);
				break;
			case OP_BITXOR_I_v6p:
				OPC(int) = OPA(int) ^ OPB(int);
				break;
			case OP_BITNOT_I_v6p:
				OPC(int) = ~OPA(int);
				break;

			case OP_GE_I_v6p:
			case OP_GE_P_v6p:
				OPC(int) = OPA(int) >= OPB(int);
				break;
			case OP_GE_U_v6p:
				OPC(int) = OPA(uint) >= OPB(uint);
				break;
			case OP_LE_I_v6p:
			case OP_LE_P_v6p:
				OPC(int) = OPA(int) <= OPB(int);
				break;
			case OP_LE_U_v6p:
				OPC(int) = OPA(uint) <= OPB(uint);
				break;
			case OP_GT_I_v6p:
			case OP_GT_P_v6p:
				OPC(int) = OPA(int) > OPB(int);
				break;
			case OP_GT_U_v6p:
				OPC(int) = OPA(uint) > OPB(uint);
				break;
			case OP_LT_I_v6p:
			case OP_LT_P_v6p:
				OPC(int) = OPA(int) < OPB(int);
				break;
			case OP_LT_U_v6p:
				OPC(int) = OPA(uint) < OPB(uint);
				break;

			case OP_AND_I_v6p:
				OPC(int) = OPA(int) && OPB(int);
				break;
			case OP_OR_I_v6p:
				OPC(int) = OPA(int) || OPB(int);
				break;
			case OP_NOT_I_v6p:
			case OP_NOT_P_v6p:
				OPC(int) = !OPA(int);
				break;
			case OP_EQ_I_v6p:
			case OP_EQ_P_v6p:
				OPC(int) = OPA(int) == OPB(int);
				break;
			case OP_NE_I_v6p:
			case OP_NE_P_v6p:
				OPC(int) = OPA(int) != OPB(int);
				break;

			case OP_MOVEI_v6p:
				memmove (op_c, op_a, (pr_ushort_t) st->b * 4);
				break;
			case OP_MOVEP_v6p:
				if (pr_boundscheck) {
					PR_BoundsCheckSize (pr, OPC(ptr), OPB(uint));
					PR_BoundsCheckSize (pr, OPA(ptr), OPB(uint));
				}
				memmove (pr->pr_globals + OPC(ptr),
						 pr->pr_globals + OPA(ptr),
						 OPB(uint) * 4);
				break;
			case OP_MOVEPI_v6p:
				if (pr_boundscheck) {
					PR_BoundsCheckSize (pr, OPC(ptr), (pr_ushort_t) st->b);
					PR_BoundsCheckSize (pr, OPA(ptr), (pr_ushort_t) st->b);
				}
				memmove (pr->pr_globals + OPC(ptr),
						 pr->pr_globals + OPA(ptr),
						 (pr_ushort_t) st->b * 4);
				break;
			case OP_MEMSETI_v6p:
				pr_memset (op_c, OPA(ptr), (pr_ushort_t) st->b);
				break;
			case OP_MEMSETP_v6p:
				if (pr_boundscheck) {
					PR_BoundsCheckSize (pr, OPC(ptr), OPB(uint));
				}
				pr_memset (pr->pr_globals + OPC(ptr), OPA(int),
						   OPB(uint));
				break;
			case OP_MEMSETPI_v6p:
				if (pr_boundscheck) {
					PR_BoundsCheckSize (pr, OPC(ptr), (pr_ushort_t) st->b);
				}
				pr_memset (pr->pr_globals + OPC(ptr), OPA(int),
						   (pr_ushort_t) st->b);
				break;
			case OP_GE_D_v6p:
				OPC(float) = OPA(double) >= OPB(double);
				break;
			case OP_LE_D_v6p:
				OPC(float) = OPA(double) <= OPB(double);
				break;
			case OP_GT_D_v6p:
				OPC(float) = OPA(double) > OPB(double);
				break;
			case OP_LT_D_v6p:
				OPC(float) = OPA(double) < OPB(double);
				break;
			case OP_NOT_D_v6p:
				OPC(int) = (op_a[0].value || (op_a[1].value & ~0x80000000u));
				break;
			case OP_EQ_D_v6p:
				OPC(int) = OPA(double) == OPB(double);
				break;
			case OP_NE_D_v6p:
				OPC(int) = OPA(double) != OPB(double);
				break;
			case OP_CONV_ID_v6p:
				OPC(double) = OPA(int);
				break;
			case OP_CONV_DI_v6p:
				OPC(int) = OPA(double);
				break;
			case OP_CONV_FD_v6p:
				OPC(double) = OPA(float);
				break;
			case OP_CONV_DF_v6p:
				OPC(float) = OPA(double);
				break;

// LordHavoc: to be enabled when Progs version 7 (or whatever it will be numbered) is finalized
/*
			case OP_BOUNDCHECK_v6p:
				if (OPA(ptr) >= (pr_ushort_t) st->b) {
					PR_RunError (pr, "Progs boundcheck failed at line number "
					"%d, value is < 0 or >= %d", (pr_ushort_t) st->b,
					(pr_ushort_t) st->c);
				}
				break;

*/
			default:
				PR_RunError (pr, "Bad opcode %i", st->op & ~OP_BREAK);
		}
		if (pr->watch && pr->watch->value != old_val.value) {
			if (!pr->wp_conditional
				|| pr->watch->value == pr->wp_val.value) {
				if (pr->debug_handler) {
					pr->debug_handler (prd_watchpoint, 0, pr->debug_data);
				} else {
					PR_RunError (pr, "watchpoint hit: %d -> %d",
								 old_val.value, pr->watch->value);
				}
			}
			old_val.value = pr->watch->value;
		}
	}
exit_program:;
}

#define MM(type) (*((pr_##type##_t *) (mm)))
#define STK(type) (*((pr_##type##_t *) (stk)))

static pr_type_t *
pr_address_mode (progs_t *pr, const dstatement_t *st, int mm_ind)
{
	pr_type_t  *op_a = pr->pr_globals + st->a + PR_BASE (pr, st, A);
	pr_type_t  *op_b = pr->pr_globals + st->b + PR_BASE (pr, st, B);
	pr_ptr_t    mm_offs = 0;
	pr_ptr_t    edict_area = 0;

	switch (mm_ind) {
		case 0:
			// regular global access
			mm_offs = op_a - pr->pr_globals;
			break;
		case 1:
			// entity.field (equivalent to OP_LOAD_t_v6p)
			edict_area = pr->pr_edict_area - pr->pr_globals;
			mm_offs = edict_area + OPA(entity) + OPB(field);
			break;
		case 2:
			// constant indexed pointer: *a + b (supports -ve offset)
			mm_offs = OPA(ptr) + st->b;
			break;
		case 3:
			// variable indexed pointer: *a + *b (supports -ve offset)
			mm_offs = OPA(ptr) + OPB(int);
			break;
		case 4:
			// global access with constant offset (supports -ve offset)
			mm_offs = op_a - pr->pr_globals + st->b;
			break;
		case 5:
			// global access with variable offset (supports -ve offset)
			mm_offs = op_a - pr->pr_globals + OPB(int);
			break;
	}
	return pr->pr_globals + mm_offs;
}

static pr_type_t *
pr_call_mode (progs_t *pr, const dstatement_t *st, int mm_ind)
{
	pr_type_t  *op_a = pr->pr_globals + st->a + PR_BASE (pr, st, A);
	pr_type_t  *op_b = pr->pr_globals + st->b + PR_BASE (pr, st, B);
	pr_ptr_t    mm_offs = 0;
	pr_ptr_t    edict_area = 0;

	switch (mm_ind) {
		case 1:
			// regular global access
			mm_offs = op_a - pr->pr_globals;
			break;
		case 2:
			// constant indexed pointer: *a + b (supports -ve offset)
			mm_offs = OPA(ptr) + st->b;
			break;
		case 3:
			// variable indexed pointer: *a + *b (supports -ve offset)
			mm_offs = OPA(ptr) + OPB(int);
			break;
		case 4:
			// entity.field (equivalent to OP_LOAD_t_v6p)
			edict_area = pr->pr_edict_area - pr->pr_globals;
			mm_offs = edict_area + OPA(entity) + OPB(field);
			break;
	}
	return pr->pr_globals + mm_offs;
}

static pr_ptr_t __attribute__((pure))
pr_jump_mode (progs_t *pr, const dstatement_t *st, int jump_ind)
{
	pr_type_t  *op_a = pr->pr_globals + st->a + PR_BASE (pr, st, A);
	pr_type_t  *op_b = pr->pr_globals + st->b + PR_BASE (pr, st, B);
	pr_ptr_t    jump_offs = pr->pr_xstatement;

	switch (jump_ind) {
		case 0:
			// instruction relative offset
			jump_offs = jump_offs + st->a;
			break;
		case 1:
			// variable indexed array: a + *b (only +ve)
			jump_offs = PR_PTR (uint, op_a + OPB(uint));
			break;
		case 2:
			// constant indexed pointer: *a + b (supports -ve offset)
			jump_offs = OPA(ptr) + st->b;
			break;
		case 3:
			// variable indexed pointer: *a + *b (supports -ve offset)
			jump_offs = OPA(ptr) + OPB(int);
			break;
	}
	if (pr_boundscheck && jump_offs >= pr->progs->statements.count) {
		PR_RunError (pr, "out of bounds: %x", jump_offs);
	}
	return jump_offs - 1;	// for st++
}

static pr_type_t *
pr_stack_push (progs_t *pr)
{
	// keep the stack 16-byte aligned
	pr_ptr_t    stack = *pr->globals.stack - 4;
	pr_type_t  *stk = pr->pr_globals + stack;
	if (pr_boundscheck) {
		check_stack_pointer (pr, stack, 4);
	}
	*pr->globals.stack = stack;
	return stk;
}

static pr_type_t *
pr_stack_pop (progs_t *pr)
{
	pr_ptr_t    stack = *pr->globals.stack;
	pr_type_t  *stk = pr->pr_globals + stack;
	if (pr_boundscheck) {
		check_stack_pointer (pr, stack, 4);
	}
	// keep the stack 16-byte aligned
	*pr->globals.stack = stack + 4;
	return stk;
}

static void
pr_stack_adjust (progs_t *pr, int mode, int offset)
{
	// keep the stack 16-byte aligned
	if (mode || (offset & 3)) {
		PR_RunError (pr, "invalid stack adjustment: %d, %d", mode, offset);
	}

	pr_ptr_t    stack = *pr->globals.stack;
	if (pr_boundscheck) {
		check_stack_pointer (pr, stack + offset, 0);
	}
	*pr->globals.stack = stack + offset;
}

static void
pr_with (progs_t *pr, const dstatement_t *st)
{
	pr_ptr_t    edict_area = pr->pr_edict_area - pr->pr_globals;
	pr_type_t  *op_b = pr->pr_globals + PR_BASE (pr, st, B) + st->b;
	pr_type_t  *stk;

	switch (st->a) {
		// fixed offset
		case 0:
			// hard-0 base
			pr->pr_bases[st->c & 3] = (pr_ushort_t) st->b;
			return;
		case 1:
			// relative to current base (-ve offset)
			pr->pr_bases[st->c & 3] = PR_BASE (pr, st, B) + st->b;
			return;
		case 2:
			// relative to stack (-ve offset)
			pr->pr_bases[st->c & 3] = *pr->globals.stack + st->b;
			return;
		case 3:
			// relative to edict_area (only +ve)
			pr->pr_bases[st->c & 3] = edict_area + (pr_ushort_t) st->b;
			return;

		case 4:
			// hard-0 base
			pr->pr_bases[st->c & 3] = G_POINTER (pr, (pr_ushort_t) st->b);;
			return;
		case 5:
			pr->pr_bases[st->c & 3] = OPB(ptr);
			return;
		case 6:
			// relative to stack (-ve offset)
			pr->pr_bases[st->c & 3] = *pr->globals.stack + OPB(int);
			return;
		case 7:
			// relative to edict_area (only +ve)
			pr->pr_bases[st->c & 3] = edict_area + OPB(field);
			return;

		case 8:
			// pushregs
			stk = pr_stack_push (pr);
			STK(uvec4) = pr->pr_bases;
			return;
		case 9:
			// popregs
			stk = pr_stack_pop (pr);
			pr->pr_bases = STK(uvec4);
			return;
		case 10:
			// reset
			pr->pr_bases = (pr_uvec4_t) {};
			return;
		case 11:
			// return pointer
			pr->pr_bases[st->c & 3] = pr->pr_return - pr->pr_globals;
			return;
	}
	PR_RunError (pr, "Invalid with index: %u", st->a);
}

static pr_ivec4_t
pr_swizzle_f (pr_ivec4_t vec, pr_ushort_t swiz)
{
	goto do_swizzle;
#include "libs/gamecode/pr_swizzle32.cinc"
	static const pr_ivec4_t neg[16] = {
		{     0,     0,     0,     0 },
		{ 1<<31,     0,     0,     0 },
		{     0, 1<<31,     0,     0 },
		{ 1<<31, 1<<31,     0,     0 },
		{     0,     0, 1<<31,     0 },
		{ 1<<31,     0, 1<<31,     0 },
		{     0, 1<<31, 1<<31,     0 },
		{ 1<<31, 1<<31, 1<<31,     0 },
		{     0,     0,     0, 1<<31 },
		{ 1<<31,     0,     0, 1<<31 },
		{     0, 1<<31,     0, 1<<31 },
		{ 1<<31, 1<<31,     0, 1<<31 },
		{     0,     0, 1<<31, 1<<31 },
		{ 1<<31,     0, 1<<31, 1<<31 },
		{     0, 1<<31, 1<<31, 1<<31 },
		{ 1<<31, 1<<31, 1<<31, 1<<31 },
	};
	static const pr_ivec4_t zero[16] = {
		{ ~0, ~0, ~0, ~0 },
		{  0, ~0, ~0, ~0 },
		{ ~0,  0, ~0, ~0 },
		{  0,  0, ~0, ~0 },
		{ ~0, ~0,  0, ~0 },
		{  0, ~0,  0, ~0 },
		{ ~0,  0,  0, ~0 },
		{  0,  0,  0, ~0 },
		{ ~0, ~0, ~0,  0 },
		{  0, ~0, ~0,  0 },
		{ ~0,  0, ~0,  0 },
		{  0,  0, ~0,  0 },
		{ ~0, ~0,  0,  0 },
		{  0, ~0,  0,  0 },
		{ ~0,  0,  0,  0 },
		{  0,  0,  0,  0 },
	};

do_swizzle:
	goto *swizzle_table[swiz & 0xff];
negate:
	vec ^= neg[(swiz >> 8) & 0xf];
	vec &= zero[(swiz >> 12) & 0xf];
	return vec;
}

static pr_lvec4_t
#ifdef _WIN64
//force gcc to use registers for the parameters to avoid alignment issues
//on the stack (gcc bug as of 11.2)
__attribute__((sysv_abi))
#endif
pr_swizzle_d (pr_lvec4_t vec, pr_ushort_t swiz)
{
	goto do_swizzle;
#include "libs/gamecode/pr_swizzle64.cinc"
#define L(x) UINT64_C(x)
	static const pr_lvec4_t neg[16] = {
		{     INT64_C(0),     INT64_C(0),     INT64_C(0),     INT64_C(0) },
		{ INT64_C(1)<<63,     INT64_C(0),     INT64_C(0),     INT64_C(0) },
		{     INT64_C(0), INT64_C(1)<<63,     INT64_C(0),     INT64_C(0) },
		{ INT64_C(1)<<63, INT64_C(1)<<63,     INT64_C(0),     INT64_C(0) },
		{     INT64_C(0),     INT64_C(0), INT64_C(1)<<63,     INT64_C(0) },
		{ INT64_C(1)<<63,     INT64_C(0), INT64_C(1)<<63,     INT64_C(0) },
		{     INT64_C(0), INT64_C(1)<<63, INT64_C(1)<<63,     INT64_C(0) },
		{ INT64_C(1)<<63, INT64_C(1)<<63, INT64_C(1)<<63,     INT64_C(0) },
		{     INT64_C(0),     INT64_C(0),     INT64_C(0), INT64_C(1)<<63 },
		{ INT64_C(1)<<63,     INT64_C(0),     INT64_C(0), INT64_C(1)<<63 },
		{     INT64_C(0), INT64_C(1)<<63,     INT64_C(0), INT64_C(1)<<63 },
		{ INT64_C(1)<<63, INT64_C(1)<<63,     INT64_C(0), INT64_C(1)<<63 },
		{     INT64_C(0),     INT64_C(0), INT64_C(1)<<63, INT64_C(1)<<63 },
		{ INT64_C(1)<<63,     INT64_C(0), INT64_C(1)<<63, INT64_C(1)<<63 },
		{     INT64_C(0), INT64_C(1)<<63, INT64_C(1)<<63, INT64_C(1)<<63 },
		{ INT64_C(1)<<63, INT64_C(1)<<63, INT64_C(1)<<63, INT64_C(1)<<63 },
	};
	static const pr_lvec4_t zero[16] = {
		{ ~INT64_C(0), ~INT64_C(0), ~INT64_C(0), ~INT64_C(0) },
		{  INT64_C(0), ~INT64_C(0), ~INT64_C(0), ~INT64_C(0) },
		{ ~INT64_C(0),  INT64_C(0), ~INT64_C(0), ~INT64_C(0) },
		{  INT64_C(0),  INT64_C(0), ~INT64_C(0), ~INT64_C(0) },
		{ ~INT64_C(0), ~INT64_C(0),  INT64_C(0), ~INT64_C(0) },
		{  INT64_C(0), ~INT64_C(0),  INT64_C(0), ~INT64_C(0) },
		{ ~INT64_C(0),  INT64_C(0),  INT64_C(0), ~INT64_C(0) },
		{  INT64_C(0),  INT64_C(0),  INT64_C(0), ~INT64_C(0) },
		{ ~INT64_C(0), ~INT64_C(0), ~INT64_C(0),  INT64_C(0) },
		{  INT64_C(0), ~INT64_C(0), ~INT64_C(0),  INT64_C(0) },
		{ ~INT64_C(0),  INT64_C(0), ~INT64_C(0),  INT64_C(0) },
		{  INT64_C(0),  INT64_C(0), ~INT64_C(0),  INT64_C(0) },
		{ ~INT64_C(0), ~INT64_C(0),  INT64_C(0),  INT64_C(0) },
		{  INT64_C(0), ~INT64_C(0),  INT64_C(0),  INT64_C(0) },
		{ ~INT64_C(0),  INT64_C(0),  INT64_C(0),  INT64_C(0) },
		{  INT64_C(0),  INT64_C(0),  INT64_C(0),  INT64_C(0) },
	};

do_swizzle:
	goto *swizzle_table[swiz & 0xff];
negate:
	vec ^= neg[(swiz >> 8) & 0xf];
	vec &= zero[(swiz >> 12) & 0xf];
	return vec;
}

static void
pr_exec_ruamoko (progs_t *pr, int exitdepth)
{
	int         profile, startprofile;
	dstatement_t *st;
	pr_type_t   old_val = {0};

	// make a stack frame
	startprofile = profile = 0;

	st = pr->pr_statements + pr->pr_xstatement;

	if (pr->watch) {
		old_val = *pr->watch;
	}

	while (1) {
		st++;
		++pr->pr_xstatement;
		if (pr->pr_xstatement != st - pr->pr_statements)
			PR_RunError (pr, "internal error");
		if (++profile > 1000000 && !pr->no_exec_limit) {
			PR_RunError (pr, "runaway loop error");
		}

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

		pr_ptr_t    st_a = st->a + PR_BASE (pr, st, A);
		pr_ptr_t    st_b = st->b + PR_BASE (pr, st, B);
		pr_ptr_t    st_c = st->c + PR_BASE (pr, st, C);


		pr_type_t  *op_a = pr->pr_globals + st_a;
		pr_type_t  *op_b = pr->pr_globals + st_b;
		pr_type_t  *op_c = pr->pr_globals + st_c;

		pr_type_t  *stk;
		pr_type_t  *mm;
		pr_func_t   function;

		int         ret_size = 0;

		pr_opcode_e st_op = st->op & OP_MASK;
		switch (st_op) {
			// 0 0000
			case OP_NOP:
				break;
			case OP_ADJSTK:
				pr_stack_adjust (pr, (pr_ushort_t) st->a, st->b);
				break;
			case OP_LDCONST:
				PR_RunError (pr, "OP_LDCONST not implemented");
				break;
			case OP_LOAD_B_1:
			case OP_LOAD_C_1:
			case OP_LOAD_D_1:
				mm = pr_address_mode (pr, st, (st_op - OP_LOAD_B_1 + 4) >> 2);
				OPC(int) = MM(int);
				break;
			case OP_LOAD_B_2:
			case OP_LOAD_C_2:
			case OP_LOAD_D_2:
				mm = pr_address_mode (pr, st, (st_op - OP_LOAD_B_2 + 4) >> 2);
				OPC(ivec2) = MM(ivec2);
				break;
			case OP_LOAD_B_3:
			case OP_LOAD_C_3:
			case OP_LOAD_D_3:
				mm = pr_address_mode (pr, st, (st_op - OP_LOAD_B_3 + 4) >> 2);
				VectorCopy (&MM(int), &OPC(int));
				break;
			case OP_LOAD_B_4:
			case OP_LOAD_C_4:
			case OP_LOAD_D_4:
				mm = pr_address_mode (pr, st, (st_op - OP_LOAD_B_4 + 4) >> 2);
				OPC(ivec4) = MM(ivec4);
				break;
			// 0 0001
			case OP_STORE_A_1:
			case OP_STORE_B_1:
			case OP_STORE_C_1:
			case OP_STORE_D_1:
				mm = pr_address_mode (pr, st, (st_op - OP_STORE_A_1) >> 2);
				MM(int) = OPC(int);
				break;
			case OP_STORE_A_2:
			case OP_STORE_B_2:
			case OP_STORE_C_2:
			case OP_STORE_D_2:
				mm = pr_address_mode (pr, st, (st_op - OP_STORE_A_2) >> 2);
				MM(ivec2) = OPC(ivec2);
				break;
			case OP_STORE_A_3:
			case OP_STORE_B_3:
			case OP_STORE_C_3:
			case OP_STORE_D_3:
				mm = pr_address_mode (pr, st, (st_op - OP_STORE_A_3) >> 2);
				VectorCopy (&OPC(int), &MM(int));
				break;
			case OP_STORE_A_4:
			case OP_STORE_B_4:
			case OP_STORE_C_4:
			case OP_STORE_D_4:
				mm = pr_address_mode (pr, st, (st_op - OP_STORE_A_4) >> 2);
				MM(ivec4) = OPC(ivec4);
				break;
			// 0 0010
			case OP_PUSH_A_1:
			case OP_PUSH_B_1:
			case OP_PUSH_C_1:
			case OP_PUSH_D_1:
				mm = pr_address_mode (pr, st, (st_op - OP_PUSH_A_1) >> 2);
				stk = pr_stack_push (pr);
				STK(int) = MM(int);
				break;
			case OP_PUSH_A_2:
			case OP_PUSH_B_2:
			case OP_PUSH_C_2:
			case OP_PUSH_D_2:
				mm = pr_address_mode (pr, st, (st_op - OP_PUSH_A_2) >> 2);
				stk = pr_stack_push (pr);
				STK(ivec2) = MM(ivec2);
				break;
			case OP_PUSH_A_3:
			case OP_PUSH_B_3:
			case OP_PUSH_C_3:
			case OP_PUSH_D_3:
				mm = pr_address_mode (pr, st, (st_op - OP_PUSH_A_3) >> 2);
				stk = pr_stack_push (pr);
				VectorCopy (&MM(int), &STK(int));
				break;
			case OP_PUSH_A_4:
			case OP_PUSH_B_4:
			case OP_PUSH_C_4:
			case OP_PUSH_D_4:
				mm = pr_address_mode (pr, st, (st_op - OP_PUSH_A_4) >> 2);
				stk = pr_stack_push (pr);
				STK(ivec4) = MM(ivec4);
				break;
			// 0 0011
			case OP_POP_A_1:
			case OP_POP_B_1:
			case OP_POP_C_1:
			case OP_POP_D_1:
				mm = pr_address_mode (pr, st, (st_op - OP_POP_A_1) >> 2);
				stk = pr_stack_pop (pr);
				MM(int) = STK(int);
				break;
			case OP_POP_A_2:
			case OP_POP_B_2:
			case OP_POP_C_2:
			case OP_POP_D_2:
				mm = pr_address_mode (pr, st, (st_op - OP_POP_A_2) >> 2);
				stk = pr_stack_pop (pr);
				MM(ivec2) = STK(ivec2);
				break;
			case OP_POP_A_3:
			case OP_POP_B_3:
			case OP_POP_C_3:
			case OP_POP_D_3:
				mm = pr_address_mode (pr, st, (st_op - OP_POP_A_3) >> 2);
				stk = pr_stack_pop (pr);
				VectorCopy (&STK(int), &MM(int));
				break;
			case OP_POP_A_4:
			case OP_POP_B_4:
			case OP_POP_C_4:
			case OP_POP_D_4:
				mm = pr_address_mode (pr, st, (st_op - OP_POP_A_4) >> 2);
				stk = pr_stack_pop (pr);
				MM(ivec4) = STK(ivec4);
				break;

#define OP_mvmul_T_3(T,cols,rows,t) \
			case OP_MVMUL_##cols##3##_##T: \
				{ \
					auto a = &OPA(t##vec##rows); \
					auto b = OPB(t##vec##cols); \
					auto c = OPC(t##vec##rows); \
					VectorScale (a[0], b[0], c); \
					for (int i = 1; i < cols; i++) { \
						VectorMultAdd(c, b[i], a[i], c); \
					} \
				} \
				break
#define OP_mvmul_T(T,cols,rows,t) \
			case OP_MVMUL_##cols##rows##_##T: \
				{ \
					auto a = &OPA(t##vec##rows); \
					auto b = OPB(t##vec##cols); \
					pr_##t##vec##rows##_t c = a[0] * b[0]; \
					for (int i = 1; i < cols; i++) { \
						c += a[i] * b[i]; \
					} \
					OPC(t##vec##rows) = c; \
				} \
				break
			// 0 0100
			OP_mvmul_T  (F,3,2,);
			OP_mvmul_T_3(F,3,3,);
			OP_mvmul_T  (F,3,4,);
			OP_mvmul_T_3(F,2,3,);
			OP_mvmul_T  (D,3,2,d);
			OP_mvmul_T_3(D,3,3,d);
			OP_mvmul_T  (D,3,4,d);
			OP_mvmul_T_3(D,2,3,d);
			OP_mvmul_T  (F,4,2,);
			OP_mvmul_T_3(F,4,3,);
			OP_mvmul_T  (F,4,4,);
			OP_mvmul_T  (F,2,4,);
			OP_mvmul_T  (D,4,2,d);
			OP_mvmul_T_3(D,4,3,d);
			OP_mvmul_T  (D,4,4,d);
			OP_mvmul_T  (D,2,4,d);

#define OP_vmmul_T_3(T,cols,rows,t) \
			case OP_VMMUL_##cols##3##_##T: \
				{ \
					auto a = OPA(t##vec##rows); \
					auto b = &OPB(t##vec##rows); \
					for (int i = 0; i < cols; i++) { \
						OPC(t##vec##cols)[i] = DotProduct (a, b[i]); \
					} \
				} \
				break
#define OP_vmmul_T(T,cols,rows,t,t2) \
			case OP_VMMUL_##cols##rows##_##T: \
				{ \
					auto a = OPA(t##vec##rows); \
					auto b = &OPB(t##vec##rows); \
					for (int i = 0; i < cols; i++) { \
						OPC(t##vec##cols)[i] = dot##rows##t2(a, b[i])[0]; \
					} \
				} \
				break
#define dot4f dotf
#define dot4d dotd
			// 0 0101
			OP_vmmul_T  (F,3,2,,f);
			OP_vmmul_T_3(F,3,3,);
			OP_vmmul_T  (F,3,4,,f);
			OP_vmmul_T_3(F,2,3,);
			OP_vmmul_T  (D,3,2,d,d);
			OP_vmmul_T_3(D,3,3,d);
			OP_vmmul_T  (D,3,4,d,d);
			OP_vmmul_T_3(D,2,3,d);
			OP_vmmul_T  (F,4,2,,f);
			OP_vmmul_T_3(F,4,3,);
			OP_vmmul_T  (F,4,4,,f);
			OP_vmmul_T  (F,2,4,,f);
			OP_vmmul_T  (D,4,2,d,d);
			OP_vmmul_T_3(D,4,3,d);
			OP_vmmul_T  (D,4,4,d,d);
			OP_vmmul_T  (D,2,4,d,d);
#undef dot4f
#undef dot4d

#define OP_outer_T(T,cols,rows,t) \
			case OP_OUTER_##cols##rows##_##T: \
				{ \
					auto a = OPA(t##vec##rows); \
					auto b = OPB(t##vec##cols); \
					auto c = &OPC(t##vec##rows); \
					for (int i = 0; i < rows; i++) { \
						for (int j = 0; j < cols; j++) { \
							c[i][j] = a[i] * b[j]; \
						} \
					} \
				} \
				break
			// 0 0110
			OP_outer_T(F,3,2,);
			OP_outer_T(F,3,3,);
			OP_outer_T(F,3,4,);
			OP_outer_T(F,2,3,);
			OP_outer_T(D,3,2,d);
			OP_outer_T(D,3,3,d);
			OP_outer_T(D,3,4,d);
			OP_outer_T(D,2,3,d);
			OP_outer_T(F,4,2,);
			OP_outer_T(F,4,3,);
			OP_outer_T(F,4,4,);
			OP_outer_T(F,2,4,);
			OP_outer_T(D,4,2,d);
			OP_outer_T(D,4,3,d);
			OP_outer_T(D,4,4,d);
			OP_outer_T(D,2,4,d);

			// 0 0111
			OP_mvmul_T(F,2,2,);
			OP_vmmul_T(F,2,2,,f);
			OP_outer_T(F,2,2,);
			case OP_WEDGE_F_2:
				{
					auto a = OPA(vec2);
					auto b = OPB(vec2);
					OPC(float) = a[0] * b[1] - a[1] * b[0];
				}
				break;
			OP_mvmul_T(D,2,2,d);
			OP_vmmul_T(D,2,2,d,d);
			OP_outer_T(D,2,2,d);
			case OP_WEDGE_D_2:
				{
					auto a = OPA(dvec2);
					auto b = OPB(dvec2);
					OPC(double) = a[0] * b[1] - a[1] * b[0];
				}
				break;
			case OP_SWIZZLE_F_2:
				{
					auto s2 = OPA(ivec2);
					pr_ivec4_t s4 = { s2[0], s2[1] };
					s4 = pr_swizzle_f (s4, (pr_ushort_t) st->b);
					OPC(ivec2) = (pr_ivec2_t) { s4[0], s4[1] };
				}
				break;
			case OP_SWIZZLE_F_3:
				{
					auto s4 = loadvec3i (&OPA(int));
					s4 = pr_swizzle_f (s4, (pr_ushort_t) st->b);
					storevec3i (&OPC(int), s4);
				}
				break;
			case OP_SWIZZLE_D_2:
				{
					auto s2 = OPA(lvec2);
					pr_lvec4_t s4 = { s2[0], s2[1] };
					s4 = pr_swizzle_d (s4, (pr_ushort_t) st->b);
					OPC(lvec2) = (pr_lvec2_t) { s4[0], s4[1] };
				}
				break;
			case OP_SWIZZLE_D_3:
				{
					auto s4 = loadvec3l (&OPA(long));
					s4 = pr_swizzle_d (s4, (pr_ushort_t) st->b);
					storevec3l (&OPC(long), s4);
				}
				break;
			// spare
			// spare
			// spare
			// spare

#define OP_cmp_1(OP, T, rt, cmp, ct) \
			case OP_##OP##_##T##_1: \
				OPC(rt) = -(OPA(ct) cmp OPB(ct)); \
				break
#define OP_cmp_2(OP, T, rt, cmp, ct) \
			case OP_##OP##_##T##_2: \
				OPC(rt) = (OPA(ct) cmp OPB(ct)); \
				break
#define OP_cmp_3(OP, T, rt, cmp, ct) \
			case OP_##OP##_##T##_3: \
				VectorCompCompare (&OPC(rt), -, &OPA(ct), cmp, &OPB(ct)); \
				break;
#define OP_cmp_4(OP, T, rt, cmp, ct) \
			case OP_##OP##_##T##_4: \
				OPC(rt) = (OPA(ct) cmp OPB(ct)); \
				break
#define OP_cmp_T(OP, T, rt1, rt2, rt4, cmp, ct1, ct2, ct4) \
			OP_cmp_1 (OP, T, rt1, cmp, ct1); \
			OP_cmp_2 (OP, T, rt2, cmp, ct2); \
			OP_cmp_3 (OP, T, rt1, cmp, ct1); \
			OP_cmp_4 (OP, T, rt4, cmp, ct4)
#define OP_cmp(OP, cmp) \
			OP_cmp_T (OP, I, int, ivec2, ivec4, cmp, int, ivec2, ivec4); \
			OP_cmp_T (OP, F, int, ivec2, ivec4, cmp, float, vec2, vec4); \
			OP_cmp_T (OP, L, long, lvec2, lvec4, cmp, long, lvec2, lvec4); \
			OP_cmp_T (OP, D, long, lvec2, lvec4, cmp, double, dvec2, dvec4)

			// 0 1000
			OP_cmp(EQ, ==);
			// 0 1001
			OP_cmp(LT, <);
			// 0 1010
			OP_cmp(GT, >);

#define OP_op_1(OP, T, t, op) \
			case OP_##OP##_##T##_1: \
				OPC(t) = (OPA(t) op OPB(t)); \
				break
#define OP_op_2(OP, T, t, op) \
			case OP_##OP##_##T##_2: \
				OPC(t) = (OPA(t) op OPB(t)); \
				break
#define OP_op_3(OP, T, t, op) \
			case OP_##OP##_##T##_3: \
				VectorCompOp (&OPC(t), &OPA(t), op, &OPB(t)); \
				break;
#define OP_op_4(OP, T, t, op) \
			case OP_##OP##_##T##_4: \
				OPC(t) = (OPA(t) op OPB(t)); \
				break
#define OP_op_T(OP, T, t1, t2, t4, op) \
			OP_op_1 (OP, T, t1, op); \
			OP_op_2 (OP, T, t2, op); \
			OP_op_3 (OP, T, t1, op); \
			OP_op_4 (OP, T, t4, op)
			// 0 1011
			OP_op_T (DIV, u, uint, uvec2, uvec4, /);
			OP_op_T (DIV, U, ulong, ulvec2, ulvec4, /);
			OP_op_T (REM, u, uint, uvec2, uvec4, %);
			OP_op_T (REM, U, ulong, ulvec2, ulvec4, %);
			// 0 1100
			OP_cmp(NE, !=);
			// 0 1101
			OP_cmp(GE, >=);
			// 0 1110
			OP_cmp(LE, <=);
			// 0 1111
			case OP_LEA_E:
				mm = pr_address_mode (pr, st, 4);
				OPC(ptr) = mm - pr->pr_globals;
				break;
			case OP_LOAD64_B_3:
			case OP_LOAD64_C_3:
			case OP_LOAD64_D_3:
				mm = pr_address_mode (pr, st, (st_op - OP_LOAD64_B_3 + 1));
				VectorCopy (&MM(long), &OPC(long));
				break;
			case OP_STORE64_A_3:
			case OP_STORE64_B_3:
			case OP_STORE64_C_3:
			case OP_STORE64_D_3:
				mm = pr_address_mode (pr, st, (st_op - OP_STORE64_A_3));
				VectorCopy (&OPC(long), &MM(long));
				break;
			case OP_LEA_F:
				mm = pr_address_mode (pr, st, 5);
				OPC(ptr) = mm - pr->pr_globals;
				break;
			case OP_LOAD64_B_4:
			case OP_LOAD64_C_4:
			case OP_LOAD64_D_4:
				mm = pr_address_mode (pr, st, (st_op - OP_LOAD64_B_4 + 1));
				OPC(lvec4) = MM(lvec4);
				break;
			case OP_STORE64_A_4:
			case OP_STORE64_B_4:
			case OP_STORE64_C_4:
			case OP_STORE64_D_4:
				mm = pr_address_mode (pr, st, (st_op - OP_STORE64_A_4));
				MM(lvec4) = OPC(lvec4);
				break;

#define OP_op(OP, op) \
			OP_op_T (OP, I, int, ivec2, ivec4, op); \
			OP_op_T (OP, F, float, vec2, vec4, op); \
			OP_op_T (OP, L, long, lvec2, lvec4, op); \
			OP_op_T (OP, D, double, dvec2, dvec4, op)
#define OP_uop_1(OP, T, t, op) \
			case OP_##OP##_##T##_1: \
				OPC(t) = op (OPA(t)); \
				break
#define OP_uop_2(OP, T, t, op) \
			case OP_##OP##_##T##_2: \
				OPC(t) = op (OPA(t)); \
				break
#define OP_uop_3(OP, T, t, op) \
			case OP_##OP##_##T##_3: \
				VectorCompUop (&OPC(t), op, &OPA(t)); \
				break;
#define OP_uop_4(OP, T, t, op) \
			case OP_##OP##_##T##_4: \
				OPC(t) = op (OPA(t)); \
				break
#define OP_uop_T(OP, T, t1, t2, t4, op) \
			OP_uop_1 (OP, T, t1, op); \
			OP_uop_2 (OP, T, t2, op); \
			OP_uop_3 (OP, T, t1, op); \
			OP_uop_4 (OP, T, t4, op)

			// 1 0000
			OP_op(MUL, *);
			// 1 0001
			OP_op(DIV, /);

// implement remainder (c %) for integers:
//  5 rem  3 =  2
// -5 rem  3 = -2
//  5 rem -3 =  2
// -5 rem -3 = -2
#define OP_store(d, s) *(d) = s
#define OP_remmod_T(OP, T, n, t, l, f, s) \
			case OP_##OP##_##T##_##n: \
				{ \
					__auto_type a = l (&OPA(t)); \
					__auto_type b = l (&OPB(t)); \
					s (&OPC(t), a - b * f(a / b)); \
				} \
				break
#define OP_rem_T(T, n, t, l, f, s) \
			OP_remmod_T(REM, T, n, t, l, f, s)

			// 1 0010
			OP_op_T (REM, I, int, ivec2, ivec4, %);
			OP_rem_T (F, 1, float, *, truncf, OP_store);
			OP_rem_T (F, 2, vec2, *, vtrunc2f, OP_store);
			OP_rem_T (F, 3, float, loadvec3f, vtrunc4f, storevec3f);
			OP_rem_T (F, 4, vec4, *, vtrunc4f, OP_store);
			OP_op_T (REM, L, long, lvec2, lvec4, %);
			OP_rem_T (D, 1, double, *, trunc, OP_store);
			OP_rem_T (D, 2, dvec2, *, vtrunc2d, OP_store);
			OP_rem_T (D, 3, double, loadvec3d, vtrunc4d, storevec3d);
			OP_rem_T (D, 4, dvec4, *, vtrunc4d, OP_store);

// implement true modulo (python %) for integers:
//  5 mod  3 =  2
// -5 mod  3 =  1
//  5 mod -3 = -1
// -5 mod -3 = -2
#define OP_mod_Ti(T, n, t, l, m, s) \
			case OP_MOD_##T##_##n: \
				{ \
					__auto_type a = l(&OPA(t)); \
					__auto_type b = l(&OPB(t)); \
					__auto_type c = a % b; \
					/* % is really remainder and so has the same sign rules */\
					/* as division: -5 % 3 = -2, so need to add b (3 here)  */\
					/* if c's sign is incorrect, but only if c is non-zero  */\
					__auto_type mask = m((a ^ b) < 0); \
					mask &= m(c != 0); \
					s(&OPC(t), c + (mask & b)); \
				} \
				break
// floating point modulo is so much easier :P (just use floor instead of trunc)
#define OP_mod_Tf(T, n, t, l, f, s) \
			OP_remmod_T(MOD, T, n, t, l, f, s)

			// 1 0011
			OP_mod_Ti (I, 1, int, *, -, OP_store);
			OP_mod_Ti (I, 2, ivec2, *, +, OP_store);
			OP_mod_Ti (I, 3, int, loadvec3i1, +, storevec3i);
			OP_mod_Ti (I, 4, ivec4, *, +, OP_store);
			OP_mod_Tf (F, 1, float, *, floorf, OP_store);
			OP_mod_Tf (F, 2, vec2, *, vfloor2f, OP_store);
			OP_mod_Tf (F, 3, float, loadvec3f, vfloor4f, storevec3f);
			OP_mod_Tf (F, 4, vec4, *, vfloor4f, OP_store);
			OP_mod_Ti (L, 1, long, *, -, OP_store);
			OP_mod_Ti (L, 2, lvec2, *, +, OP_store);
			OP_mod_Ti (L, 3, long, loadvec3l1, +, storevec3l);
			OP_mod_Ti (L, 4, lvec4, *, +, OP_store);
			OP_mod_Tf (D, 1, double, *, floor, OP_store);
			OP_mod_Tf (D, 2, dvec2, *, vfloor2d, OP_store);
			OP_mod_Tf (D, 3, double, loadvec3d, vfloor4d, storevec3d);
			OP_mod_Tf (D, 4, dvec4, *, vfloor4d, OP_store);

			// 1 0100
			OP_op(ADD, +);
			// 1 0101
			OP_op(SUB, -);
			// 1 0110
			OP_op_T (SHL, u, uint, ivec2, ivec4, <<);
			OP_op_T (SHL, U, ulong, lvec2, lvec4, <<);
			case OP_EQ_S:
			case OP_LT_S:
			case OP_GT_S:
			case OP_CMP_S:
			case OP_GE_S:
			case OP_LE_S:
				{
					int         cmp = strcmp (PR_GetString (pr, OPA(string)),
											  PR_GetString (pr, OPB(string)));
					switch (st_op) {
						case OP_EQ_S: cmp = -(cmp == 0); break;
						case OP_LT_S: cmp = -(cmp <  0); break;
						case OP_GT_S: cmp = -(cmp >  0); break;
						case OP_GE_S: cmp = -(cmp >= 0); break;
						case OP_LE_S: cmp = -(cmp <= 0); break;
						case OP_CMP_S: break;
						default: break;
					}
					OPC(int) = cmp;
				}
				break;
			case OP_ADD_S:
				OPC(string) = PR_CatStrings(pr, PR_GetString (pr, OPA(string)),
											PR_GetString (pr, OPB(string)));
				break;
			case OP_NOT_S:
				OPC(int) = -(!OPA(string) || !*PR_GetString (pr, OPA(string)));
				break;
			// 1 0111
			OP_op_T (ASR, I, int, ivec2, ivec4, >>);
			OP_op_T (SHR, u, uint, uvec2, uvec4, >>);
			OP_op_T (ASR, L, long, lvec2, lvec4, >>);
			OP_op_T (SHR, U, ulong, ulvec2, ulvec4, >>);
			// 1 1000
			OP_op_T (BITAND, I, int, ivec2, ivec4, &);
			OP_op_T (BITOR, I, int, ivec2, ivec4, |);
			OP_op_T (BITXOR, I, int, ivec2, ivec4, ^);
			OP_uop_T (BITNOT, I, int, ivec2, ivec4, ~);
			// 1 1001
			OP_cmp_T (LT, u, int, ivec2, ivec4, <, uint, uvec2, uvec4);
			case OP_JUMP_A:
			case OP_JUMP_B:
			case OP_JUMP_C:
			case OP_JUMP_D:
				pr->pr_xstatement = pr_jump_mode (pr, st, st_op - OP_JUMP_A);
				st = pr->pr_statements + pr->pr_xstatement;
				break;
			OP_cmp_T (LT, U, long, lvec2, lvec4, <, ulong, ulvec2, ulvec4);
			case OP_RETURN:
				ret_size = (st->c & 0x1f) + 1;	// up to 32 words
				if ((pr_ushort_t) st->c != 0xffff) {
					mm = pr_address_mode (pr, st, ((pr_ushort_t) st->c) >> 5);
					memcpy (&R_INT (pr), mm, ret_size * sizeof (*op_a));
				}
				pr->pr_xfunction->profile += profile - startprofile;
				startprofile = profile;
				PR_LeaveFunction (pr, pr->pr_depth == exitdepth);
				st = pr->pr_statements + pr->pr_xstatement;
				if (pr->pr_depth == exitdepth) {
					if (pr->pr_trace && pr->pr_depth <= pr->pr_trace_depth) {
						pr->pr_trace = false;
					}
					goto exit_program;
				}
				break;
			case OP_CALL_B:
			case OP_CALL_C:
			case OP_CALL_D:
				mm = pr_call_mode (pr, st, st_op - OP_CALL_B + 1);
				function = PR_PTR (func, mm);
				pr->pr_argc = 0;
				// op_c specifies the location for the return value if any
				pr->pr_xfunction->profile += profile - startprofile;
				startprofile = profile;
				PR_CallFunction (pr, function, op_c);
				st = pr->pr_statements + pr->pr_xstatement;
				break;
			// 1 1010
			OP_cmp_T (GT, u, int, ivec2, ivec4, >, uint, uvec2, uvec4);
			case OP_SWIZZLE_F_4:
				OPC(ivec4) = pr_swizzle_f (OPA(ivec4), (pr_ushort_t) st->b);
				break;
			case OP_SCALE_F_2:
				OPC(vec2) = OPA(vec2) * OPB(float);
				break;
			case OP_SCALE_F_3:
				VectorScale (&OPA(float), OPB(float), &OPC(float));
				break;
			case OP_SCALE_F_4:
				OPC(vec4) = OPA(vec4) * OPB(float);
				break;
			OP_cmp_T (GT, U, long, lvec2, lvec4, >, ulong, ulvec2, ulvec4);
			case OP_SWIZZLE_D_4:
				OPC(lvec4) = pr_swizzle_d (OPA(lvec4), (pr_ushort_t) st->b);
				break;
			case OP_SCALE_D_2:
				OPC(dvec2) = OPA(dvec2) * OPB(double);
				break;
			case OP_SCALE_D_3:
				VectorScale (&OPA(double), OPB(double), &OPC(double));
				break;
			case OP_SCALE_D_4:
				OPC(dvec4) = OPA(dvec4) * OPB(double);
				break;
			// 1 1011
			case OP_CROSS_F:
				{
					pr_vec4_t   a = loadvec3f (&OPA(float));
					pr_vec4_t   b = loadvec3f (&OPB(float));
					pr_vec4_t   c = crossf (a, b);
					storevec3f (&OPC(float), c);
				}
				break;
			case OP_CDOT_F:
				OPC(float) = dot2f (OPA(vec2), OPB(vec2))[0];
				break;
			case OP_VDOT_F:
				OPC(float) = DotProduct (&OPA(float), &OPB(float));
				break;
			case OP_QDOT_F:
				OPC(float) = dotf (OPA(vec4), OPB(vec4))[0];
				break;
			case OP_CMUL_F:
				OPC(vec2) = cmulf (OPA(vec2), OPB(vec2));
				break;
			case OP_QVMUL_F:
				{
					pr_vec4_t   v = loadvec3f (&OPB(float));
					v = qvmulf (OPA(vec4), v);
					storevec3f (&OPC(float), v);
				}
				break;
			case OP_VQMUL_F:
				{
					pr_vec4_t   v = loadvec3f (&OPA(float));
					v = vqmulf (v, OPB(vec4));
					storevec3f (&OPC(float), v);
				}
				break;
			case OP_QMUL_F:
				OPC(vec4) = qmulf (OPA(vec4), OPB(vec4));
				break;
			case OP_CROSS_D:
				{
					pr_dvec4_t  a = loadvec3d (&OPA(double));
					pr_dvec4_t  b = loadvec3d (&OPB(double));
					pr_dvec4_t  c = crossd (a, b);
					storevec3d (&OPC(double), c);
				}
				break;
			case OP_CDOT_D:
				OPC(double) = dot2d (OPA(dvec2), OPB(dvec2))[0];
				break;
			case OP_VDOT_D:
				OPC(double) = DotProduct (&OPA(double), &OPB(double));
				break;
			case OP_QDOT_D:
				OPC(double) = dotd (OPA(dvec4), OPB(dvec4))[0];
				break;
			case OP_CMUL_D:
				OPC(dvec2) = cmuld (OPA(dvec2), OPB(dvec2));
				break;
			case OP_QVMUL_D:
				{
					pr_dvec4_t   v = loadvec3d (&OPB(double));
					v = qvmuld (OPA(dvec4), v);
					storevec3d (&OPC(double), v);
				}
				break;
			case OP_VQMUL_D:
				{
					pr_dvec4_t  v = loadvec3d (&OPA(double));
					v = vqmuld (v, OPB(dvec4));
					storevec3d (&OPC(double), v);
				}
				break;
			case OP_QMUL_D:
				OPC(dvec4) = qmuld (OPA(dvec4), OPB(dvec4));
				break;
			// 1 1100
			OP_op_T (BITAND, L, long, lvec2, lvec4, &);
			OP_op_T (BITOR, L, long, lvec2, lvec4, |);
			OP_op_T (BITXOR, L, long, lvec2, lvec4, ^);
			OP_uop_T (BITNOT, L, long, lvec2, lvec4, ~);
			// 1 1101
			OP_cmp_T (GE, u, int, ivec2, ivec4, >=, uint, uvec2, uvec4);
			case OP_MOVE_I:
				memmove (op_c, op_a, (pr_ushort_t) st->b * sizeof (pr_type_t));
				break;
			case OP_MOVE_P:
				memmove (pr->pr_globals + OPC(ptr), pr->pr_globals + OPA(ptr),
						 OPB(uint) * sizeof (pr_type_t));
				break;
			case OP_MOVE_PI:
				memmove (pr->pr_globals + OPC(ptr), pr->pr_globals + OPA(ptr),
						 (pr_ushort_t) st->b * sizeof (pr_type_t));
				break;
			case OP_STATE_ft:
				{
					int         self = *pr->globals.self;
					int         nextthink = pr->fields.nextthink + self;
					int         frame = pr->fields.frame + self;
					int         think = pr->fields.think + self;
					float       time = *pr->globals.ftime + 0.1;
					PR_PTR (float, &pr->pr_edict_area[nextthink]) = time;
					PR_PTR (float, &pr->pr_edict_area[frame]) = OPA(float);
					PR_PTR (func, &pr->pr_edict_area[think]) = OPB(func);
				}
				break;
			OP_cmp_T (GE, U, long, lvec2, lvec4, >=, ulong, ulvec2, ulvec4);
			case OP_MEMSET_I:
				pr_memset (op_c, OPA(int), (pr_ushort_t) st->b);
				break;
			case OP_MEMSET_P:
				pr_memset (pr->pr_globals + OPC(ptr), OPA(int), OPB(uint));
				break;
			case OP_MEMSET_PI:
				pr_memset (pr->pr_globals + OPC(ptr), OPA(int),
						   (pr_ushort_t) st->b);
				break;
			case OP_STATE_ftt:
				{
					int         self = *pr->globals.self;
					int         nextthink = pr->fields.nextthink + self;
					int         frame = pr->fields.frame + self;
					int         think = pr->fields.think + self;
					float       time = *pr->globals.ftime + OPC(float);
					PR_PTR (float, &pr->pr_edict_area[nextthink]) = time;
					PR_PTR (float, &pr->pr_edict_area[frame]) = OPA(float);
					PR_PTR (func, &pr->pr_edict_area[think]) = OPB(func);
				}
				break;
			// 1 1110
			OP_cmp_T (LE, u, int, ivec2, ivec4, <=, uint, uvec2, uvec4);
			case OP_IFZ:
				if (!OPC(int)) {
					pr->pr_xstatement = pr_jump_mode (pr, st, 0);
					st = pr->pr_statements + pr->pr_xstatement;
				}
				break;
			case OP_IFB:
				if (OPC(int) < 0) {
					pr->pr_xstatement = pr_jump_mode (pr, st, 0);
					st = pr->pr_statements + pr->pr_xstatement;
				}
				break;
			case OP_IFA:
				if (OPC(int) > 0) {
					pr->pr_xstatement = pr_jump_mode (pr, st, 0);
					st = pr->pr_statements + pr->pr_xstatement;
				}
				break;
			case OP_STATE_dt:
				{
					int         self = *pr->globals.self;
					int         nextthink = pr->fields.nextthink + self;
					int         frame = pr->fields.frame + self;
					int         think = pr->fields.think + self;
					double      time = *pr->globals.dtime + 0.1;
					PR_PTR (double, &pr->pr_edict_area[nextthink]) = time;
					PR_PTR (int, &pr->pr_edict_area[frame]) = OPA(int);
					PR_PTR (func, &pr->pr_edict_area[think]) = OPB(func);
				}
				break;
			OP_cmp_T (LE, U, long, lvec2, lvec4, <=, ulong, ulvec2, ulvec4);
			case OP_IFNZ:
				if (OPC(int)) {
					pr->pr_xstatement = pr_jump_mode (pr, st, 0);
					st = pr->pr_statements + pr->pr_xstatement;
				}
				break;
			case OP_IFAE:
				if (OPC(int) >= 0) {
					pr->pr_xstatement = pr_jump_mode (pr, st, 0);
					st = pr->pr_statements + pr->pr_xstatement;
				}
				break;
			case OP_IFBE:
				if (OPC(int) <= 0) {
					pr->pr_xstatement = pr_jump_mode (pr, st, 0);
					st = pr->pr_statements + pr->pr_xstatement;
				}
				break;
			case OP_STATE_dtt:
				{
					int         self = *pr->globals.self;
					int         nextthink = pr->fields.nextthink + self;
					int         frame = pr->fields.frame + self;
					int         think = pr->fields.think + self;
					double      time = *pr->globals.dtime + OPC(double);
					PR_PTR (double, &pr->pr_edict_area[nextthink]) = time;
					PR_PTR (int, &pr->pr_edict_area[frame]) = OPA(int);
					PR_PTR (func, &pr->pr_edict_area[think]) = OPB(func);
				}
				break;
			// 1 1111
			case OP_LEA_A:
			case OP_LEA_B:
			case OP_LEA_C:
			case OP_LEA_D:
				mm = pr_address_mode (pr, st, (st_op - OP_LEA_A));
				OPC(ptr) = mm - pr->pr_globals;
				break;
			case OP_QV4MUL_F:
				OPC(vec4) = qvmulf (OPA(vec4), OPB(vec4));
				break;
			case OP_V4QMUL_F:
				OPC(vec4) = vqmulf (OPA(vec4), OPB(vec4));
				break;
			case OP_QV4MUL_D:
				OPC(dvec4) = qvmuld (OPA(dvec4), OPB(dvec4));
				break;
			case OP_V4QMUL_D:
				OPC(dvec4) = vqmuld (OPA(dvec4), OPB(dvec4));
				break;
			case OP_BITAND_F:
				OPC(float) = (int) OPA(float) & (int) OPB(float);
				break;
			case OP_BITOR_F:
				OPC(float) = (int) OPA(float) | (int) OPB(float);
				break;
			case OP_BITXOR_F:
				OPC(float) = (int) OPA(float) ^ (int) OPB(float);
				break;
			case OP_BITNOT_F:
				OPC(float) = ~ (int) OPA(float);
				break;
			case OP_CONV:
				switch ((pr_ushort_t) st->b) {
#include "libs/gamecode/pr_convert.cinc"
					default:
						PR_RunError (pr, "invalid conversion code: %04o",
									 (pr_ushort_t) st->b);
				}
				break;
			case OP_WITH:
				pr_with (pr, st);
				break;
			case OP_EXTEND:
				switch ((pr_ushort_t) st->b) {
#include "libs/gamecode/pr_extend.cinc"
					default:
						PR_RunError (pr, "invalid extend code: %04o",
									 (pr_ushort_t) st->b);
				}
				break;
#define OP_hop2(vec, op) ((vec)[0] op (vec)[1])
#define OP_hop3(vec, op) ((vec)[0] op (vec)[1] op (vec)[2])
#define OP_hop4(vec, op) ((vec)[0] op (vec)[1] op (vec)[2] op (vec)[3])
			case OP_HOPS:
				switch ((pr_ushort_t) st->b) {
#include "libs/gamecode/pr_hops.cinc"
					default:
						PR_RunError (pr, "invalid hops code: %04o",
									 (pr_ushort_t) st->b);
				}
				break;
			default:
				PR_RunError (pr, "Bad opcode x%03x", st->op & OP_MASK);
		}
		if (pr->watch && pr->watch->value != old_val.value) {
			if (!pr->wp_conditional
				|| pr->watch->value == pr->wp_val.value) {
				if (pr->debug_handler) {
					pr->debug_handler (prd_watchpoint, 0, pr->debug_data);
				} else {
					PR_RunError (pr, "watchpoint hit: %d -> %d",
								 old_val.value, pr->watch->value);
				}
			}
			old_val.value = pr->watch->value;
		}
	}
exit_program:;
}
/*
	PR_ExecuteProgram

	The interpretation main loop
*/
VISIBLE void
PR_ExecuteProgram (progs_t *pr, pr_func_t fnum)
{
	qfZoneScoped (true);
	Sys_PushSignalHook (signal_hook, pr);
	Sys_PushErrorHandler (error_handler, pr);

	if (pr->debug_handler) {
		pr->debug_handler (prd_subenter, &fnum, pr->debug_data);
	}

	int         exitdepth = pr->pr_depth;
	if (!PR_CallFunction (pr, fnum, pr->pr_return)) {
		// called a builtin instead of progs code
		goto exit_program;
	}
	if (pr->progs->version < PROG_VERSION) {
		pr_exec_quakec (pr, exitdepth);
	} else {
		pr_exec_ruamoko (pr, exitdepth);
	}
exit_program:
	if (pr->debug_handler) {
		pr->debug_handler (prd_subexit, 0, pr->debug_data);
	}
	pr->pr_argc = 0;
	Sys_PopErrorHandler ();
	Sys_PopSignalHook ();
}
