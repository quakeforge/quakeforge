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
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdarg.h>

#include "QF/cvar.h"
#include "QF/mathlib.h"
#include "QF/progs.h"
#include "QF/sys.h"
#include "QF/zone.h"

#include "compat.h"



void
PR_PrintStatement (progs_t * pr, dstatement_t *s)
{
	int			addr = s - pr->pr_statements;
	opcode_t   *op;

	if (pr_debug->int_val && pr->debug) {
		const char *source_line = PR_Get_Source_Line (pr, addr);

		if (source_line)
			Sys_Printf ("%s\n", source_line);
	}
	Sys_Printf ("%-7d ", addr);
	op = PR_Opcode (s->op);
	if (!op) {
		Sys_Printf ("unknown opcode %d\n", s->op);
		return;
	}
	Sys_Printf ("%-9s ", op->opname);

	if (s->op == OP_IF || s->op == OP_IFNOT || s->op == OP_IFBE
		|| s->op == OP_IFB || s->op == OP_IFAE || s->op == OP_IFA)
		Sys_Printf ("%sbranch %i (%i)",
					PR_GlobalString (pr, s->a, ev_integer), s->b, addr + s->b);
	else if (s->op == OP_GOTO) {
		Sys_Printf ("branch %i (%i)", s->a, addr + s->a);
	} else if (s->op == OP_RETURN || s->op == OP_DONE) {
		Sys_Printf ("%s", PR_GlobalString (pr, s->a, ev_void));
	} else {
		if (op->type_a != ev_void)
			Sys_Printf ("%s", PR_GlobalString (pr, s->a, op->type_a));
		if (op->type_b != ev_void) {
			if (op->type_c != ev_void)
				Sys_Printf ("%s", PR_GlobalString (pr, s->b, op->type_b));
			else
				Sys_Printf ("%s", PR_GlobalStringNoContents (pr, s->b));
		}
		if (op->type_c != ev_void)
			Sys_Printf ("%s", PR_GlobalStringNoContents (pr, s->c));
	}
	Sys_Printf ("\n");
}

void
PR_StackTrace (progs_t * pr)
{
	int				i;
	dfunction_t	   *f;

	if (pr->pr_depth == 0) {
		Sys_Printf ("<NO STACK>\n");
		return;
	}

	pr->pr_stack[pr->pr_depth].f = pr->pr_xfunction;
	for (i = pr->pr_depth; i >= 0; i--) {
		f = pr->pr_stack[i].f;

		if (!f) {
			Sys_Printf ("<NO FUNCTION>\n");
		} else
			Sys_Printf ("%12s : %s\n", PR_GetString (pr, f->s_file),
						PR_GetString (pr, f->s_name));
	}
}

void
PR_Profile (progs_t * pr)
{
	int				max, num, i;
	dfunction_t	   *best, *f;

	num = 0;
	do {
		max = 0;
		best = NULL;
		for (i = 0; i < pr->progs->numfunctions; i++) {
			f = &pr->pr_functions[i];
			if (f->profile > max) {
				max = f->profile;
				best = f;
			}
		}
		if (best) {
			if (num < 10)
				Sys_Printf ("%7i %s\n", best->profile,
							PR_GetString (pr, best->s_name));
			num++;
			best->profile = 0;
		}
	} while (best);
}

/*
	PR_RunError

	Aborts the currently executing function
*/
void
PR_RunError (progs_t * pr, const char *error, ...)
{
	char		string[1024];
	va_list		argptr;

	va_start (argptr, error);
	vsnprintf (string, sizeof (string), error, argptr);
	va_end (argptr);

	PR_DumpState (pr);

	Sys_Printf ("%s\n", string);

	// dump the stack so PR_Error can shutdown functions
	pr->pr_depth = 0;					

	PR_Error (pr, "Program error");
}

