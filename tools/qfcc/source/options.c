/*
	options.c

	command line options handlnig

	Copyright (C) 2001 Jeff Teunissen <deek@d2dc.net>

	Author: Jeff Teunissen <deek@d2dc.net>
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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>
#include <stdio.h>

#include <getopt.h>

#include "QF/va.h"

#include "QF/progs/pr_comp.h"

#include "tools/qfcc/include/cpp.h"
#include "tools/qfcc/include/linker.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/type.h"

#include "tools/qfcc/source/qc-parse.h"

options_t   options = {
	.code = {
		.fast_float = true,
		.promote_float = true,
	},
	.warnings = {
		.uninited_variable = true,
		.unused = true,
		.executable = true,
		.traditional = true,
		.precedence = true,
		.initializer = true,
		.unimplemented = true,
		.redeclared = true,
		.enum_switch = true,
	},
	.single_cpp = true,
	.save_temps = false,
	.verbosity = 0,
};
static options_t   options_user_set;

const char *this_program;
const char **source_files;
static int num_files;
static int files_size;

// keep me sane when adding long options :P
enum {
	start_opts = 255,	// not used, starts the enum.
	OPT_ADVANCED,
	OPT_BLOCK_DOT,
	OPT_CPP,
	OPT_EXTENDED,
	OPT_FRAMES,
	OPT_INCLUDE,
	OPT_NO_DEFAULT_PATHS,
	OPT_PROGDEFS,
	OPT_QCCX_ESCAPES,
	OPT_RUAMOKO,
	OPT_TRADITIONAL,
	OPT_BUG,
};

static struct option const long_options[] = {
	{"advanced", no_argument, 0, OPT_ADVANCED},
	{"block-dot", optional_argument, 0, OPT_BLOCK_DOT},
	{"bug", required_argument, 0, OPT_BUG},
	{"code", required_argument, 0, 'C'},
	{"cpp", required_argument, 0, OPT_CPP},
	{"define", required_argument, 0, 'D'},
	{"extended", no_argument, 0, OPT_EXTENDED},
	{"files", no_argument, 0, 'F'},
	{"frames", no_argument, 0, OPT_FRAMES},
	{"help", no_argument, 0, 'h'},
	{"include", required_argument, 0, OPT_INCLUDE},
	{"no-default-paths", no_argument, 0, OPT_NO_DEFAULT_PATHS},
	{"notice", required_argument, 0, 'N'},
	{"output-file", required_argument, 0, 'o'},
	{"progdefs", no_argument, 0, OPT_PROGDEFS},
	{"progs-src", required_argument, 0, 'P'},
	{"qccx-escapes", no_argument, 0, OPT_QCCX_ESCAPES},
	{"quiet", no_argument, 0, 'q'},
	{"raumoko", no_argument, 0, OPT_RUAMOKO},
	{"relocatable", no_argument, 0, 'r'},
	{"ruamoko", no_argument, 0, OPT_RUAMOKO},
	{"save-temps", no_argument, 0, 'S'},
	{"source", required_argument, 0, 's'},
	{"traditional", no_argument, 0, OPT_TRADITIONAL},
	{"undefine", required_argument, 0, 'U'},
	{"verbose", no_argument, 0, 'v'},
	{"version", no_argument, 0, 'V'},
	{"warn", required_argument, 0, 'W'},
	{NULL, 0, NULL, 0}
};

static const char *short_options =
	"-"		// magic option parsing mode doohicky (must come first)
	"C:"	// code options
	"c"		// separate compilation
	"D:"	// define
	"E"		// only preprocess
	"F"		// generate files.dat
	"g"		// debug
	"h"		// help
	"i::"	// set includes
	"I:"	// set includes
	"L:"	// lib path
	"l:"	// lib file
	"M::"
	"N:"	// notice options
	"o:"	// output file
	"O"		// optimize
	"P:"	// progs.src name
	"q"		// quiet
	"r"		// partial linking
	"S"		// save temps
	"s:"	// source dir
	"U:"	// undefine
	"V"		// version
	"v"		// verbose
	"W:"	// warning options
	"z"		// compress qfo files
	;

static void
usage (int status)
{
	printf ("%s - QuakeForge Code Compiler\n", this_program);
	printf ("Usage: %s [options] [files]\n", this_program);
	printf (
"Options:\n"
"        --advanced            Advanced Ruamoko mode\n"
"        --bug OPTION,...      Set bug options\n"
"    -C, --code OPTION,...     Set code generation options\n"
"    -c                        Only compile, don't link\n"
"        --cpp CPPSPEC         cpp execution command line\n"
"    -D, --define SYMBOL[=VAL] Define symbols for the preprocessor\n"
"    -E                        Only preprocess\n"
"        --extended            Allow extended keywords in traditional mode\n"
"    -F, --files               Generate files.dat\n"
"        --frames              Generate <source>.frame files\n"
"    -g                        Generate debugging info\n"
"    -h, --help                Display this help and exit\n"
"    -I DIR                    Set directories for the preprocessor\n"
"                              to search for #includes\n"
"        --include FILE        Process file as if `#include \"file\"'\n"
"                              appeared as the first line of the primary\n"
"                              source file.\n"
"    -L DIR                    Add linker library search path\n"
"    -l LIB                    Link with libLIB.a\n"
"    -M[flags]                 Generate depency info. Dependent on cpp\n"
"        --no-default-paths    Do not search default paths for headers or\n"
"                              libs\n"
"    -N, --notice OPTION,...   Set notice options\n"
"    -o, --output-file FILE    Specify output file name\n"
"        --progdefs            Genderate progdefs.h\n"
"    -P, --progs-src FILE      File to use instead of progs.src\n"
"        --qccx-escapes        Use QCCX escape sequences instead of standard\n"
"                              C/QuakeForge sequences.\n"
"    -q, --quiet               Inhibit usual output\n"
"    -r, --relocatable         Incremental linking\n"
"        --raumoko             Use both Ruamoko language features and the\n"
"        --ruamoko             Ruamoko ISA, providing access to SIMD types\n"
"                              and a stack for locals.\n"
"                              default for separate compilation mode\n"
"    -S, --save-temps          Do not delete temporary files\n"
"    -s, --source DIR          Look for progs.src in DIR instead of \".\"\n"
"        --traditional         Traditional QuakeC mode: implies v6only\n"
"                              default when using progs.src\n"
"    -U, --undefine SYMBOL     Undefine preprocessor symbols\n"
"    -V, --version             Output version information and exit\n"
"    -v, --verbose             Display more output than usual\n"
"    -W, --warn OPTION,...     Set warning options\n"
"    -z                        Compress object files\n"
"\n"
"For help on options for --code, --notice and --warn, see the qfcc(1) manual\n"
"page, or use `help' as the option.\n"
	);
	exit (status);
}

static bool
code_usage (void)
{
	printf ("%s - QuakeForge Code Compiler\n", this_program);
	printf ("Code generation options\n");
	printf (
"    [no-]const-initializers Treat initialized globals as constants.\n"
"    [no-]cow                Allow assignment to initialized globals.\n"
"    [no-]cpp                Preprocess all input files with cpp.\n"
"    [no-]crc                Write progdefs.h crc to progs.dat.\n"
"    [no-]debug              Generate debug information.\n"
"    [no-]fast-float         Use float values directly in \"if\" statements.\n"
"    help                    Display this text.\n"
"    [no-]local-merging      Merge the local variable blocks into one.\n"
"    [no-]optimize           Perform various optimizations on the code.\n"
"    [no-]promote-float      Promote float when passed through ...\n"
"    [no-]short-circuit      Generate short circuit code for logical\n"
"                            operators.\n"
"    [no-]single-cpp         Convert progs.src to cpp input file.\n"
"    [no-]vector-calls       Generate slower but data efficient code for\n"
"                            passing\n"
"                            vectors to functiosn.\n"
"    [no-]vector-components  Create *_[xyz] symbols for vector variables.\n"
"    target=v6|v6p|ruamoko   Generate code for the specified target VM\n"
"                                v6       Standard Quake VM (qcc compatible)\n"
"                                v6p     *QuakeForge extended v6 instructions\n"
"                                ruamoko  QuakeForge SIMD instructions\n"
"\n"
"For details, see the qfcc(1) manual page\n"
	);
	exit (0);
	return false;
}

static bool
warning_usage (void)
{
	printf ("%s - QuakeForge Code Compiler\n", this_program);
	printf ("Warnings options\n");
	printf (
"    all                     Turn on all warnings.\n"
"    [no-]cow                Warn about assignement to initialized globals.\n"
"    [no-]error              Treat warnings as errors.\n"
"    [no-]executable         Warn about statements with no effect.\n"
"    help                    Display his text.\n"
"    [no-]initializer        Warn about excessive structure/array\n"
"                            initializers.\n"
"    [no-]integer-divide     Warn about division of integer constants.\n"
"    [no-]interface-check    Warn about methods not declared in the\n"
"                            interface.\n"
"    none                    Turn off all warnings.\n"
"    [no-]precedence         Warn about potentially ambiguous logic.\n"
"    [no-]switch             Warn about unhandled enum values in switch\n"
"                            statements.\n"
"    [no-]redeclared         Warn about redeclared local variables.\n"
"    [no-]traditional        Warn about bad code that qcc allowed.\n"
"    [no-]undef-function     Warn about calling a yet to be defined\n"
"                            function.\n"
"    [no-]unimplemented      Warn about unimplemented class methods.\n"
"    [no-]unused             Warn about unused local variables.\n"
"    [no-]uninited-var       Warn about using uninitialied variables.\n"
"    [no-]vararg-integer     Warn about passing integer constants to\n"
"                            functions with variable args.\n"
"\n"
"For details, see the qfcc(1) manual page\n"
	);
	exit (0);
	return false;
}

static bool
notice_usage (void)
{
	printf ("%s - QuakeForge Code Compiler\n", this_program);
	printf ("Notice options\n");
	printf (
"    help                    Display his text.\n"
"    none                    Turn off all notices.\n"
"    warn                    Change notices to warnings.\n"
"\n"
"For details, see the qfcc(1) manual page\n"
	);
	exit (0);
	return false;
}

static bool
bug_usage (void)
{
	printf ("%s - QuakeForge Code Compiler\n", this_program);
	printf ("Bug options\n");
	printf (
"    help                    Display his text.\n"
"    none                    Turn off all bugs (don't we wish: messages).\n"
"    die                     Change bugs to internal errors.\n"
"\n"
"This is a developer feature and thus not in the manual page\n"
	);
	exit (0);
	return false;
}

static void
add_file (const char *file)
{
	if (num_files >= files_size - 1) {
		files_size += 16;
		source_files = realloc (source_files, files_size * sizeof (char *));
	}
	source_files[num_files++] = save_string (file);
	source_files[num_files] = 0;
}

#define OPTION(type, opt, str, field, flag) \
	({ \
		bool match = false; \
		if (strcasecmp (opt, str) == 0) { \
			options.type.field = flag; \
			options_user_set.type.field = true; \
			match = true; \
		} \
		match; \
	})

static bool
parse_block_dot_option (const char *opt)
{
	bool        flag = true;

	if (!strncasecmp (opt, "no-", 3)) {
		flag = false;
		opt += 3;
	}
	if (OPTION (block_dot, opt, "initial", initial, flag)) {
		return true;
	}
	if (OPTION (block_dot, opt, "thread", thread, flag)) {
		return true;
	}
	if (OPTION (block_dot, opt, "dead", dead, flag)) {
		return true;
	}
	if (OPTION (block_dot, opt, "final", final, flag)) {
		return true;
	}
	if (OPTION (block_dot, opt, "dags", dags, flag)) {
		return true;
	}
	if (OPTION (block_dot, opt, "expr", expr, flag)) {
		return true;
	}
	if (OPTION (block_dot, opt, "flow", flow, flag)) {
		return true;
	}
	if (OPTION (block_dot, opt, "reaching", reaching, flag)) {
		return true;
	}
	if (OPTION (block_dot, opt, "statements", statements, flag)) {
		return true;
	}
	if (OPTION (block_dot, opt, "live", live, flag)) {
		return true;
	}
	if (OPTION (block_dot, opt, "post", post, flag)) {
		return true;
	}
	return false;
}

static bool
parse_bug_option (const char *opt)
{
	if (OPTION (bug, opt, "help", help, bug_usage ())) {
		return true;
	}
	if (OPTION (bug, opt, "none", silent, true)) {
		return true;
	}
	if (OPTION (bug, opt, "warn", promote, true)) {
		return true;
	}
	return false;
}

static bool
parse_notice_option (const char *opt)
{
	if (OPTION (notices, opt, "help", help, notice_usage ())) {
		return true;
	}
	if (OPTION (notices, opt, "none", silent, true)) {
		return true;
	}
	if (OPTION (notices, opt, "warn", promote, true)) {
		return true;
	}
	return false;
}

bool
parse_warning_option (const char *opt)
{
	if (!(strcasecmp (opt, "all"))) {
		OPTION(warnings, "", "", cow, true);
		OPTION(warnings, "", "", undefined_function, true);
		OPTION(warnings, "", "", uninited_variable, true);
		OPTION(warnings, "", "", vararg_integer, true);
		OPTION(warnings, "", "", integer_divide, true);
		OPTION(warnings, "", "", interface_check, true);
		OPTION(warnings, "", "", unused, true);
		OPTION(warnings, "", "", executable, true);
		OPTION(warnings, "", "", traditional, true);
		OPTION(warnings, "", "", precedence, true);
		OPTION(warnings, "", "", initializer, true);
		OPTION(warnings, "", "", unimplemented, true);
		OPTION(warnings, "", "", redeclared, true);
		OPTION(warnings, "", "", enum_switch, true);
		return true;
	} else if (!(strcasecmp (opt, "none"))) {
		OPTION(warnings, "", "", cow, false);
		OPTION(warnings, "", "", undefined_function, false);
		OPTION(warnings, "", "", uninited_variable, false);
		OPTION(warnings, "", "", vararg_integer, false);
		OPTION(warnings, "", "", integer_divide, false);
		OPTION(warnings, "", "", interface_check, false);
		OPTION(warnings, "", "", unused, false);
		OPTION(warnings, "", "", executable, false);
		OPTION(warnings, "", "", traditional, false);
		OPTION(warnings, "", "", precedence, false);
		OPTION(warnings, "", "", initializer, false);
		OPTION(warnings, "", "", unimplemented, false);
		OPTION(warnings, "", "", redeclared, false);
		OPTION(warnings, "", "", enum_switch, false);
		return true;
	} else {
		bool        flag = true;

		if (!strncasecmp (opt, "no-", 3)) {
			flag = false;
			opt += 3;
		}
		if (OPTION(warnings, opt, "cow", cow, flag)) {
			return true;
		}
		if (OPTION(warnings, opt, "error", promote, flag)) {
			return true;
		}
		if (OPTION(warnings, opt, "executable", executable, flag)) {
			return true;
		}
		if (OPTION(warnings, opt, "help", help, warning_usage())) {
			return true;
		}
		if (OPTION(warnings, opt, "initializer", initializer, flag)) {
			return true;
		}
		if (OPTION(warnings, opt, "integer-divide", integer_divide, flag)) {
			return true;
		}
		if (OPTION(warnings, opt, "interface-check", interface_check, flag)) {
			return true;
		}
		if (OPTION(warnings, opt, "precedence", precedence, flag)) {
			return true;
		}
		if (OPTION(warnings, opt, "redeclared", redeclared, flag)) {
			return true;
		}
		if (OPTION(warnings, opt, "switch", enum_switch, flag)) {
			return true;
		}
		if (OPTION(warnings, opt, "traditional", traditional, flag)) {
			return true;
		}
		if (OPTION(warnings, opt, "undef-function", undefined_function, flag)) {
			return true;
		}
		if (OPTION(warnings, opt, "unimplemented", unimplemented, flag)) {
			return true;
		}
		if (OPTION(warnings, opt, "unused", unused, flag)) {
			return true;
		}
		if (OPTION(warnings, opt, "uninited-var", uninited_variable, flag)) {
			return true;
		}
		if (OPTION(warnings, opt, "vararg-integer", vararg_integer, flag)) {
			return true;
		}
	}
	return false;
}

bool
parse_code_option (const char *opt)
{
	bool        flag = true;

	if (!(strncasecmp (opt, "target=", 7))) {
		const char *tgt = opt + 7;
		if (!strcasecmp (tgt, "v6")) {
			options.code.progsversion = PROG_ID_VERSION;
		} else if (!strcasecmp (tgt, "v6p")) {
			options.code.progsversion = PROG_V6P_VERSION;
		} else if (!strcasecmp (tgt, "ruamoko")) {
			options.code.progsversion = PROG_VERSION;
		} else {
			fprintf (stderr, "unknown target: %s\n", tgt);
			exit (1);
		}
		return true;
	}
	if (!strncasecmp (opt, "no-", 3)) {
		flag = false;
		opt += 3;
	}
	if (!strcasecmp (opt, "cpp")) {
		cpp_name = flag ? CPP_NAME : 0;
		return true;
	}
	if (!strcasecmp (opt, "single-cpp")) {
		options.single_cpp = flag;
		return true;
	}
	if (OPTION(code, opt, "cow", cow, flag)) {
		return true;
	}
	if (OPTION(code, opt, "crc", crc, flag)) {
		return true;
	}
	if (OPTION(code, opt, "debug", debug, flag)) {
		return true;
	}
	if (OPTION(code, opt, "fast-float", fast_float, flag)) {
		return true;
	}
	if (OPTION(code, opt, "promote-float", promote_float, flag)) {
		return true;
	}
	if (OPTION(code, opt, "help", help, code_usage ())) {
		return true;
	}
	if (OPTION(code, opt, "local-merging", local_merging, flag)) {
		return true;
	}
	if (OPTION(code, opt, "optimize", optimize, flag)) {
		return true;
	}
	if (OPTION(code, opt, "short-circuit", short_circuit, flag)) {
		return true;
	}
	if (OPTION(code, opt, "ifstring", ifstring, flag)) {
		return true;
	}
	if (OPTION(code, opt, "vector-calls", vector_calls, flag)) {
		return true;
	}
	if (OPTION(code, opt, "vector-components", vector_components, flag)) {
		return true;
	}
	if (OPTION(code, opt, "const-initializers", const_initializers, flag)) {
		return true;
	}
	return false;
}

static int
parse_args_string (const char *args, bool (*parse) (const char *))
{
	void free_opts (char **o) { free (*o); }
	__attribute__((cleanup(free_opts))) char *opts = strdup (optarg);

	char       *temp = strtok (opts, ",");
	while (temp) {
		if (!parse (temp)) {
			return false;
		}
		temp = strtok (NULL, ",");
	}
	return true;
}

int
DecodeArgs (int argc, char **argv)
{
	int         c;
	int         saw_E = 0, saw_MD = 0;

	add_cpp_undef ("-undef");
	add_cpp_undef ("-nostdinc");
	add_cpp_undef ("-fno-extended-identifiers");

	cpp_define ("__QFCC__");
	cpp_define ("__QUAKEC__");

	sourcedir = "";
	progs_src = "progs.src";

	while ((c = getopt_long (argc, argv, short_options, long_options, 0))
		   != EOF) {
		switch (c) {
			case 1:						// ordinary file
				add_file (NORMALIZE (optarg));
				break;
			case 'o':
				if (options.output_file) {
					fprintf (stderr, "%s: -o must not be used more than once\n",
							 this_program);
					exit (1);
				} else {
					options.output_file = save_string (NORMALIZE (optarg));
				}
				break;
			case 'l':					// lib file
				add_file (va (0, "-l%s", NORMALIZE (optarg)));
				break;
			case 'L':
				linker_add_path (NORMALIZE (optarg));
				break;
			case 'h':					// help
				usage (0);
				break;
			case 'V':					// version
				printf ("%s version %s\n", PACKAGE_NAME, PACKAGE_VERSION);
				exit (0);
				break;
			case 's':					// src dir
				sourcedir = save_string (NORMALIZE (optarg));
				break;
			case 'P':					// progs-src
				progs_src = save_string (NORMALIZE (optarg));
				break;
			case 'F':
				options.files_dat = true;
				break;
			case OPT_PROGDEFS:
				options.progdefs_h = true;
				break;
			case OPT_QCCX_ESCAPES:
				options.qccx_escapes = true;
				break;
			case 'q':					// quiet
				options.verbosity -= 1;
				break;
			case 'v':					// verbose
				options.verbosity += 1;
				break;
			case 'g':					// debug
				options.code.debug = true;
				break;
			case 'O':					// optimize
				options.code.optimize = true;
				break;
			case OPT_FRAMES:
				options.frames_files = 1;
				break;
			case OPT_EXTENDED:
				options.traditional = 1;
				options.advanced = false;
				options.code.progsversion = PROG_ID_VERSION;
				options.code.const_initializers = true;
				break;
			case OPT_TRADITIONAL:
				options.traditional = 2;
				options.advanced = 0;
				options.code.progsversion = PROG_ID_VERSION;
				options.code.const_initializers = true;
				break;
			case OPT_ADVANCED:
				options.traditional = 0;
				options.advanced = 1;
				options.code.progsversion = PROG_V6P_VERSION;
				options.code.const_initializers = false;
				break;
			case OPT_RUAMOKO:
				options.traditional = 0;
				options.advanced = 2;
				options.code.progsversion = PROG_VERSION;
				options.code.const_initializers = false;
				break;
			case OPT_BLOCK_DOT:
				if (optarg) {
					parse_args_string (optarg, parse_block_dot_option);
			    } else {
					options.block_dot.initial = true;
					options.block_dot.thread = true;
					options.block_dot.dead = true;
					options.block_dot.final = true;
					options.block_dot.dags = true;
					options.block_dot.expr = true;
					options.block_dot.flow = true;
					options.block_dot.reaching = true;
					options.block_dot.statements = true;
					options.block_dot.live = true;
					options.block_dot.post = true;
				}
				break;
			case 'c':
				options.compile = true;
				break;
			case 'r':
				options.partial_link = true;
				break;
			case 'z':
				options.gzip = true;
				break;
			case 'C':
				parse_args_string (optarg, parse_code_option);
				break;
			case 'W':
				parse_args_string (optarg, parse_warning_option);
				break;
			case 'N':
				parse_args_string (optarg, parse_notice_option);
				break;
			case OPT_BUG:
				parse_args_string (optarg, parse_bug_option);
				break;
			case OPT_CPP:				// --cpp=
				cpp_name = save_string (optarg);
				break;
			case 'S':					// save temps
				options.save_temps = true;
				break;
			case 'D':					// defines for cpp
				cpp_define (optarg);
				break;
			case 'E':					// defines for cpp
				saw_E = 1;
				options.preprocess_only = 1;
				add_cpp_def ("-E");
				break;
			case OPT_INCLUDE:			// include-file
				add_cpp_def (nva ("%s", "-include"));
				add_cpp_def (nva ("%s", optarg));
				break;
			case 'i':					// includes
				{
					int o = cpp_include (optarg, argv[optind]);
					if (o < 0) {
						usage (1);
					} else {
						optind += o;
					}
				}
				break;
			case 'I':					// includes
				cpp_include ("I", optarg);
				break;
			case 'U':					// undefines
				cpp_undefine (optarg);
				break;
			case 'M':
				{
					if (optarg && strchr (optarg, 'D')) {
						saw_MD = 1;
					}
					int o = cpp_depend (optarg, argv[optind]);
					if (o < 0) {
						usage (1);
					} else {
						optind += o;
					}
				}
				break;
			case OPT_NO_DEFAULT_PATHS:
				options.no_default_paths = 1;
				break;
			default:
				usage (1);
		}
	}
	if (!cpp_name)
		options.single_cpp = 0;
	if (saw_E && saw_MD) {
		fprintf (stderr, "%s: cannot use -E and -MD together\n", this_program);
		exit (1);
	}
	if (saw_MD) {
		options.preprocess_only = 0;
	}
	options.preprocess_ouput = saw_E;
	if (!source_files && !options.advanced) {
		// progs.src mode without --advanced or --ruamoko implies --traditional
		// but --extended overrides
		if (!options.traditional)
			options.traditional = 2;
		options.advanced = false;
		if (!options.code.progsversion) {
			options.code.progsversion = PROG_ID_VERSION;
		}
		if (!options_user_set.code.ifstring) {
			options.code.ifstring = false;
		}
		if (!options_user_set.code.short_circuit) {
			options.code.short_circuit = false;
		}
		if (!options_user_set.code.local_merging) {
			options.code.local_merging = false;
		}
		if (!options_user_set.code.vector_components) {
			options.code.vector_components = true;
		}
		if (options.math.vector_mult == 0) {
			options.math.vector_mult = QC_DOT;
		}
	}
	if (!options.code.progsversion)
		options.code.progsversion = PROG_VERSION;
	if (!options.traditional) {
		// avanced=2 requires the Ruamoko ISA
		options.advanced = 2 - (options.code.progsversion < PROG_VERSION);
		const char *ruamoko = va (0, "__RUAMOKO__=%d", options.advanced);
		cpp_define (ruamoko);
		if (!options_user_set.code.ifstring) {
			options.code.ifstring = false;
		}
		if (!options_user_set.code.short_circuit) {
			options.code.short_circuit = true;
		}
		if (!options_user_set.code.local_merging) {
			options.code.local_merging = true;
		}
		if (!options_user_set.code.vector_components) {
			options.code.vector_components = false;
		}
		if (!options_user_set.math.vector_mult == 0) {
			options.math.vector_mult = options.advanced == 1 ? QC_DOT
															 : QC_HADAMARD;
		}
	} else {
		options.code.promote_float = false;
	}
	if (options.code.progsversion == PROG_ID_VERSION) {
		options.code.promote_float = false;
		cpp_define ("__VERSION6__=1");
		if (!options_user_set.code.crc) {
			options.code.crc = true;
		}
	} else {
		if (!options_user_set.code.crc) {
			options.code.crc = false;
		}
	}

	if (options.traditional && options.advanced) {
		fprintf (stderr,
				 "%s: internal error: traditional and advanced twisted\n",
				 this_program);
		abort ();
	}

	// add the default paths
	if (!options.no_default_paths) {
		cpp_include ("system", QFCC_INCLUDE_PATH);
		linker_add_path (QFCC_LIB_PATH);
	}

	if (options.verbosity >= 3) {
		pre_yydebug = 1;
		qc_yydebug = 1;
		qp_yydebug = 1;
	}
	return optind;
}
