/*
	debug.c

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

#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/keys.h"
#include "QF/quakefs.h"
#include "QF/sys.h"

#include "ruamoko/qwaq/qwaq.h"
#include "ruamoko/qwaq/ui/event.h"
#include "ruamoko/qwaq/ui/curses.h"
#include "ruamoko/qwaq/debugger/debug.h"

typedef struct qwaq_target_s {
	progs_t    *pr;
	struct qwaq_debug_s *debugger;
	int         handle;
	prdebug_t   event;
	void       *param;
	rwcond_t    run_cond;
	int         run_command;
} qwaq_target_t;

typedef struct qwaq_debug_s {
	progs_t    *pr;
	qwaq_input_resources_t *input;	// to communicate with the debugger thread
	PR_RESMAP (qwaq_target_t) targets;
} qwaq_debug_t;

#define always_inline inline __attribute__((__always_inline__))

static qwaq_target_t *
target_new (qwaq_debug_t *debug)
{
	return PR_RESNEW (debug->targets);
}

static void
target_free (qwaq_debug_t *debug, qwaq_target_t *target)
{
	PR_RESFREE (debug->targets, target);
}

static void
target_reset (qwaq_debug_t *debug)
{
	PR_RESRESET (debug->targets);
}

static inline qwaq_target_t *
target_get (qwaq_debug_t *debug, unsigned index)
{
	return PR_RESGET (debug->targets, index);
}

static inline int __attribute__((pure))
target_index (qwaq_debug_t *debug, qwaq_target_t *target)
{
	return PR_RESINDEX (debug->targets, target);
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
qwaq_debug_handler (prdebug_t debug_event, void *param, void *data)
{
	__auto_type target = (qwaq_target_t *) data;
	qwaq_debug_t *debug = target->debugger;
	qwaq_event_t event = {};
	int         ret;

	target->event = debug_event;
	target->param = param;
	event.what = qe_debug_event;
	event.message.pointer_val = target->handle;

	while ((ret = qwaq_add_event (debug->input, &event)) == ETIMEDOUT) {
		// spin
	}
	if (ret == EINVAL) {
		Sys_Error ("event queue broke");
	}
	pthread_mutex_lock (&target->run_cond.mut);
	while (!target->run_command) {
		pthread_cond_wait (&target->run_cond.rcond, &target->run_cond.mut);
	}
	target->run_command = 0;
	pthread_mutex_unlock (&target->run_cond.mut);
	if (debug_event == prd_runerror || debug_event == prd_error) {
		pthread_exit (param);
	}
}

//FIXME need a better way to get this from one thread to the others
pthread_cond_t debug_data_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t debug_data_mutex = PTHREAD_MUTEX_INITIALIZER;
static qwaq_debug_t *qwaq_debug_data;

static int
qwaq_debug_load (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");

	pthread_mutex_lock (&debug_data_mutex);
	qwaq_debug_data = debug;	// FIXME ? see decl
	pthread_cond_broadcast (&debug_data_cond);
	pthread_mutex_unlock (&debug_data_mutex);

	return 1;
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

static int
qwaq_target_load (progs_t *pr)
{
	pthread_mutex_lock (&debug_data_mutex);
	while (!qwaq_debug_data) {
		pthread_cond_wait (&debug_data_cond, &debug_data_mutex);
	}
	pthread_mutex_unlock (&debug_data_mutex);

	qwaq_target_t *target = target_new (qwaq_debug_data);
	target->pr = pr;
	target->debugger = qwaq_debug_data;
	target->handle = target_index (qwaq_debug_data, target);
	qwaq_init_cond (&target->run_cond);
	target->run_command = 0;

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
	pr_ptr_t    handle = P_INT (pr, 0);
	int         state = P_INT (pr, 1);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;

	tpr->pr_trace = state;
}

static void
qdb_set_breakpoint (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pr_ptr_t    handle = P_INT (pr, 0);
	unsigned    staddr = P_INT (pr, 1);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;

	if (staddr >= tpr->progs->statements.count) {
		R_INT (pr) = -1;
		return;
	}
	int         set = (tpr->pr_statements[staddr].op & OP_BREAK) != 0;
	tpr->pr_statements[staddr].op |= OP_BREAK;
	R_INT (pr) = set;
}

static void
qdb_clear_breakpoint (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pr_ptr_t    handle = P_INT (pr, 0);
	unsigned    staddr = P_UINT (pr, 1);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;

	if (staddr >= tpr->progs->statements.count) {
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
	pr_ptr_t    handle = P_INT (pr, 0);
	pr_ptr_t    offset = P_UINT (pr, 1);
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
	pr_ptr_t    handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;

	tpr->watch = 0;
	R_INT (pr) = 0;
}

static void
qdb_continue (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pr_ptr_t    handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);

	pthread_mutex_lock (&target->run_cond.mut);
	target->run_command = 1;
	pthread_cond_signal (&target->run_cond.rcond);
	pthread_mutex_unlock (&target->run_cond.mut);
}

static void
qdb_get_state (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pr_ptr_t    handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;
	pr_lineno_t *lineno;
	pr_auxfunction_t *f;
	pr_string_t file = 0;
	unsigned    line = 0;
	unsigned    staddr = tpr->pr_xstatement;
	pr_func_t   func = 0;

	if (tpr->pr_xfunction) {
		func = tpr->pr_xfunction - tpr->function_table;
	}

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
qdb_get_stack_depth (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pr_ptr_t    handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;

	R_INT (pr) = tpr->pr_depth;
}

static void
qdb_get_stack (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pr_ptr_t    handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;
	int         count = tpr->pr_depth;

	R_POINTER (pr) = 0;
	if (count > 0) {
		size_t      size = count * sizeof (qdb_stack_t);
		pr_string_t stack_block = PR_AllocTempBlock (pr, size);
		__auto_type stack = (qdb_stack_t *) PR_GetString (pr, stack_block);

		for (int i = 0; i < count; i++) {
			stack[i].staddr = tpr->pr_stack[i].staddr;
			stack[i].func = tpr->pr_stack[i].func - tpr->pr_xfunction;
			//XXX temp strings (need access somehow)
		}
		R_POINTER (pr) = PR_SetPointer (pr, stack);
	}
}

static void
qdb_get_event (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pr_ptr_t    handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	__auto_type event = &P_STRUCT (pr, qdb_event_t, 1);

	memset (event, 0, sizeof (*event));
	event->what = target->event;
	switch (event->what) {
		case prd_subenter:
			event->function = *(pr_func_t *) target->param;
			break;
		case prd_runerror:
		case prd_error:
			event->message = PR_SetReturnString (pr, (char *) target->param);
			break;
		case prd_begin:
			event->function = *(pr_func_t *) target->param;
			break;
		case prd_terminate:
			event->exit_code = *(int *) target->param;
			break;
		case prd_trace:
		case prd_breakpoint:
		case prd_watchpoint:
		case prd_subexit:
		case prd_none:
			break;
	}
	R_INT (pr) = target->event != prd_none;
}

static void
qdb_get_data (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pr_ptr_t    handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;
	pr_ptr_t    srcoff = P_POINTER (pr, 1);
	unsigned    length = P_UINT (pr, 2);
	pr_ptr_t    dstoff = P_POINTER(pr, 3);

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
qdb_get_string (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pr_ptr_t    handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;
	pr_string_t string = P_STRING (pr, 1);

	R_STRING (pr) = 0;
	if (PR_StringValid (tpr, string)) {
		RETURN_STRING (pr, PR_GetString (tpr, string));
	}
}

static void
qdb_get_file_path (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pr_ptr_t    handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;
	const char *file = P_GSTRING (pr, 1);
	const char *basedir = PR_Debug_GetBaseDirectory (tpr, file);

	if (basedir) {
		size_t      baselen = strlen (basedir);
		size_t      filelen = strlen (file);
		char       *path = alloca (baselen + filelen + 2);
		strcpy (path, basedir);
		path[baselen] = '/';
		strcpy (path + baselen + 1, file);
		path = QFS_CompressPath (path);
		RETURN_STRING (pr, path);
		free (path);
	} else {
		R_STRING (pr) = P_STRING (pr, 1);
	}
}

static void
qdb_find_string (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pr_ptr_t    handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;
	const char *str = P_GSTRING (pr, 1);

	R_INT (pr) = PR_FindString (tpr, str);
}

static void
qdb_find_global (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pr_ptr_t    handle = P_INT (pr, 0);
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
	pr_ptr_t    handle = P_INT (pr, 0);
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
return_function (progs_t *pr, dfunction_t *func)
{
	R_POINTER (pr) = 0;
	if (func) {
		__auto_type f
			= (qdb_function_t *) PR_Zone_Malloc (pr, sizeof (qdb_function_t));
		f->staddr = func->first_statement;
		f->local_data = func->parm_start;
		f->local_size = func->locals;
		f->profile = func->profile;
		f->name = func->name;
		f->file = func->file;
		f->num_params = func->numparms;
		R_POINTER (pr) = PR_SetPointer (pr, f);
	}
}

static void
qdb_find_function (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pr_ptr_t    handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;
	const char *name = P_GSTRING (pr, 1);
	dfunction_t *func = PR_FindFunction (tpr, name);

	return_function (pr, func);
}

static void
qdb_get_function (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pr_ptr_t    handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;
	pr_uint_t   fnum = P_INT (pr, 1);
	dfunction_t *func = tpr->pr_functions + fnum;

	if (fnum >= tpr->progs->functions.count) {
		func = 0;
	}
	return_function (pr, func);
}

static void
return_auxfunction (progs_t *pr, pr_auxfunction_t *auxfunc)
{
	R_POINTER (pr) = 0;
	if (auxfunc) {
		__auto_type f
			= (qdb_auxfunction_t *) PR_Zone_Malloc (pr,
												sizeof (qdb_auxfunction_t));
		f->function = auxfunc->function;
		f->source_line = auxfunc->source_line;
		f->line_info = auxfunc->line_info;
		f->local_defs = auxfunc->local_defs;
		f->num_locals = auxfunc->num_locals;
		f->return_type = auxfunc->return_type;
		R_POINTER (pr) = PR_SetPointer (pr, f);
	}
}

static void
qdb_find_auxfunction (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pr_ptr_t    handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;
	const char *name = P_GSTRING (pr, 1);
	dfunction_t *func = PR_FindFunction (tpr, name);
	pr_uint_t   fnum = func - tpr->pr_functions;
	pr_auxfunction_t *auxfunc = PR_Debug_MappedAuxFunction (tpr, fnum);

	if (fnum >= tpr->progs->functions.count) {
		func = 0;
	}
	return_auxfunction (pr, auxfunc);
}

static void
qdb_get_auxfunction (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pr_ptr_t    handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;
	pr_uint_t   fnum = P_UINT (pr, 1);
	pr_auxfunction_t *auxfunc = PR_Debug_MappedAuxFunction (tpr, fnum);

	return_auxfunction (pr, auxfunc);
}

static void
qdb_get_local_defs (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pr_ptr_t    handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;
	pr_uint_t   fnum = P_UINT (pr, 1);
	pr_auxfunction_t *auxfunc = PR_Debug_MappedAuxFunction (tpr, fnum);

	R_POINTER (pr) = 0;
	if (auxfunc && auxfunc->num_locals) {
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
		R_POINTER (pr) = PR_SetPointer (pr, qdefs);
	}
}

static void
qdb_get_source_line_addr (progs_t *pr)
{
	__auto_type debug = PR_Resources_Find (pr, "qwaq-debug");
	pr_ptr_t    handle = P_INT (pr, 0);
	qwaq_target_t *target = get_target (debug, __FUNCTION__, handle);
	progs_t    *tpr = target->pr;
	const char *file = P_GSTRING (pr, 1);
	pr_uint_t   line = P_UINT (pr, 2);

	R_UINT (pr) = PR_FindSourceLineAddr (tpr, file, line);
}

#define bi(x,np,params...) {#x, x, -1, np, {params}}
#define p(type) PR_PARAM(type)
static builtin_t builtins[] = {
	bi(qdb_set_trace,            2, p(int), p(int)),
	bi(qdb_set_breakpoint,       2, p(int), p(uint)),
	bi(qdb_clear_breakpoint,     2, p(int), p(uint)),
	bi(qdb_set_watchpoint,       2, p(int), p(uint)),
	bi(qdb_clear_watchpoint,     1, p(int)),
	bi(qdb_continue,             1, p(int)),
	bi(qdb_get_state,            1, p(int)),
	bi(qdb_get_stack_depth,      1, p(int)),
	bi(qdb_get_stack,            1, p(int)),
	bi(qdb_get_event,            2, p(int), p(ptr)),
	bi(qdb_get_data,             4, p(int), p(uint), p(uint), p(ptr)),
	{"qdb_get_string|{tag qdb_target_s=}I",	qdb_get_string,	-1, 1, {p(uint)}},
	{"qdb_get_string|{tag qdb_target_s=}*",	qdb_get_string,	-1, 1, {p(string)}},
	bi(qdb_get_file_path,        2, p(int), p(string)),
	bi(qdb_find_string,          2, p(int), p(string)),
	bi(qdb_find_global,          2, p(int), p(string)),
	bi(qdb_find_field,           2, p(int), p(string)),
	bi(qdb_find_function,        2, p(int), p(string)),
	bi(qdb_get_function,         2, p(int), p(uint)),
	bi(qdb_find_auxfunction,     2, p(int), p(string)),
	bi(qdb_get_auxfunction,      2, p(int), p(uint)),
	bi(qdb_get_local_defs,       2, p(int), p(uint)),
	bi(qdb_get_source_line_addr, 3, p(int), p(string), p(uint)),
	{}
};

void
QWAQ_Debug_Init (progs_t *pr)
{
	qwaq_debug_t *debug = calloc (sizeof (*debug), 1);

	debug->pr = pr;
	debug->input = PR_Resources_Find (pr, "input");

	PR_AddLoadFunc (pr, qwaq_debug_load);
	PR_Resources_Register (pr, "qwaq-debug", debug, qwaq_debug_clear);
	PR_RegisterBuiltins (pr, builtins, debug);
}

void
QWAQ_DebugTarget_Init (progs_t *pr)
{
	PR_AddLoadFunc (pr, qwaq_target_load);
	PR_Resources_Register (pr, "qwaq-target", 0, qwaq_target_clear);
}