/*
	PR_EnterFunction

	Returns the new program statement counter
*/
int
PR_EnterFunction (progs_t * pr, dfunction_t *f)
{
	int			i, j, c, o;
	int			k;

	//Sys_Printf("%s:\n", PR_GetString(pr,f->s_name));
	pr->pr_stack[pr->pr_depth].s = pr->pr_xstatement;
	pr->pr_stack[pr->pr_depth].f = pr->pr_xfunction;
	pr->pr_depth++;
	if (pr->pr_depth >= MAX_STACK_DEPTH)
		PR_RunError (pr, "stack overflow");

	// save off any locals that the new function steps on
	c = f->locals;
	if (pr->localstack_used + c > LOCALSTACK_SIZE)
		PR_RunError (pr, "PR_ExecuteProgram: locals stack overflow\n");

	memcpy (&pr->localstack[pr->localstack_used],
			&pr->pr_globals[f->parm_start],
			sizeof (pr_type_t) * c);
	pr->localstack_used += c;

	if (pr_deadbeef_locals->int_val)
		for (k = f->parm_start; k < f->parm_start + c; k++)
			pr->pr_globals[k].integer_var = 0xdeadbeef;

	// copy parameters
	o = f->parm_start;
	for (i = 0; i < f->numparms; i++) {
		for (j = 0; j < f->parm_size[i]; j++) {
			memcpy (&pr->pr_globals[o],
					&pr->pr_globals[OFS_PARM0 + i * 3 + j],
					sizeof (pr_type_t));
			o++;
		}
	}

	pr->pr_xfunction = f;
	return f->first_statement - 1;		// offset the s++
}

int
PR_LeaveFunction (progs_t * pr)
{
	int			c;

	if (pr->pr_depth <= 0)
		PR_Error (pr, "prog stack underflow");

	// restore locals from the stack
	c = pr->pr_xfunction->locals;
	pr->localstack_used -= c;
	if (pr->localstack_used < 0)
		PR_RunError (pr, "PR_ExecuteProgram: locals stack underflow\n");

	memcpy (&pr->pr_globals[pr->pr_xfunction->parm_start],
			&pr->localstack[pr->localstack_used],
			sizeof (pr_type_t) * c);

	// up stack
	pr->pr_depth--;
	pr->pr_xfunction = pr->pr_stack[pr->pr_depth].f;
	return pr->pr_stack[pr->pr_depth].s;
}

#define OPA (pr->pr_globals[st->a])
#define OPB (pr->pr_globals[st->b])
#define OPC (pr->pr_globals[st->c])


