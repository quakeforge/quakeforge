/*
	options.c

	command line options handlnig

	Copyright (C) 2001 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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
#include <stdlib.h>
#include <stdio.h>

#include <getopt.h>

#include "QF/pr_comp.h"
#include "QF/va.h"

#include "cpp.h"
#include "options.h"

const char *this_program;

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
	{"traditional", no_argument, 0, 't'},
	{"strip-path", required_argument, 0, 'p'},
	{"define", required_argument, 0, 'D'},
	{"include", required_argument, 0, 'I'},
	{"undefine", required_argument, 0, 'U'},
	{"cpp", required_argument, 0, 256},
	{"notice", required_argument, 0, 'N'},
	{NULL, 0, NULL, 0}
};

static const char *short_options =
	"o:"	// output file
	"c"		// separate compilation
	"s:"	// source dir
	"P:"	// progs.src name
	"F"		// generate files.dat
	"q"		// quiet
	"v"		// verbose
	"g"		// debug
	"C:"	// code options
	"W:"	// warning options
	"h"		// help
	"V"		// version
	"p:"	// strip path
	"S"		// save temps
	"D:"	// define
	"I:"	// set includes
	"U:"	// undefine
	"N:"	// notice options
	;

static void
usage (int status)
{
	printf ("%s - QuakeForge Code Compiler\n", this_program);
	printf ("Usage: %s [options]\n", this_program);
	printf (
"Options:\n"
"    -s, --source DIR          Look for progs.src in DIR instead of \".\"\n"
"    -q, --quiet               Inhibit usual output\n"
"    -v, --verbose             Display more output than usual\n"
"    -g,                       Generate debuggin info\n"
"    -C, --code OPTION,...     Set code generation options\n"
"    -W, --warn OPTION,...     Set warning options\n"
"    -N, --notice OPTION,...   Set notice options\n"
"    -h, --help                Display this help and exit\n"
"    -V, --version             Output version information and exit\n\n"
"    -S, --save-temps          Do not delete temporary files\n"
"    -D, --define SYMBOL[=VAL],...  Define symbols for the preprocessor\n"
"    -I, --include DIR,...          Set directories for the preprocessor \n"
"                                   to search for #includes\n"
"    -U, --undefine SYMBOL,...      Undefine preprocessor symbols\n\n"
"For help on options for --code and --warn, see the qfcc(1) manual page\n"
	);
	exit (status);
}

int
DecodeArgs (int argc, char **argv)
{
	int         c;

	add_cpp_def ("-D__QFCC__=1");
	add_cpp_def ("-D__QUAKEC__=1");
	add_cpp_def ("-D__RUAMOKO__=1");
	add_cpp_def ("-D__RAUMOKO__=1");

	options.code.progsversion = PROG_VERSION;
	options.warnings.uninited_variable = true;

	options.save_temps = false;
	options.verbosity = 0;
	options.strip_path = 0;

	sourcedir = "";
	progs_src = "progs.src";

	while ((c = getopt_long (argc, argv, short_options, long_options, 0))
		   != EOF) {
		switch (c) {
			case 'h':					// help
				usage (0);
				break;
			case 'V':					// version
				printf ("%s version %s\n", PACKAGE, VERSION);
				exit (0);
				break;
			case 's':					// src dir
				sourcedir = strdup (optarg);
				break;
			case 'P':					// progs-src
				progs_src = strdup (optarg);
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
			case 't':					// traditional
				options.traditional = true;
				options.code.progsversion = PROG_ID_VERSION;
				break;
			case 'c':					// traditional
				options.compile = true;
				break;
			case 'C':{					// code options
					char       *opts = strdup (optarg);
					char       *temp = strtok (opts, ",");

					while (temp) {
						if (!(strcasecmp (temp, "cow"))) {
							options.code.cow = true;
						} else if (!(strcasecmp (temp, "no-cow"))) {
							options.code.cow = false;
						} else if (!(strcasecmp (temp, "no-cpp"))) {
							cpp_name = 0;
						} else if (!(strcasecmp (temp, "debug"))) {
							options.code.debug = true;
						} else if (!(strcasecmp (temp, "no-debug"))) {
							options.code.debug = false;
						} else if (!(strcasecmp (temp, "v6only"))) {
							options.code.progsversion = PROG_ID_VERSION;
							add_cpp_def ("-D__VERSION6__=1");
						} else if (!(strcasecmp (temp, "no-v6only"))) {
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
						} else if (!(strcasecmp (temp, "none"))) {
							options.warnings.cow = false;
							options.warnings.undefined_function = false;
							options.warnings.uninited_variable = false;
							options.warnings.vararg_integer = false;
							options.warnings.integer_divide = false;
						} else if (!(strcasecmp (temp, "cow"))) {
							options.warnings.cow = true;
						} else if (!(strcasecmp (temp, "no-cow"))) {
							options.warnings.cow = false;
						} else if (!(strcasecmp (temp, "error"))) {
							options.warnings.promote = true;
						} else if (!(strcasecmp (temp, "no-error"))) {
							options.warnings.promote = false;
						} else if (!(strcasecmp (temp, "undef-function"))) {
							options.warnings.undefined_function = true;
						} else if (!(strcasecmp (temp, "no-undef-function"))) {
							options.warnings.undefined_function = false;
						} else if (!(strcasecmp (temp, "uninited-var"))) {
							options.warnings.uninited_variable = true;
						} else if (!(strcasecmp (temp, "no-uninited-var"))) {
							options.warnings.uninited_variable = false;
						} else if (!(strcasecmp (temp, "vararg-integer"))) {
							options.warnings.vararg_integer = true;
						} else if (!(strcasecmp (temp, "no-vararg-integer"))) {
							options.warnings.vararg_integer = false;
						} else if (!(strcasecmp (temp, "integer-divide"))) {
							options.warnings.integer_divide = true;
						} else if (!(strcasecmp (temp, "no-integer-divide"))) {
							options.warnings.integer_divide = false;
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
				cpp_name = strdup (optarg);
				break;
			case 'S':					// save temps
				options.save_temps = true;
				break;
			case 'D':{					// defines for cpp
					char       *opts = strdup (optarg);
					char       *temp = strtok (opts, ",");

					while (temp) {
						add_cpp_def (nva ("%s%s", "-D", temp));
						temp = strtok (NULL, ",");
					}
					free (opts);
				}
				break;
			case 'I':{					// includes
					char       *opts = strdup (optarg);
					char       *temp = strtok (opts, ",");

					while (temp) {
						add_cpp_def (nva ("%s%s", "-I", temp));
						temp = strtok (NULL, ",");
					}
					free (opts);
				}
				break;
			case 'U':{					// undefines
					char       *opts = strdup (optarg);
					char       *temp = strtok (opts, ",");

					while (temp) {
						add_cpp_def (nva ("%s%s", "-U", temp));
						temp = strtok (NULL, ",");
					}
					free (opts);
				}
				break;
			default:
				usage (1);
		}
	}
	return optind;
}
