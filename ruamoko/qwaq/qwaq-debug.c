/*
	qwaq-debug.c

	Debugging support

	Copyright (C) 2020 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2020/03/24

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

#include <sys/ioctl.h>
#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/keys.h"
#include "QF/sys.h"

#include "qwaq.h"
#include "event.h"
#include "qwaq-curses.h"
#include "qwaq-debug.h"

typedef struct qwaq_target_s {
	progs_t    *pr;
	struct qwaq_debug_s *debugger;
	int         handle;
	prdebug_t   event;
	rwcond_t    run_cond;
} qwaq_target_t;

typedef struct qwaq_debug_s {
	progs_t    *pr;
	qwaq_resources_t *qwaq;	// to communicate with the debugger thread
	PR_RESMAP (qwaq_target_t) targets;
} qwaq_debug_t;

#define always_inline inline __attribute__((__always_inline__))

static qwaq_target_t *
target_new (qwaq_debug_t *debug)
{
	PR_RESNEW (qwaq_target_t, debug->targets);
}

static void
target_free (qwaq_debug_t *debug, qwaq_target_t *target)
{
	PR_RESFREE (qwaq_target_t, debug->targets, target);
}

static void
target_reset (qwaq_debug_t *debug)
{
	PR_RESRESET (qwaq_target_t, debug->targets);
}

static inline qwaq_target_t *
target_get (qwaq_debug_t *debug, unsigned index)
{
	PR_RESGET (debug->targets, index);
}

static inline int
target_index (qwaq_debug_t *debug, qwaq_target_t *target)
{
	PR_RESINDEX (debug->targets, target);
}

static always_inline qwaq_target_t * __attribute__((pure))
get_target (qwaq_debug_t *debug, const char *name, int handle)
{
	qwaq_target_t *target = target_get (debug, handle);

	if (!target || !target->debugger) {
		PR_RunError (debug->pr, "invalid target passed to %s", name + 4);
	}
	return target;
}

static void
qwaq_debug_handler (prdebug_t debug_event, void *data)
{
	__auto_type target = (qwaq_target_t *) data;
	qwaq_debug_t *debug = target->debugger;
	qwaq_event_t event = {};
	int         ret;

	target->event = debug_event;
	event.what = qe_debug_event;
	event.message.pointer_val = target->handle;

	while ((ret = qwaq_add_event (debug->qwaq, &event)) == ETIMEDOUT) {
		// spin
	}
	if (ret == EINVAL) {
		Sys_Error ("event queue broke");
	}
	pthread_mutex_lock (&target->run_cond.mut);
	pthread_cond_wait (&target->run_cond.rcond, &target->run_cond.mut);
	pthread_mutex_unlock (&target->run_cond.mut);
	if (debug_event == prd_runerror || debug_event == prd_error) {
		pthread_exit ((void *) target->pr->error_string);
	}
}

static void
qwaq_debug_clear (progs_t *pr, void *data)
{
	__auto_type debug = (qwaq_debug_t *) data;
	target_reset (debug);
}

static void
qwaq_target_clear (progs_t *pr, void *data)
{
	qwaq_target_t *target = pr->debug_data;
	if (target) {
		target_free (target->debugger, target);
	}
}

//FIXME need a better way to get this from one thread to the others
static qwaq_debug_t *qwaq_debug_data;

static int
qwaq_target_load (progs_t *pr)
{
	qwaq_target_t *target = target_new (qwaq_debug_data);
	target->pr = pr;
	target->debugger = qwaq_debug_data;
	target->handle = target_index (qwaq_debug_data, target);
	qwaq_init_cond (&target->run_cond);

	pr->debug_handler = qwaq_debug_handler;
	pr->debug_data = target;

	// start tracing immediately so the debugger has a chance to start up
	// before the target progs begin running
	pr->pr_trace = 1;
	pr->pr_trace_depth = -1;

	return 1;
}

static void
qdb_set_trace (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pointer_t   handle = P_INT (pr, 0);
	int         state = P_INT (pr, 1);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;

	tpr->pr_trace = state;
}

static void
qdb_set_breakpoint (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pointer_t   handle = P_INT (pr, 0);
	unsigned    staddr = P_INT (pr, 1);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;

	if (staddr >= tpr->progs->numstatements) {
		R_INT (pr) = -1;
		return;
	}
	tpr->pr_statements[staddr].op |= OP_BREAK;
	R_INT (pr) = 0;
}

static void
qdb_clear_breakpoint (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pointer_t   handle = P_INT (pr, 0);
	unsigned    staddr = P_UINT (pr, 1);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;

	if (staddr >= tpr->progs->numstatements) {
		R_INT (pr) = -1;
		return;
	}
	tpr->pr_statements[staddr].op &= ~OP_BREAK;
	R_INT (pr) = 0;
}

static void
qdb_set_watchpoint (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pointer_t   handle = P_INT (pr, 0);
	pointer_t   offset = P_UINT (pr, 1);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;

	if (offset >= tpr->globals_size) {
		R_INT (pr) = -1;
		return;
	}
	tpr->watch = &tpr->pr_globals[offset];
	R_INT (pr) = 0;
}

static void
qdb_clear_watchpoint (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pointer_t   handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;

	tpr->watch = 0;
	R_INT (pr) = 0;
}

static void
qdb_continue (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pointer_t   handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);

	pthread_mutex_lock (&target->run_cond.mut);
	pthread_cond_signal (&target->run_cond.rcond);
	pthread_mutex_unlock (&target->run_cond.mut);
}

static void
qdb_get_state (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pointer_t   handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;
	pr_lineno_t *lineno;
	pr_auxfunction_t *f;
	string_t    file = 0;
	unsigned    line = 0;
	unsigned    staddr = tpr->pr_xstatement;
	func_t      func = tpr->pr_xfunction - tpr->function_table;

	lineno = PR_Find_Lineno (tpr, staddr);
	if (lineno) {
		f = PR_Get_Lineno_Func (tpr, lineno);
		//FIXME file is a permanent string. dynamic would be better
		//but they're not merged (and would need refcounting)
		file = PR_SetString (pr, PR_Get_Source_File (tpr, lineno));
		func = f->function;
		line = PR_Get_Lineno_Line (tpr, lineno);
		line += f->source_line;
	}

	qdb_state_t state = {};
	state.staddr = staddr;
	state.func = func;
	state.file = file;
	state.line = line;

	R_PACKED (pr, qdb_state_t) = state;
}

static void
qdb_get_data (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pointer_t   handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;
	pointer_t   srcoff = P_POINTER (pr, 1);
	unsigned    length = P_UINT (pr, 2);
	pointer_t   dstoff = P_POINTER(pr, 3);

	pr_type_t  *src = PR_GetPointer(tpr, srcoff);
	pr_type_t  *dst = PR_GetPointer(pr, dstoff);

	if (srcoff >= tpr->globals_size || srcoff + length >= tpr->globals_size) {
		R_INT (pr) = -1;
		return;
	}
	if (dstoff >= pr->globals_size || dstoff + length >= pr->globals_size) {
		R_INT (pr) = -1;
		return;
	}
	memcpy (dst, src, length * sizeof (pr_type_t));
	R_INT (pr) = 0;
}

static void
qdb_find_global (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pointer_t   handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;
	const char *name = P_GSTRING (pr, 1);
	pr_def_t   *def = PR_FindGlobal (tpr, name);

	if (def) {
		R_PACKED (pr, qdb_def_t).type_size = (def->size << 16) | def->type;
		R_PACKED (pr, qdb_def_t).offset = def->ofs;
		R_PACKED (pr, qdb_def_t).name = def->name;
		R_PACKED (pr, qdb_def_t).type_encoding = def->type_encoding;
	} else {
		memset (&R_PACKED (pr, qdb_def_t), 0, sizeof (qdb_def_t));
	}
}

static void
qdb_find_field (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pointer_t   handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;
	const char *name = P_GSTRING (pr, 1);
	pr_def_t   *def = PR_FindField (tpr, name);

	if (def) {
		R_PACKED (pr, qdb_def_t).type_size = (def->size << 16) | def->type;
		R_PACKED (pr, qdb_def_t).offset = def->ofs;
		R_PACKED (pr, qdb_def_t).name = def->name;
		R_PACKED (pr, qdb_def_t).type_encoding = def->type_encoding;
	} else {
		memset (&R_PACKED (pr, qdb_def_t), 0, sizeof (qdb_def_t));
	}
}

static void
qdb_find_function (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pointer_t   handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;
	const char *name = P_GSTRING (pr, 1);
	dfunction_t *func = PR_FindFunction (tpr, name);

	R_INT (pr) = 0;
	if (func) {
		__auto_type f
			= (qdb_function_t *) PR_Zone_Malloc (pr, sizeof (qdb_function_t));
		f->staddr = func->first_statement;
		f->local_data = func->parm_start;
		f->local_size = func->locals;
		f->profile = func->profile;
		f->name = func->s_name;
		f->file = func->s_file;
		f->num_params = func->numparms;
		R_INT (pr) = PR_SetPointer (pr, f);
	}
}

static void
qdb_find_auxfunction (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pointer_t   handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;
	const char *name = P_GSTRING (pr, 1);
	dfunction_t *func = PR_FindFunction (tpr, name);
	size_t      fnum = func - tpr->pr_functions;
	pr_auxfunction_t *aux_func = PR_Debug_MappedAuxFunction (tpr, fnum);

	R_INT (pr) = 0;
	if (aux_func) {
		__auto_type f
			= (qdb_auxfunction_t *) PR_Zone_Malloc (pr,
												sizeof (qdb_auxfunction_t));
		f->function = aux_func->function;
		f->source_line = aux_func->source_line;
		f->line_info = aux_func->line_info;
		f->local_defs = aux_func->local_defs;
		f->num_locals = aux_func->num_locals;
		f->return_type = aux_func->return_type;
		R_INT (pr) = PR_SetPointer (pr, f);
	}
}

static void
qdb_get_local_defs (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pointer_t   handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;
	size_t      fnum = P_UINT (pr, 1);
	pr_auxfunction_t *auxfunc = PR_Debug_MappedAuxFunction (tpr, fnum);

	R_INT (pr) = 0;
	if (auxfunc) {
		pr_def_t   *defs = PR_Debug_LocalDefs (tpr, auxfunc);
		__auto_type qdefs
			= (qdb_def_t *) PR_Zone_Malloc (pr,
								auxfunc->num_locals * sizeof (qdb_def_t));
		for (unsigned i = 0; i < auxfunc->num_locals; i++) {
			qdefs[i].type_size = (defs[i].size << 16) | defs[i].type;
			qdefs[i].offset = defs[i].ofs;
			qdefs[i].name = defs[i].name;
			qdefs[i].type_encoding = defs[i].type_encoding;
		}
		R_INT (pr) = PR_SetPointer (pr, qdefs);
	}
}

static builtin_t builtins[] = {
	{"qdb_set_trace",			qdb_set_trace,			-1},
	{"qdb_set_breakpoint",		qdb_set_breakpoint,		-1},
	{"qdb_clear_breakpoint",	qdb_clear_breakpoint,	-1},
	{"qdb_set_watchpoint",		qdb_set_watchpoint,		-1},
	{"qdb_clear_watchpoint",	qdb_clear_watchpoint,	-1},
	{"qdb_continue",			qdb_continue,			-1},
	{"qdb_get_state",			qdb_get_state,			-1},
	{"qdb_get_data",			qdb_get_data,			-1},
	{"qdb_find_global",			qdb_find_global,		-1},
	{"qdb_find_field",			qdb_find_field,			-1},
	{"qdb_find_function",		qdb_find_function,		-1},
	{"qdb_find_auxfunction",	qdb_find_auxfunction,	-1},
	{"qdb_get_local_defs",		qdb_get_local_defs,		-1},
	{}
};

void
QWAQ_Debug_Init (progs_t *pr)
{
	qwaq_debug_t *debug = calloc (sizeof (*debug), 1);
	qwaq_debug_data = debug;	// FIXME ? see decl
	debug->pr = pr;
	debug->qwaq = PR_Resources_Find (pr, "qwaq");

	PR_Resources_Register (pr, "qwaq-debug", debug, qwaq_debug_clear);
	PR_RegisterBuiltins (pr, builtins);
}

void
QWAQ_DebugTarget_Init (progs_t *pr)
{
	PR_AddLoadFunc (pr, qwaq_target_load);
	PR_Resources_Register (pr, "qwaq-target", 0, qwaq_target_clear);
}
