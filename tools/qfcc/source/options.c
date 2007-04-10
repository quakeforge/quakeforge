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

static __attribute__ ((used)) const char rcsid[] =
	"$Id$";

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

const char *this_program;
const char **source_files;
static int num_files;
static int files_size;

static struct option const long_options[] = {
	{"output-file", required_argument, 0, 'o'},
	{"source", required_argument, 0, 's'},
	{"progs-src", required_argument, 0, 'P'},
	{"save-temps", no_argument, 0, 'S'},
	{"quiet", no_argument, 0, 'q'},
	{"verbose", no_argument, 0, 'v'},
	{"code", required_argument, 0, 'C'},
	{"warn", required_argument, 0, 'W'},
	{"help", no_argument, 0, 'h'},
	{"version", no_argument, 0, 'V'},
	{"files", no_argument, 0, 'F'},
	{"traditional", no_argument, 0, 257},
	{"advanced", no_argument, 0, 258},
	{"strip-path", required_argument, 0, 'p'},
	{"define", required_argument, 0, 'D'},
	{"include", required_argument, 0, 259},
	{"undefine", required_argument, 0, 'U'},
	{"cpp", required_argument, 0, 256},
	{"notice", required_argument, 0, 'N'},
	{NULL, 0, NULL, 0}
};

static const char *short_options =
	"-"		// magic option parsing mode doohicky (must come first)
	"C:"	// code options
	"c"		// separate compilation
	"D:"	// define
	"E"		// preprocess only
	"F"		// generate files.dat
	"g"		// debug
	"h"		// help
	"I:"	// set includes
	"l:"	// lib file
	"L:"	// lib path
	"M::"
	"N:"	// notice options
	"o:"	// output file
	"P:"	// progs.src name
	"p:"	// strip path
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
"    -C, --code OPTION,...     Set code generation options\n"
"    -c                        Compile only, don't link\n"
"        --cpp CPPSPEC         cpp execution command line\n"
"    -D, --define SYMBOL[=VAL] Define symbols for the preprocessor\n"
"    -E                        Preprocess only\n"
"    -F, --files               Generate files.dat\n"
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
"    -N, --notice OPTION,...   Set notice options\n"
"    -o, --output-file FILE    Specify output file name\n"
"    -P, --progs-src FILE      File to use instead of progs.src\n"
"    -p, --strip-path NUM      Strip NUM leading path elements from file\n"
"                              names\n"
"    -q, --quiet               Inhibit usual output\n"
"    -r                        Incremental linking\n"
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
"For help on options for --code and --warn, see the qfcc(1) manual page\n"
	);
	exit (status);
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

	add_cpp_def ("-D__QFCC__=1");
	add_cpp_def ("-D__QUAKEC__=1");

	options.code.short_circuit = -1;
	options.code.local_merging = -1;
	options.code.fast_float = true;
	options.warnings.uninited_variable = true;
	options.warnings.unused = true;
	options.warnings.executable = true;
	options.warnings.traditional = true;
	options.warnings.precedence = true;
	options.warnings.initializer = true;
	options.warnings.unimplemented = true;

	options.single_cpp = true;
	options.save_temps = false;
	options.verbosity = 0;
	options.strip_path = 0;

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
				printf ("%s version %s\n", PACKAGE, VERSION);
				exit (0);
				break;
			case 's':					// src dir
				sourcedir = save_string (NORMALIZE (optarg));
				break;
			case 'P':					// progs-src
				progs_src = save_string (NORMALIZE (optarg));
				break;
			case 'p':
				options.strip_path = atoi (optarg);
				break;
			case 'F':
				options.files_dat = true;
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
			case 257:					// traditional
				options.traditional = true;
				options.advanced = false;
				options.code.progsversion = PROG_ID_VERSION;
				break;
			case 258:					// advanced
				options.traditional = false;
				options.advanced = true;
				options.code.progsversion = PROG_VERSION;
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
						} else if (!(strcasecmp (temp, "debug"))) {
							options.code.debug = flag;
						} else if (!(strcasecmp (temp, "fast-float"))) {
							options.code.fast_float = flag;
						} else if (!(strcasecmp (temp, "local-merging"))) {
							options.code.local_merging = flag;
						} else if (!(strcasecmp (temp, "short-circuit"))) {
							options.code.short_circuit = flag;
						} else if (!(strcasecmp (temp, "single-cpp"))) {
							options.single_cpp = flag;
						} else if (!(strcasecmp (temp, "vector-calls"))) {
							options.code.vector_calls = flag;
						} else if (!(strcasecmp (temp, "v6only"))) {
							if (flag)
								options.code.progsversion = PROG_ID_VERSION;
							else
								options.code.progsversion = PROG_VERSION;
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
							} else if (!strcasecmp (temp, "initializer")) {
								options.warnings.initializer = flag;
							} else if (!strcasecmp (temp, "integer-divide")) {
								options.warnings.integer_divide = flag;
							} else if (!strcasecmp (temp, "interface-check")) {
								options.warnings.interface_check = flag;
							} else if (!strcasecmp (temp, "precedence")) {
								options.warnings.precedence = flag;
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
						if (!(strcasecmp (temp, "none"))) {
							options.notices.silent = true;
						} else if (!(strcasecmp (temp, "warn"))) {
							options.notices.promote = true;
						}
						temp = strtok (NULL, ",");
					}
					free (opts);
				}
				break;
			case 256:					// --cpp=
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
			case 259:					// include-file
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
		options.traditional = true;
		options.advanced = false;
		if (!options.code.progsversion)
			options.code.progsversion = PROG_ID_VERSION;
		if (options.code.short_circuit == (qboolean) -1)
			options.code.short_circuit = false;
		if (options.code.local_merging == (qboolean) -1)
			options.code.local_merging = false;
	}
	if (!options.code.progsversion)
		options.code.progsversion = PROG_VERSION;
	if (!options.traditional) {
		options.advanced = true;
		add_cpp_def ("-D__RUAMOKO__=1");
		add_cpp_def ("-D__RAUMOKO__=1");
		if (options.code.short_circuit == (qboolean) -1)
			options.code.short_circuit = true;
		if (options.code.local_merging == (qboolean) -1)
			options.code.local_merging = true;
	}
	if (options.code.progsversion == PROG_ID_VERSION)
		add_cpp_def ("-D__VERSION6__=1");

	// add the default paths
	add_cpp_def (nva ("-I%s", QFCC_INCLUDE_PATH));
	linker_add_path (QFCC_LIB_PATH);

	if (options.verbosity >= 3)
		yydebug = 1;
	return optind;
}
