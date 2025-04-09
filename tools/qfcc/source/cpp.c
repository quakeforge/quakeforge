/*
	cpp.c

	cpp preprocessing support

	Copyright (C) 2002 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2002/06/04

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

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_PROCESS_H
# include <process.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_IO_H
#include <io.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "QF/alloc.h"
#include "QF/dstring.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "tools/qfcc/include/cpp.h"
#include "tools/qfcc/include/debug.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/rua-lang.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/symtab.h"

typedef struct cpp_arg_s {
	struct cpp_arg_s *next;
	const char *arg;
} cpp_arg_t;

typedef struct cpp_func_s {
	const char *name;
	int       (*func) (const char *opt, const char *arg);
} cpp_func_t;

ALLOC_STATE (cpp_arg_t, cpp_args);

static cpp_arg_t *cpp_quote_list,  **cpp_quote_tail  = &cpp_quote_list;
static cpp_arg_t *cpp_include_list,**cpp_include_tail= &cpp_include_list;
static cpp_arg_t *cpp_system_list, **cpp_system_tail = &cpp_system_list;
static cpp_arg_t *cpp_after_list,  **cpp_after_tail  = &cpp_after_list;

static const char *cpp_prefix = "";
static const char *cpp_sysroot = QFCC_INCLUDE_PATH;
static cpp_arg_t *cpp_quote_start = 0;	// stack

static cpp_arg_t *cpp_arg_list,    **cpp_arg_tail    = &cpp_arg_list;
static cpp_arg_t *cpp_def_list,    **cpp_def_tail    = &cpp_def_list;
static cpp_arg_t *cpp_undef_list,  **cpp_undef_tail  = &cpp_undef_list;
static cpp_arg_t *cpp_sysinc_list, **cpp_sysinc_tail = &cpp_sysinc_list;

static const char **cpp_argv;
static int  cpp_argc = 0;

static const char *cpp_dep_file = 0;
static const char *cpp_dep_target = 0;
static bool cpp_dep_generate = false;
static bool cpp_dep_phony = false;
static bool cpp_dep_quote = false;

symtab_t   *cpp_macros;
const char *cpp_name = 0;//CPP_NAME;
dstring_t  *tempname;

static const char **
append_cpp_args (const char **arg, cpp_arg_t *arg_list)
{
	cpp_arg_t *cpp_arg;

	for (cpp_arg = arg_list; cpp_arg; cpp_arg = cpp_arg->next)
		*arg++ = cpp_arg->arg;
	return arg;
}

static cpp_arg_t *
cpp_alloc_arg (void)
{
	cpp_arg_t *cpp_arg;
	ALLOC (256, cpp_arg_t, cpp_args, cpp_arg);
	return cpp_arg;
}

static void
cpp_free_arg (cpp_arg_t *cpp_arg)
{
	FREE (cpp_args, cpp_arg);
}

#define CPP_ADD(list, a) \
	do { \
		cpp_arg_t *cpp_arg = cpp_alloc_arg (); \
		*(cpp_arg) = (cpp_arg_t) { .arg = save_string (a) }; \
		*cpp_##list##_tail = cpp_arg; \
		cpp_##list##_tail = &cpp_arg->next; \
	} while (0)

static void
add_cpp_arg (const char *arg)
{
	CPP_ADD (arg, arg);
	cpp_argc++;
}

static void
add_cpp_sysinc (const char *arg)
{
	CPP_ADD (sysinc, arg);
	cpp_argc++;
}

void
add_cpp_undef (const char *arg)
{
	CPP_ADD (undef, arg);
	cpp_argc++;
}

void
add_cpp_def (const char *arg)
{
	CPP_ADD (def, arg);
	cpp_argc++;
}

static int
cpp_depend_ (const char *opt, const char *arg)
{
	options.preprocess_only = true;
	options.dependencies = true;

	add_cpp_def ("-M");
	return 0;
}

static int
cpp_depend_D (const char *opt, const char *arg)
{
	options.dependencies = true;

	add_cpp_def ("-MD");
	return 0;
}

static int
cpp_depend_F (const char *opt, const char *arg)
{
	cpp_dep_file = save_string (arg);

	add_cpp_def ("-MF");
	add_cpp_def (arg);
	return 1;
}

static int
cpp_depend_G (const char *opt, const char *arg)
{
	cpp_dep_generate = true;

	add_cpp_def ("-MG");
	return 0;
}

static int
cpp_depend_M (const char *opt, const char *arg)
{
	options.preprocess_only = true;
	options.dependencies = true;

	add_cpp_def ("-MM");
	return 0;
}

static int
cpp_depend_MD (const char *opt, const char *arg)
{
	options.dependencies = true;

	add_cpp_def ("-MMD");
	return 0;
}

static int
cpp_depend_P (const char *opt, const char *arg)
{
	cpp_dep_phony = true;

	add_cpp_def ("-MP");
	return 0;
}

static int
cpp_depend_Q (const char *opt, const char *arg)
{
	cpp_dep_quote = true;
	cpp_dep_target = save_string (arg);

	add_cpp_def ("-MQ");
	add_cpp_def (arg);
	return 1;
}

static int
cpp_depend_T (const char *opt, const char *arg)
{
	cpp_dep_target = save_string (arg);

	add_cpp_def ("-MT");
	add_cpp_def (arg);
	return 1;
}

#define CPP_DEPEND(name) {#name, cpp_depend_##name}
int
cpp_depend (const char *opt, const char *arg)
{
	static cpp_func_t depend_funcs[] = {
		CPP_DEPEND (),
		CPP_DEPEND (D),
		CPP_DEPEND (F),
		CPP_DEPEND (G),
		CPP_DEPEND (M),
		CPP_DEPEND (MD),
		CPP_DEPEND (P),
		CPP_DEPEND (Q),
		CPP_DEPEND (T),
		{}
	};
	if (!opt) {
		opt = "";
	}
	for (int i = 0; depend_funcs[i].name; i++) {
		if (!strcmp (opt, depend_funcs[i].name)) {
			return depend_funcs[i].func (opt, arg);
		}
	}
	return -1;
}
#undef CPP_DEPEND

static int
cpp_include_I (const char *opt, const char *arg)
{
	if (!arg) {
		return -1;
	}
	if (!strcmp (arg, "-")) {
		return -1;
	}
	CPP_ADD (include, arg);
	add_cpp_def (save_string ("-I"));
	add_cpp_def (save_string (arg));
	return 1;
}

static int
cpp_include_dirafter (const char *opt, const char *arg)
{
	if (!arg) {
		return -1;
	}
	CPP_ADD (quote, arg);
	add_cpp_def (save_string ("-idirafter"));
	add_cpp_def (save_string (arg));
	return 1;
}

static int
cpp_include_prefix (const char *opt, const char *arg)
{
	cpp_prefix = save_string (arg);
	add_cpp_def (save_string ("-iprefix"));
	add_cpp_def (save_string (arg));
	return 0;
}

static int
cpp_include_quote (const char *opt, const char *arg)
{
	if (!arg) {
		return -1;
	}
	CPP_ADD (quote, arg);
	add_cpp_def (save_string ("-iquote"));
	add_cpp_def (save_string (arg));
	return 1;
}

static int
cpp_include_sysroot (const char *opt, const char *arg)
{
	cpp_sysroot = save_string (arg);
	add_cpp_def (save_string ("-isysroot"));
	add_cpp_def (save_string (arg));
	return 0;
}

static int
cpp_include_system (const char *opt, const char *arg)
{
	if (!arg) {
		return -1;
	}
	CPP_ADD (system, arg);

	add_cpp_sysinc ("-isystem");
	add_cpp_sysinc (arg);
	return 1;
}

static int
cpp_include_withprefix (const char *opt, const char *arg)
{
	if (!arg) {
		return -1;
	}
	arg = va ("%s%s", cpp_prefix, arg);
	CPP_ADD (after, arg);
	add_cpp_def (save_string ("-iwithprefix"));
	add_cpp_def (save_string (arg));
	return 0;
}

static int
cpp_include_withprefixbefore (const char *opt, const char *arg)
{
	if (!arg) {
		return -1;
	}
	arg = va ("%s%s", cpp_prefix, arg);
	CPP_ADD (include, arg);
	add_cpp_def (save_string ("-iwithprefixbefore"));
	add_cpp_def (save_string (arg));
	return 0;
}

#define CPP_INCLUDE(name) {#name, cpp_include_##name}
int
cpp_include (const char *opt, const char *arg)
{
	static cpp_func_t include_funcs[] = {
		CPP_INCLUDE (I),
		CPP_INCLUDE (prefix),
		CPP_INCLUDE (dirafter),
		CPP_INCLUDE (quote),
		CPP_INCLUDE (sysroot),
		CPP_INCLUDE (system),
		CPP_INCLUDE (withprefix),
		CPP_INCLUDE (withprefixbefore),
		{}
	};
	for (int i = 0; include_funcs[i].name; i++) {
		if (!strcmp (opt, include_funcs[i].name)) {
			return include_funcs[i].func (opt, arg);
		}
	}
	return -1;
}
#undef CPP_INCLUDE

static rua_macro_t *
make_magic_macro (symtab_t *tab, const char *name, rua_macro_f update)
{
	rua_macro_t *macro = malloc (sizeof (*macro));
	*macro = (rua_macro_t) {
		.name = save_string (name),
		.tail = &macro->tokens,
		.update = update,
	};
	auto sym = symtab_lookup (tab, macro->name);
	if (sym) {
		internal_error (0, "\"%s\" redefined", macro->name);
	}
	sym = new_symbol (macro->name);
	sym->sy_type = sy_macro;
	sym->macro = macro;
	symtab_addsymbol (tab, sym);
	return macro;
}

void cpp_define (const char *arg)
{
	if (!cpp_macros) {
		cpp_macros = new_symtab (0, stab_global);
		make_magic_macro (cpp_macros, "__FILE__", rua_macro_file);
		make_magic_macro (cpp_macros, "__LINE__", rua_macro_line);
		//make_magic_macro (cpp_macros, "__VA_ARGS__", rua_macro_va_args);
	}
	size_t len = strlen (arg);
	if (len > 0x10000) {
		error (0, "command line define too long: %zd", len);
		return;
	}
	char argstr[len + 4];
	strcpy (argstr, arg);
	argstr[len] = '\n';
	argstr[len + 1] = 0;
	char *eq = strchr (argstr, '=');
	if (eq) {
		*eq = ' ';
	} else {
		strcpy (argstr + len, " 1\n");
	}
	rua_parse_define (argstr);

	arg = va ("-D%s", arg);
	CPP_ADD (def, arg);
	cpp_argc++;
}

void cpp_undefine (const char *arg)
{
	if (cpp_macros) {
		auto sym = symtab_lookup (cpp_macros, arg);
		if (sym) {
			symtab_removesymbol (cpp_macros, sym);
		}
	}
	arg = va ("-D%s", arg);
	CPP_ADD (undef, arg);
}

void
parse_cpp_name (void)
{
	char       *n, *e;

	if (!cpp_name || !*cpp_name) {
		return;
	}

	for (n = strdup (cpp_name); *n; n = e) {
		for (; *n && *n == ' '; n++) continue;
		for (e = n; *e && *e != ' '; e++) continue;
		if (*e) *e++ = 0;
		add_cpp_arg (n);
	}
}

static void
build_cpp_args (const char *in_name, const char *out_name)
{
	cpp_arg_t  *cpp_arg;
	const char **arg;

	if (cpp_argv)
		free (cpp_argv);
	cpp_argv = (const char **)malloc ((cpp_argc + 1) * sizeof (char *));
	for (arg = cpp_argv, cpp_arg = cpp_arg_list;
		 cpp_arg;
		 cpp_arg = cpp_arg->next) {
		if (!strcmp (cpp_arg->arg, "%u")) {
			arg = append_cpp_args (arg, cpp_undef_list);
		} else if (!strcmp (cpp_arg->arg, "%s")) {
			arg = append_cpp_args (arg, cpp_sysinc_list);
		} else if (!strcmp (cpp_arg->arg, "%d")) {
			arg = append_cpp_args (arg, cpp_def_list);
		} else if (!strcmp (cpp_arg->arg, "%i")) {
			*arg++ = in_name;
		} else if (!strcmp (cpp_arg->arg, "%o")) {
			if (!options.preprocess_only) {
				*arg++ = out_name;
			}
		} else {
			if (!options.preprocess_only || strcmp (cpp_arg->arg, "-o") != 0)
				*arg++ = cpp_arg->arg;
		}
	}
	*arg = 0;
}

//============================================================================

void
intermediate_file (dstring_t *ifile, const char *filename, const char *ext,
				   int local)
{
	if (options.save_temps) {
		char	*basename = strdup (filename);
		char	*temp;

		temp = strrchr (basename, '/');
		if (!temp)
			temp = basename;
		temp = strrchr (temp, '.');
		if (temp)
			*temp = '\0';	// ignore the rest of the string

		temp = strrchr (basename, '/');
		if (!temp)
			temp = basename;
		else
			temp++;

		if (*sourcedir) {
			dsprintf (ifile, "%s%c%s.%s", sourcedir,
					  PATH_SEPARATOR, temp, ext);
		} else {
			dsprintf (ifile, "%s.%s", temp, ext);
		}
		free (basename);
	} else if (local) {
		char       *temp2 = strrchr (this_program, PATH_SEPARATOR);
		dsprintf (ifile, "%sXXXXXX", temp2 ? temp2 + 1 : this_program);
	} else {
		const char *temp1 = getenv ("TMPDIR");
		char       *temp2 = strrchr (this_program, PATH_SEPARATOR);

		if ((!temp1) || (!temp1[0])) {
			temp1 = getenv ("TEMP");
			if ((!temp1) || (!temp1[0])) {
				temp1 = "/tmp";
			}
		}

		dsprintf (ifile, "%s%c%sXXXXXX", temp1,
				  PATH_SEPARATOR, temp2 ? temp2 + 1 : this_program);
	}
}

static FILE *
run_cpp (const char *filename, const char *ext)
{
#ifndef _WIN32
	pid_t       pid;
	int         tempfd = 0;
#endif

	intermediate_file (tempname, filename, ext ? ext : "p", 0);
	build_cpp_args (filename, tempname->str);
	if (!cpp_argv[0]) {
		internal_error(0, "cpp_argv[0] is null");
	}

#ifdef _WIN32
	if (!options.save_temps && !options.preprocess_only) {
		mktemp (tempname->str);
	}

	FILE       *tmp = fopen (tempname->str, "wb");
	if (tmp == NULL) {
		fprintf (stderr, "%s: qfcc was unable to open\n",
				 tempname->str);
		return 0;
	}
	fclose (tmp);

	int		status;

	if (options.verbosity > 1) {
		const char **a;
		for (a = cpp_argv; *a; a++)
			printf ("%s ", *a);
		puts("");
	}

#if defined(_WIN64) || defined(_WIN32)
	status = spawnvp (_P_WAIT, cpp_argv[0], (char **) cpp_argv);
#else
	status = spawnvp (_P_WAIT, cpp_argv[0], cpp_argv);
#endif

	if (status) {
		fprintf (stderr, "%s: cpp returned error code %d\n",
				filename,
				status);
		return 0;
	}

	if (options.preprocess_only)
		return 0;
	return fopen (tempname->str, "rb");
#else
	if (!options.save_temps && !options.preprocess_only)
		tempfd = mkstemp (tempname->str);

	if ((pid = fork ()) == -1) {
		perror ("fork");
		return 0;
	}
	if (!pid) {
		// we're a child, check for abuse
		if (options.verbosity > 1) {
			const char **a;
			for (a = cpp_argv; *a; a++)
				printf ("%s ", *a);
			puts("");
		}
#ifdef HAVE_EXECVP
		execvp (cpp_argv[0], (char **)cpp_argv);
#else
		execve (cpp_argv[0], (char **)cpp_argv, environ);
#endif
		perror (cpp_argv[0]);
		exit (1);
	} else {
		// give parental guidance (or bury it in the back yard)
		int         status;
		pid_t       rc;

//			printf ("pid = %d\n", pid);
#ifdef HAVE_WAITPID
		rc = waitpid (0, &status, 0 | WUNTRACED);
#else
		rc = wait (&status);
#endif
		if ((rc) != pid) {
			if (rc == -1) {
				perror ("wait");
				return 0;
			}
			fprintf (stderr, "%s: The wrong child (%ld) died. Don't ask me, I don't know either.\n",
					this_program,
					(long) rc);
			return 0;
		}
		if (WIFEXITED (status)) {
			if (WEXITSTATUS (status)) {
				fprintf (stderr, "%s: cpp returned error code %d\n",
						filename,
						WEXITSTATUS (status));
				return 0;
			}
		} else {
			fprintf (stderr, "%s: cpp returned prematurely.\n", filename);
			return 0;
		}
	}
	if (options.preprocess_only) {
		return 0;
	} else if (options.save_temps) {
		set_line_file (1, tempname->str, 0);
		return fopen (tempname->str, "rb");
	} else {
		set_line_file (1, "1", 0);
		return fdopen (tempfd, "rb");
	}
#endif
}

FILE *
preprocess_file (const char *filename, const char *ext)
{
	if (cpp_name) {
		return run_cpp (filename, ext);
	} else {
		set_line_file (1, filename, 0);
		cpp_push_quote_path (filename);
		FILE *file = fopen (filename, "rb");
		if (!file) {
			fprintf (stderr, "%s: error: %s: %s\n", this_program, filename,
					 strerror (errno));
			pr.error_count++;
		}
		return file;
	}
}

static const char *
test_path (const char *path, const char *name)
{
	static dstring_t *fullpath;
	if (!fullpath) {
		fullpath = dstring_new ();
	}
	dstring_copystr (fullpath, path);
	if (fullpath->size > 1 && fullpath->str[fullpath->size - 2] != '/') {
		dstring_appendstr (fullpath, "/");
	}
	dstring_appendstr (fullpath, name);
	if (Sys_FileExists (fullpath->str) == 0) {
		return fullpath->str;
	}
	return 0;
}

const char *
cpp_find_file (const char *name, int quote, bool *is_system)
{
	*is_system = false;
	if (*name == '/') {
		return name;
	}
	const char *path;
	if (quote == '"') {
		if (cpp_quote_start
			&& (path = test_path (cpp_quote_start->arg, name))) {
			return path;
		}
		for (auto dir = cpp_quote_list; dir; dir = dir->next) {
			if ((path = test_path (cpp_quote_start->arg, dir->arg))) {
				return path;
			}
		}
	}
	for (auto dir = cpp_include_list; dir; dir = dir->next) {
		if ((path = test_path (dir->arg, name))) {
			return path;
		}
	}
	*is_system = true;
	for (auto dir = cpp_system_list; dir; dir = dir->next) {
		if ((path = test_path (dir->arg, name))) {
			return path;
		}
	}
	for (auto dir = cpp_after_list; dir; dir = dir->next) {
		if ((path = test_path (dir->arg, name))) {
			return path;
		}
	}
	if (!errno) {
		errno = ENOENT;
	}
	return 0;
}

void
cpp_push_quote_path (const char *path)
{
	if (!path) {
		path = "";
	} else {
		const char *e = strrchr (path, '/');
		if (!e) {
			path = "";
		} else {
			path = save_substring (path, e - path);
		}
	}
	auto quote_path = cpp_alloc_arg ();
	quote_path->arg = path;
	quote_path->next = cpp_quote_start;
	cpp_quote_start = quote_path;
}

void cpp_pop_quote_path (void)
{
	if (!cpp_quote_start) {
		internal_error (0, "quote path stack underflow");
	}
	auto quote_path = cpp_quote_start;
	cpp_quote_start = cpp_quote_start->next;
	cpp_free_arg (quote_path);
}

void
cpp_write_dependencies (const char *sourcefile, const char *outputfile)
{
	if (!options.dependencies || cpp_name) {
		return;
	}
	if (!cpp_dep_target) {
		const char *out = strrchr (outputfile, '/');
		if (!out) {
			out = outputfile - 1;
		}
		out++;
		const char *dot = strrchr (outputfile, '.');
		if (!dot) {
			dot = outputfile + strlen (outputfile);
		}
		const char *tgt = va ("%.*s.qfo", (int) (dot - out), out);
		cpp_dep_target = save_string (tgt);
		cpp_dep_quote = 1;//FIXME implement
	}
	FILE *out = stdout;
	if (cpp_dep_file) {
		out = fopen (cpp_dep_file, "wt");
		if (!out) {
			fprintf (stderr, "%s: %s\n", cpp_dep_file, strerror (errno));
			exit (1);
		}
	}
	fprintf (out, "%s: %s", cpp_dep_target, sourcefile);
	for (size_t i = 0; i < pr.comp_files.size; i++) {
		const char *cf = pr.comp_files.a[i];
		if (!strcmp (cf, sourcefile)) {
			continue;
		}
		fprintf (out, " \\\n %s", cf);
	}
	if (cpp_dep_phony) {
		for (size_t i = 0; i < pr.comp_files.size; i++) {
			const char *cf = pr.comp_files.a[i];
			if (!strcmp (cf, sourcefile)) {
				continue;
			}
			fprintf (out, "\n%s:", cf);
		}
	}
	fprintf (out, "\n");
}
