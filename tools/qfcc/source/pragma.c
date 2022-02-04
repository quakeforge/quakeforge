/*
	praga.c

	pragma handling

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/11/22

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
#include <stdlib.h>

#include "QF/alloc.h"

#include "QF/progs/pr_comp.h"

#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/opcodes.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/pragma.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/type.h"

typedef struct pragma_arg_s {
	struct pragma_arg_s *next;
	const char *arg;
} pragma_arg_t;

static pragma_arg_t *pragma_args_freelist;
static pragma_arg_t *pragma_args;
static pragma_arg_t **pragma_args_tail = &pragma_args;

static void
set_traditional (int traditional)
{
	switch (traditional) {
		case -1:
			options.traditional = 0;
			options.advanced = 2;
			options.code.progsversion = PROG_VERSION;
			type_default = &type_int;
			type_long_int = &type_long;
			type_ulong_uint = &type_ulong;
			break;
		case 0:
			options.traditional = 0;
			options.advanced = 1;
			options.code.progsversion = PROG_V6P_VERSION;
			type_default = &type_int;
			type_long_int = &type_int;
			type_ulong_uint = &type_uint;
			break;
		case 1:
			options.traditional = 1;
			options.advanced = 0;
			options.code.progsversion = PROG_ID_VERSION;
			type_default = &type_float;
			break;
		case 2:
			options.traditional = 2;
			options.advanced = 0;
			options.code.progsversion = PROG_ID_VERSION;
			type_default = &type_float;
			break;
	}
	opcode_init ();		// reset the opcode table
}

static void
set_bug (pragma_arg_t *args)
{
	if (!args) {
		warning (0, "missing bug flag");
		return;
	}
	const char *flag = args->arg;
	if (!strcmp (flag, "none")) {
		options.bug.silent = true;
	} else if (!strcmp (flag, "!none")) {
		options.bug.silent = false;
	} else if (!strcmp (flag, "die")) {
		options.bug.promote = true;
	} else if (!strcmp (flag, "!die")) {
		options.bug.promote = false;
	}
	if (args->next) {
		warning (0, "pragma bug: ignoring extra arguments");
	}
}

static void
set_warn (pragma_arg_t *args)
{
	if (!args) {
		warning (0, "missing warn flag");
		return;
	}
	const char *flag = args->arg;
	if (!strcmp (flag, "error")) {
		options.warnings.promote = true;
	} else if (!strcmp (flag, "!error")) {
		options.warnings.promote = false;
	}
	if (args->next) {
		warning (0, "pragma warn: ignoring extra arguments");
	}
}

static void
set_optimize (pragma_arg_t *args)
{
	if (!args) {
		warning (0, "missing warn flag");
		return;
	}
	const char *flag = args->arg;
	if (!strcmp (flag, "on") || !strcmp (flag, "!off")) {
		options.code.optimize = true;
	} else if (!strcmp (flag, "!on") || !strcmp (flag, "off")) {
		options.code.optimize = false;
	}
	if (args->next) {
		warning (0, "pragma optimize: ignoring extra arguments");
	}
}

void
pragma_process ()
{
	if (!pragma_args) {
		warning (0, "empty pragma");
		return;
	}
	const char *id = pragma_args->arg;
	if (!strcmp (id, "traditional")) {
		set_traditional (2);
	} else if (!strcmp (id, "extended")) {
		set_traditional (1);
	} else if (!strcmp (id, "advanced")) {
		set_traditional (0);
	} else if (!strcmp (id, "ruamoko") || !strcmp (id, "raumoko")) {
		set_traditional (-1);
	} else if (!strcmp (id, "bug")) {
		set_bug (pragma_args->next);
	} else if (!strcmp (id, "warn")) {
		set_warn (pragma_args->next);
	} else if (!strcmp (id, "optimize")) {
		set_optimize (pragma_args->next);
	} else {
		warning (0, "unknown pragma: '%s'", id);
	}
	*pragma_args_tail = pragma_args_freelist;
	pragma_args_tail = &pragma_args;
	pragma_args = 0;
}

void
pragma_add_arg (const char *id)
{
	pragma_arg_t *arg;
	ALLOC (16, pragma_arg_t, pragma_args, arg);
	arg->arg = save_string (id);
	*pragma_args_tail = arg;
	pragma_args_tail = &arg->next;
}
