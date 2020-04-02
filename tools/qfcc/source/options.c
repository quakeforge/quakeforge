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

#include "QF/pr_comp.h"
#include "QF/va.h"

#include "cpp.h"
#include "linker.h"
#include "options.h"
#include "qfcc.h"
#include "strpool.h"

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
	{"relocatable", no_argument, 0, 'r'},
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
"                              default for separate compilation mode\n"
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

static void
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
"    [no-]v6only             Restrict output code to version 6 progs\n"
"                            features.\n"
"\n"
"For details, see the qfcc(1) manual page\n"
	);
	exit (0);
}

static void
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
}

static void
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
}

static void
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

int
DecodeArgs (int argc, char **argv)
{
	int         c;
	int         saw_E = 0, saw_MD = 0;

	add_cpp_undef ("-undef");
	add_cpp_undef ("-nostdinc");
	add_cpp_def ("-D__QFCC__=1");
	add_cpp_def ("-D__QUAKEC__=1");

	options.code.short_circuit = -1;
	options.code.local_merging = -1;
	options.code.vector_components = -1;
	options.code.crc = -1;
	options.code.fast_float = true;
	options.code.promote_float = true;
	options.warnings.uninited_variable = true;
	options.warnings.unused = true;
	options.warnings.executable = true;
	options.warnings.traditional = true;
	options.warnings.precedence = true;
	options.warnings.initializer = true;
	options.warnings.unimplemented = true;
	options.warnings.redeclared = true;
	options.warnings.enum_switch = true;

	options.single_cpp = true;
	options.save_temps = false;
	options.verbosity = 0;

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
				add_file (va ("-l%s", NORMALIZE (optarg)));
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
				options.advanced = false;
				options.code.progsversion = PROG_ID_VERSION;
				options.code.const_initializers = true;
				break;
			case OPT_ADVANCED:
				options.traditional = 0;
				options.advanced = true;
				options.code.progsversion = PROG_VERSION;
				options.code.const_initializers = false;
				break;
			case OPT_BLOCK_DOT:
				if (optarg) {
					char       *opts = strdup (optarg);
					char       *temp = strtok (opts, ",");

					while (temp) {
						qboolean    flag = true;

						if (!strncasecmp (temp, "no-", 3)) {
							flag = false;
							temp += 3;
						}
						if (!strcasecmp (temp, "initial")) {
							options.block_dot.initial = flag;
						} else if (!(strcasecmp (temp, "thread"))) {
							options.block_dot.thread = flag;
						} else if (!(strcasecmp (temp, "dead"))) {
							options.block_dot.dead = flag;
						} else if (!(strcasecmp (temp, "final"))) {
							options.block_dot.final = flag;
						} else if (!(strcasecmp (temp, "dags"))) {
							options.block_dot.dags = flag;
						} else if (!(strcasecmp (temp, "expr"))) {
							options.block_dot.expr = flag;
						} else if (!(strcasecmp (temp, "flow"))) {
							options.block_dot.flow = flag;
						} else if (!(strcasecmp (temp, "reaching"))) {
							options.block_dot.reaching = flag;
						} else if (!(strcasecmp (temp, "statements"))) {
							options.block_dot.statements = flag;
						} else if (!(strcasecmp (temp, "live"))) {
							options.block_dot.live = flag;
						} else if (!(strcasecmp (temp, "post"))) {
							options.block_dot.post = flag;
						}
						temp = strtok (NULL, ",");
					}
					free (opts);
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
			case 'C':{					// code options
					char       *opts = strdup (optarg);
					char       *temp = strtok (opts, ",");

					while (temp) {
						qboolean    flag = true;

						if (!strncasecmp (temp, "no-", 3)) {
							flag = false;
							temp += 3;
						}
						if (!strcasecmp (temp, "cow")) {
							options.code.cow = flag;
						} else if (!(strcasecmp (temp, "cpp"))) {
							cpp_name = flag ? CPP_NAME : 0;
						} else if (!(strcasecmp (temp, "crc"))) {
							options.code.crc = flag;
						} else if (!(strcasecmp (temp, "debug"))) {
							options.code.debug = flag;
						} else if (!(strcasecmp (temp, "fast-float"))) {
							options.code.fast_float = flag;
						} else if (!(strcasecmp (temp, "promote-float"))) {
							options.code.promote_float = flag;
						} else if (!strcasecmp (temp, "help")) {
							code_usage ();
						} else if (!(strcasecmp (temp, "local-merging"))) {
							options.code.local_merging = flag;
						} else if (!(strcasecmp (temp, "optimize"))) {
							options.code.optimize = flag;
						} else if (!(strcasecmp (temp, "short-circuit"))) {
							options.code.short_circuit = flag;
						} else if (!(strcasecmp (temp, "ifstring"))) {
							options.code.ifstring = flag;
						} else if (!(strcasecmp (temp, "single-cpp"))) {
							options.single_cpp = flag;
						} else if (!(strcasecmp (temp, "vector-calls"))) {
							options.code.vector_calls = flag;
						} else if (!(strcasecmp (temp, "vector-components"))) {
							options.code.vector_components = flag;
						} else if (!(strcasecmp (temp, "v6only"))) {
							if (flag)
								options.code.progsversion = PROG_ID_VERSION;
							else
								options.code.progsversion = PROG_VERSION;
						} else if (!(strcasecmp (temp, "const-initializers"))) {
							options.code.const_initializers = flag;
						}
						temp = strtok (NULL, ",");
					}
					free (opts);
				}
				break;
			case 'W':{					// warning options
					char       *opts = strdup (optarg);
					char       *temp = strtok (opts, ",");

					while (temp) {
						if (!(strcasecmp (temp, "all"))) {
							options.warnings.cow = true;
							options.warnings.undefined_function = true;
							options.warnings.uninited_variable = true;
							options.warnings.vararg_integer = true;
							options.warnings.integer_divide = true;
							options.warnings.interface_check = true;
							options.warnings.unused = true;
							options.warnings.executable = true;
							options.warnings.traditional = true;
							options.warnings.precedence = true;
							options.warnings.initializer = true;
							options.warnings.unimplemented = true;
							options.warnings.redeclared = true;
							options.warnings.enum_switch = true;
						} else if (!(strcasecmp (temp, "none"))) {
							options.warnings.cow = false;
							options.warnings.undefined_function = false;
							options.warnings.uninited_variable = false;
							options.warnings.vararg_integer = false;
							options.warnings.integer_divide = false;
							options.warnings.interface_check = false;
							options.warnings.unused = false;
							options.warnings.executable = false;
							options.warnings.traditional = false;
							options.warnings.precedence = false;
							options.warnings.initializer = false;
							options.warnings.unimplemented = false;
							options.warnings.redeclared = false;
							options.warnings.enum_switch = false;
						} else {
							qboolean    flag = true;

							if (!strncasecmp (temp, "no-", 3)) {
								flag = false;
								temp += 3;
							}
							if (!(strcasecmp (temp, "cow"))) {
								options.warnings.cow = flag;
							} else if (!strcasecmp (temp, "error")) {
								options.warnings.promote = flag;
							} else if (!strcasecmp (temp, "executable")) {
								options.warnings.executable = flag;
							} else if (!strcasecmp (temp, "help")) {
								warning_usage ();
							} else if (!strcasecmp (temp, "initializer")) {
								options.warnings.initializer = flag;
							} else if (!strcasecmp (temp, "integer-divide")) {
								options.warnings.integer_divide = flag;
							} else if (!strcasecmp (temp, "interface-check")) {
								options.warnings.interface_check = flag;
							} else if (!strcasecmp (temp, "precedence")) {
								options.warnings.precedence = flag;
							} else if (!strcasecmp (temp, "redeclared")) {
								options.warnings.redeclared = flag;
							} else if (!strcasecmp (temp, "switch")) {
								options.warnings.enum_switch = flag;
							} else if (!strcasecmp (temp, "traditional")) {
								options.warnings.traditional = flag;
							} else if (!strcasecmp (temp, "undef-function")) {
								options.warnings.undefined_function = flag;
							} else if (!strcasecmp (temp, "unimplemented")) {
								options.warnings.unimplemented = flag;
							} else if (!strcasecmp (temp, "unused")) {
								options.warnings.unused = flag;
							} else if (!strcasecmp (temp, "uninited-var")) {
								options.warnings.uninited_variable = flag;
							} else if (!strcasecmp (temp, "vararg-integer")) {
								options.warnings.vararg_integer = flag;
							}
						}
						temp = strtok (NULL, ",");
					}
					free (opts);
				}
				break;
			case 'N':{					// notice options
					char       *opts = strdup (optarg);
					char       *temp = strtok (opts, ",");

					while (temp) {
						if (!strcasecmp (temp, "help")) {
							notice_usage ();
						} else if (!(strcasecmp (temp, "none"))) {
							options.notices.silent = true;
						} else if (!(strcasecmp (temp, "warn"))) {
							options.notices.promote = true;
						}
						temp = strtok (NULL, ",");
					}
					free (opts);
				}
				break;
			case OPT_BUG:{
					char       *opts = strdup (optarg);
					char       *temp = strtok (opts, ",");

					while (temp) {
						if (!strcasecmp (temp, "help")) {
							bug_usage ();
						} else if (!(strcasecmp (temp, "none"))) {
							options.bug.silent = true;
						} else if (!(strcasecmp (temp, "die"))) {
							options.bug.promote = true;
						}
						temp = strtok (NULL, ",");
					}
					free (opts);
				}
				break;
			case OPT_CPP:				// --cpp=
				cpp_name = save_string (optarg);
				break;
			case 'S':					// save temps
				options.save_temps = true;
				break;
			case 'D':					// defines for cpp
				add_cpp_def (nva ("%s%s", "-D", optarg));
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
			case 'I':					// includes
				add_cpp_def (nva ("%s%s", "-I", optarg));
				break;
			case 'U':					// undefines
				add_cpp_def (nva ("%s%s", "-U", optarg));
				break;
			case 'M':
				options.preprocess_only = 1;
				if (optarg) {
					add_cpp_def (nva ("-M%s", optarg));
					if (strchr (optarg, 'D'))
						saw_MD = 1;
					if (strchr ("FQT", optarg[0]))
						add_cpp_def (argv[optind++]);
				} else {
					options.preprocess_only = 1;
					add_cpp_def (nva ("-M"));
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
	if (saw_MD)
		options.preprocess_only = 0;
	if (!source_files && !options.advanced) {
		// progs.src mode without --advanced implies --traditional
		// but --extended overrides
		if (!options.traditional)
			options.traditional = 2;
		options.advanced = false;
		if (!options.code.progsversion)
			options.code.progsversion = PROG_ID_VERSION;
		if (options.code.ifstring == (qboolean) -1)
			options.code.ifstring = false;
		if (options.code.short_circuit == (qboolean) -1)
			options.code.short_circuit = false;
		if (options.code.local_merging == (qboolean) -1)
			options.code.local_merging = false;
		if (options.code.vector_components == (qboolean) -1)
			options.code.vector_components = true;
	}
	if (!options.code.progsversion)
		options.code.progsversion = PROG_VERSION;
	if (!options.traditional) {
		options.advanced = true;
		add_cpp_def ("-D__RUAMOKO__=1");
		add_cpp_def ("-D__RAUMOKO__=1");
		if (options.code.ifstring == (qboolean) -1)
			options.code.ifstring = false;
		if (options.code.short_circuit == (qboolean) -1)
			options.code.short_circuit = true;
		if (options.code.local_merging == (qboolean) -1)
			options.code.local_merging = true;
		if (options.code.vector_components == (qboolean) -1)
			options.code.vector_components = false;
	} else {
		options.code.promote_float = 0;
	}
	if (options.code.progsversion == PROG_ID_VERSION) {
		options.code.promote_float = 0;
		add_cpp_def ("-D__VERSION6__=1");
		if (options.code.crc == (qboolean) -1)
			options.code.crc = true;
	} else {
		if (options.code.crc == (qboolean) -1)
			options.code.crc = false;
	}

	// add the default paths
	if (!options.no_default_paths) {
		add_cpp_sysinc ("-isystem");
		add_cpp_sysinc (QFCC_INCLUDE_PATH);
		linker_add_path (QFCC_LIB_PATH);
	}

	if (options.verbosity >= 3) {
		qc_yydebug = 1;
		qp_yydebug = 1;
	}
	return optind;
}
