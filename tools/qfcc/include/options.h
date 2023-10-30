/*
	options.h

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

#ifndef __options_h
#define __options_h

#include "QF/qtypes.h"

typedef struct {
	bool        cow;				// Turn constants into variables if written to
	bool        crc;				// Write progsdef.h crc to progs.dat
	bool        debug;				// Generate debug info for the engine
	bool        short_circuit;		// short circuit logic for && and ||
	bool        optimize;			// perform optimizations
	bool        fast_float;			// use floats directly in ifs
	bool        vector_calls;		// use floats instead of vectors for constant function args
	bool        local_merging;		// merge function locals into one block
	unsigned    progsversion;		// Progs version to generate code for
	bool        vector_components;	// add *_[xyz] symbols for vectors
	bool        ifstring;			// expand if (str) to if (str != "")
	bool        const_initializers;	// initialied globals are constant
	bool        promote_float;		// promote float through ...
	bool        commute_float_add;	// allow fp addition to commute
	bool        commute_float_mul;	// allow fp multiplication to commute
	bool        commute_float_dot;	// allow fp dot product to commute
	bool        assoc_float_add;	// allow fp addition to be associative
	bool        assoc_float_mul;	// allow fp multiplication to be associative

	bool        help;
} code_options_t;

typedef struct {
	int         vector_mult;		// operation for vector * vector

	bool        help;
} math_options_t;

typedef struct {
	bool        promote;			// Promote warnings to errors
	bool        cow;				// Warn on copy-on-write detection
	bool        undefined_function;	// Warn on undefined function use
	bool        uninited_variable;	// Warn on use of uninitialized vars
	bool        vararg_integer;		// Warn on passing an integer to vararg func
	bool        integer_divide;		// Warn on integer constant division
	bool        interface_check;	// Warn for methods not in interface
	bool        unused;				// Warn on unused local variables
	bool        executable;			// Warn on expressions with no effect
	bool        traditional;		// Warn on bogus constructs allowed by qcc
	bool        precedence;			// Warn on precedence issues
	bool        initializer;		// Warn on excessive initializer elements
	bool        unimplemented;		// Warn on unimplemented class methods
	bool        redeclared;			// Warn on redeclared local variables
	bool        enum_switch;		// Warn on unhandled enum values in switch

	bool        help;
} warn_options_t;

typedef struct {
	bool        promote;			// Promote notices to warnings
	bool        silent;				// don't even bother (overrides promote)

	bool        help;
} notice_options_t;

typedef struct {
	bool        promote;			// Promote bugs to internal errors
	bool        silent;				// don't even bother (overrides promote)

	bool        help;
} bug_options_t;

typedef struct {
	bool        initial;
	bool        thread;
	bool        dead;
	bool        final;
	bool        dags;
	bool        expr;
	bool        statements;
	bool        reaching;
	bool        live;
	bool        flow;
	bool        post;

	bool        help;
} blockdot_options_t;

typedef struct {
	code_options_t code;			// Code generation options
	math_options_t math;			// Various math options
	warn_options_t warnings;		// Warning options
	notice_options_t notices;		// Notice options
	bug_options_t bug;				// Bug options
	blockdot_options_t block_dot;	// Statement block flow diagrams

	int         verbosity;			// 0=silent, goes up to 2 currently
	bool        single_cpp;			// process progs.src into a series of
									// #include directives and then compile
									// that
	bool        no_default_paths;	// no default -I or -L
	bool        save_temps;			// save temporary files
	bool        files_dat;			// generate files.dat
	bool        frames_files;		// generate <basename>.frame files
	bool        progdefs_h;			// generate progdefs.h
	bool        qccx_escapes;		// use qccx escapes instead of standard C
	int         traditional;		// behave more like qcc
	int         advanced;			// behold the power of Ruamoko
	bool        compile;			// serparate compilation mode
	bool        partial_link;		// partial linking
	bool        preprocess_only;	// run only cpp, don't compile
	bool        preprocess_ouput;	// emit preprocessor output
	bool        dependencies;		// generate dependency rules
	bool        gzip;				// compress qfo files when writing
	const char *output_file;
	const char *debug_file;
} options_t;

extern options_t options;
int DecodeArgs (int argc, char **argv);
bool parse_warning_option (const char *opt);
bool parse_code_option (const char *opt);
extern const char *progs_src;
extern const char **source_files;
extern const char *this_program;
extern const char *sourcedir;

#endif//__options_h