/*
	PR_ExecuteProgram

	The interpretation main loop
*/
void
PR_ExecuteProgram (progs_t * pr, func_t fnum)
{
	int				exitdepth, profile, startprofile;
	int             pointer;
	dfunction_t	   *f, *newf;
	dstatement_t   *st;
	edict_t		   *ed;
	pr_type_t	   *ptr;

	if (!fnum || fnum >= pr->progs->numfunctions) {
		if (*pr->globals.self)
			ED_Print (pr, PROG_TO_EDICT (pr, *pr->globals.self));
		PR_RunError (pr, "PR_ExecuteProgram: NULL function");
	}

	f = &pr->pr_functions[fnum];
	//Sys_Printf("%s:\n", PR_GetString(pr,f->s_name));

	pr->pr_trace = false;

	// make a stack frame
	exitdepth = pr->pr_depth;

	st = &pr->pr_statements[PR_EnterFunction (pr, f)];
	startprofile = profile = 0;

	while (1) {
		st++;
		if (!pr->no_exec_limit
			&& ++profile > 1000000) {
			pr->pr_xstatement = st - pr->pr_statements;
			PR_RunError (pr, "runaway loop error");
		}

		if (pr->pr_trace)
			PR_PrintStatement (pr, st);

		switch (st->op) {
			case OP_ADD_F:
				OPC.float_var = OPA.float_var + OPB.float_var;
				break;
			case OP_ADD_V:
				VectorAdd (OPA.vector_var, OPB.vector_var, OPC.vector_var);
				break;
			case OP_ADD_S:
				{
					char *a = PR_GetString (pr, OPA.string_var);
					char *b = PR_GetString (pr, OPB.string_var);
					int lena = strlen (a);
					int size = lena + strlen (b) + 1;
					char *c = Hunk_TempAlloc (size);
					strcpy (c, a);
					strcpy (c + lena, b);
					OPC.string_var = PR_SetString (pr, c);
				}
				break;
			case OP_SUB_F:
				OPC.float_var = OPA.float_var - OPB.float_var;
				break;
			case OP_SUB_V:
				VectorSubtract (OPA.vector_var, OPB.vector_var, OPC.vector_var);
				break;
			case OP_MUL_F:
				OPC.float_var = OPA.float_var * OPB.float_var;
				break;
			case OP_MUL_V:
				OPC.float_var = DotProduct (OPA.vector_var, OPB.vector_var);
				break;
			case OP_MUL_FV:
				VectorScale (OPB.vector_var, OPA.float_var, OPC.vector_var);
				break;
			case OP_MUL_VF:
				VectorScale (OPA.vector_var, OPB.float_var, OPC.vector_var);
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
			case OP_GE:
				OPC.float_var = OPA.float_var >= OPB.float_var;
				break;
			case OP_LE:
				OPC.float_var = OPA.float_var <= OPB.float_var;
				break;
			case OP_GT:
				OPC.float_var = OPA.float_var > OPB.float_var;
				break;
			case OP_LT:
				OPC.float_var = OPA.float_var < OPB.float_var;
				break;
			case OP_AND:	// OPA and OPB have to be float for -0.0
				OPC.integer_var = OPA.float_var && OPB.float_var;
				break;
			case OP_OR:	// OPA and OPB have to be float for -0.0
				OPC.integer_var = OPA.float_var || OPB.float_var;
				break;
			case OP_NOT_F:
				OPC.integer_var = !OPA.float_var;
				break;
			case OP_NOT_V:
				OPC.integer_var = VectorIsZero (OPA.vector_var);
				break;
			case OP_NOT_S:
				OPC.integer_var = !OPA.string_var ||
					!*PR_GetString (pr, OPA.string_var);
				break;
			case OP_NOT_FNC:
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
			case OP_EQ_E:
				OPC.integer_var = OPA.integer_var == OPB.integer_var;
				break;
			case OP_EQ_FNC:
				OPC.integer_var = OPA.func_var == OPB.func_var;
				break;
			case OP_NE_F:
				OPC.integer_var = OPA.float_var != OPB.float_var;
				break;
			case OP_NE_V:
				OPC.integer_var = !VectorCompare (OPA.vector_var,
												  OPB.vector_var);
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
					}
					OPC.integer_var = cmp;
				}
				break;
			case OP_NE_E:
				OPC.integer_var = OPA.integer_var != OPB.integer_var;
				break;
			case OP_NE_FNC:
				OPC.integer_var = OPA.func_var != OPB.func_var;
				break;

			// ==================
			case OP_STORE_F:
			case OP_STORE_ENT:
			case OP_STORE_FLD:			// integers
			case OP_STORE_S:
			case OP_STORE_FNC:			// pointers
			case OP_STORE_I:
				OPB.integer_var = OPA.integer_var;
				break;
			case OP_STORE_V:
				VectorCopy (OPA.vector_var, OPB.vector_var);
				break;

			case OP_STOREP_F:
			case OP_STOREP_ENT:
			case OP_STOREP_FLD:		// integers
			case OP_STOREP_S:
			case OP_STOREP_FNC:		// pointers
			case OP_STOREP_I:
				//FIXME put bounds checking back
				ptr = pr->pr_globals + OPB.integer_var;
				ptr->integer_var = OPA.integer_var;
				break;
			case OP_STOREP_V:
				//FIXME put bounds checking back
				ptr = pr->pr_globals + OPB.integer_var;
				VectorCopy (OPA.vector_var, ptr->vector_var);
				break;

			case OP_ADDRESS:
				if (pr_boundscheck->int_val
					&& (OPA.entity_var < 0 || OPA.entity_var >=
						pr->pr_edictareasize)) {
					pr->pr_xstatement = st - pr->pr_statements;
					PR_RunError (pr, "Progs attempted to address an out of "
								 "bounds edict\n");
					return;
				}
				if (pr_boundscheck->int_val
					&& (OPA.entity_var == 0 && pr->null_bad)) {
					pr->pr_xstatement = st - pr->pr_statements;
					PR_RunError (pr, "assignment to world entity");
					return;
				}
				if (pr_boundscheck->int_val
					&& (OPB.integer_var < 0 || OPB.integer_var >=
						pr->progs->entityfields)) {
					pr->pr_xstatement = st - pr->pr_statements;
					PR_RunError (pr, "Progs attempted to address an invalid "
								 "field in an edict\n");
					return;
				}
				ed = PROG_TO_EDICT (pr, OPA.entity_var);
				OPC.integer_var = &ed->v[OPB.integer_var] - pr->pr_globals;
				break;
			case OP_ADDRESS_F:
			case OP_ADDRESS_V:
			case OP_ADDRESS_S:
			case OP_ADDRESS_ENT:
			case OP_ADDRESS_FLD:
			case OP_ADDRESS_FNC:
			case OP_ADDRESS_I:
			case OP_ADDRESS_P:
				OPC.integer_var = st->a;
				break;

			case OP_LOAD_F:
			case OP_LOAD_FLD:
			case OP_LOAD_ENT:
			case OP_LOAD_S:
			case OP_LOAD_FNC:
			case OP_LOAD_I:
				if (pr_boundscheck->int_val
					&& (OPA.entity_var < 0 || OPA.entity_var >=
						pr->pr_edictareasize)) {
					pr->pr_xstatement = st - pr->pr_statements;
					PR_RunError (pr, "Progs attempted to read an out of "
								 "bounds edict number\n");
					return;
				}
				if (pr_boundscheck->int_val
					&& (OPB.integer_var < 0 || OPB.integer_var >=
						pr->progs->entityfields)) {
					pr->pr_xstatement = st - pr->pr_statements;
					PR_RunError (pr, "Progs attempted to read an invalid "
								 "field in an edict\n");
					return;
				}
				ed = PROG_TO_EDICT (pr, OPA.entity_var);
				OPC.integer_var = ed->v[OPB.integer_var].integer_var;
				break;
			case OP_LOAD_V:
				if (pr_boundscheck->int_val
					&& (OPA.entity_var < 0 || OPA.entity_var >=
						pr->pr_edictareasize)) {
					pr->pr_xstatement = st - pr->pr_statements;
					PR_RunError (pr, "Progs attempted to read an out of "
								 "bounds edict number\n");
					return;
				}
				if (pr_boundscheck->int_val
					&& (OPB.integer_var < 0
						|| OPB.integer_var + 2 >= pr->progs->entityfields)) {
					pr->pr_xstatement = st - pr->pr_statements;
					PR_RunError (pr, "Progs attempted to read an invalid "
								 "field in an edict\n");
					return;
				}
				ed = PROG_TO_EDICT (pr, OPA.entity_var);
				memcpy (&OPC, &ed->v[OPB.integer_var], 3 * sizeof (OPC));
				break;

			case OP_LOADB_F:
			case OP_LOADB_S:
			case OP_LOADB_ENT:
			case OP_LOADB_FLD:
			case OP_LOADB_FNC:
			case OP_LOADB_I:
			case OP_LOADB_P:
				//FIXME put bounds checking in
				pointer = OPA.integer_var + OPB.integer_var;
				ptr = pr->pr_globals + pointer;
				OPC.integer_var = ptr->integer_var;
				break;
			case OP_LOADB_V:
				//FIXME put bounds checking in
				pointer = OPA.integer_var + OPB.integer_var;
				ptr = pr->pr_globals + pointer;
				VectorCopy (ptr->vector_var, OPC.vector_var);
				break;

			case OP_LEA:
				pointer = OPA.integer_var + OPB.integer_var;
				OPC.integer_var = pointer;
				break;

			case OP_STOREB_F:
			case OP_STOREB_S:
			case OP_STOREB_ENT:
			case OP_STOREB_FLD:
			case OP_STOREB_FNC:
			case OP_STOREB_I:
			case OP_STOREB_P:
				//FIXME put bounds checking in
				pointer = OPB.integer_var + OPC.integer_var;
				ptr = pr->pr_globals + pointer;
				ptr->integer_var = OPA.integer_var;
				break;
			case OP_STOREB_V:
				//FIXME put bounds checking in
				pointer = OPB.integer_var + OPC.integer_var;
				ptr = pr->pr_globals + pointer;
				VectorCopy (OPA.vector_var, ptr->vector_var);
				break;

			// ==================
			case OP_IFNOT:
				if (!OPA.integer_var)
					st += (short)st->b - 1;		// offset the s++
				break;
			case OP_IF:
				if (OPA.integer_var)
					st += (short)st->b - 1;		// offset the s++
				break;
			case OP_IFBE:
				if (OPA.integer_var <= 0)
					st += (short)st->b - 1;		// offset the s++
				break;
			case OP_IFB:
				if (OPA.integer_var < 0)
					st += (short)st->b - 1;		// offset the s++
				break;
			case OP_IFAE:
				if (OPA.integer_var >= 0)
					st += (short)st->b - 1;		// offset the s++
				break;
			case OP_IFA:
				if (OPA.integer_var > 0)
					st += (short)st->b - 1;		// offset the s++
				break;
			case OP_GOTO:
				st += (short)st->a - 1;			// offset the s++
				break;
			case OP_JUMP:
				if (pr_boundscheck->int_val
					&& (OPA.uinteger_var >= pr->progs->numstatements)) {
					pr->pr_xstatement = st - pr->pr_statements;
					PR_RunError (pr, "Invalid jump destination\n");
					return;
				}
				st = &pr->pr_statements[OPA.uinteger_var];
				break;
			case OP_JUMPB:
				//FIXME put bounds checking in
				pointer = OPA.integer_var + OPB.integer_var;
				ptr = pr->pr_globals + pointer;
				pointer = ptr->integer_var;
				if (pr_boundscheck->int_val
					&& (pointer >= pr->progs->numstatements)) {
					pr->pr_xstatement = st - pr->pr_statements;
					PR_RunError (pr, "Invalid jump destination\n");
					return;
				}
				st = &pr->pr_statements[pointer];
				break;

			case OP_CALL0:
			case OP_CALL1:
			case OP_CALL2:
			case OP_CALL3:
			case OP_CALL4:
			case OP_CALL5:
			case OP_CALL6:
			case OP_CALL7:
			case OP_CALL8:
				pr->pr_xfunction->profile += profile - startprofile;
				startprofile = profile;
				pr->pr_xstatement = st - pr->pr_statements;
				pr->pr_argc = st->op - OP_CALL0;
				if (!OPA.func_var)
					PR_RunError (pr, "NULL function");
				if (OPA.func_var < 0) { // dynamic builtin
					int i = -OPA.func_var;
					if (i >= pr->numbuiltins || !pr->builtins[i].proc)
						PR_RunError (pr, "Bad builtin call number");
					pr->builtins[i].proc (pr);
					break;
				}
				newf = &pr->pr_functions[OPA.func_var];
				if (newf->first_statement < 0) {
					// negative statements are built in functions
					int         i = -newf->first_statement;

					if (i >= pr->numbuiltins || !pr->builtins[i].proc)
						PR_RunError (pr, "Bad builtin call number");
					pr->builtins[i].proc (pr);
					break;
				}

				st = &pr->pr_statements[PR_EnterFunction (pr, newf)];
				break;
			case OP_DONE:
			case OP_RETURN:
				memcpy (&pr->pr_globals[OFS_RETURN], &OPA, 3 * sizeof (OPA));
				st = &pr->pr_statements[PR_LeaveFunction (pr)];
				if (pr->pr_depth == exitdepth)
					return;					// all done
				break;
			case OP_STATE:
				ed = PROG_TO_EDICT (pr, *pr->globals.self);
				ed->v[pr->fields.nextthink].float_var = *pr->globals.time +
					0.1;
				ed->v[pr->fields.frame].float_var = OPA.float_var;
				ed->v[pr->fields.think].func_var = OPB.func_var;
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
				OPC.integer_var = OPA.integer_var >= OPB.integer_var;
				break;
			case OP_LE_I:
				OPC.integer_var = OPA.integer_var <= OPB.integer_var;
				break;
			case OP_GT_I:
				OPC.integer_var = OPA.integer_var > OPB.integer_var;
				break;
			case OP_LT_I:
				OPC.integer_var = OPA.uinteger_var < OPB.uinteger_var;
				break;
			case OP_GE_UI:
				OPC.integer_var = OPA.uinteger_var >= OPB.uinteger_var;
				break;
			case OP_LE_UI:
				OPC.integer_var = OPA.uinteger_var <= OPB.uinteger_var;
				break;
			case OP_GT_UI:
				OPC.integer_var = OPA.uinteger_var > OPB.uinteger_var;
				break;
			case OP_LT_UI:
				OPC.integer_var = OPA.integer_var < OPB.integer_var;
				break;

			case OP_AND_I:
				OPC.integer_var = OPA.integer_var && OPB.integer_var;
				break;
			case OP_OR_I:
				OPC.integer_var = OPA.integer_var || OPB.integer_var;
				break;
			case OP_NOT_I:
				OPC.integer_var = !OPA.integer_var;
				break;
			case OP_EQ_I:
				OPC.integer_var = OPA.integer_var == OPB.integer_var;
				break;
			case OP_NE_I:
				OPC.integer_var = OPA.integer_var != OPB.integer_var;
				break;

// LordHavoc: to be enabled when Progs version 7 (or whatever it will be numbered) is finalized
/*
			case OP_BOUNDCHECK:
				if (OPA.integer_var < 0 || OPA.integer_var >= st->b) {
					pr->pr_xstatement = st - pr->pr_statements;
					PR_RunError (pr, "Progs boundcheck failed at line number "
					"%d, value is < 0 or >= %d\n", st->b, st->c);
					return;
				}
				break;

*/
			default:
				pr->pr_xstatement = st - pr->pr_statements;
				PR_RunError (pr, "Bad opcode %i", st->op);
		}
	}
}
