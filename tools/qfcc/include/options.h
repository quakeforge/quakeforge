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
	qboolean	cow;				// Turn constants into variables if written to
	qboolean	crc;				// Write progsdef.h crc to progs.dat
	qboolean	debug;				// Generate debug info for the engine
	qboolean	short_circuit;		// short circuit logic for && and ||
	qboolean    optimize;			// perform optimizations
	qboolean	fast_float;			// use floats directly in ifs
	qboolean    vector_calls;		// use floats instead of vectors for constant function args
	qboolean    local_merging;		// merge function locals into one block
	unsigned    progsversion;		// Progs version to generate code for
	qboolean    vector_components;	// add *_[xyz] symbols for vectors
	qboolean    ifstring;			// expand if (str) to if (str != "")
	qboolean    const_initializers;	// initialied globals are constant
	qboolean    promote_float;		// promote float through ...
} code_options_t;

typedef struct {
	qboolean	promote;			// Promote warnings to errors
	qboolean	cow;				// Warn on copy-on-write detection
	qboolean	undefined_function;	// Warn on undefined function use
	qboolean	uninited_variable;	// Warn on use of uninitialized vars
	qboolean	vararg_integer;		// Warn on passing an integer to vararg func
	qboolean	integer_divide;		// Warn on integer constant division
	qboolean	interface_check;	// Warn for methods not in interface
	qboolean	unused;				// Warn on unused local variables
	qboolean	executable;			// Warn on expressions with no effect
	qboolean	traditional;		// Warn on bogus constructs allowed by qcc
	qboolean	precedence;			// Warn on precedence issues
	qboolean	initializer;		// Warn on excessive initializer elements
	qboolean	unimplemented;		// Warn on unimplemented class methods
	qboolean	redeclared;			// Warn on redeclared local variables
	qboolean	enum_switch;		// Warn on unhandled enum values in switch
} warn_options_t;

typedef struct {
	qboolean	promote;			// Promote notices to warnings
	qboolean	silent;				// don't even bother (overrides promote)
} notice_options_t;

typedef struct {
	qboolean	promote;			// Promote bugs to internal errors
	qboolean	silent;				// don't even bother (overrides promote)
} bug_options_t;

typedef struct {
	qboolean    initial;
	qboolean    thread;
	qboolean    dead;
	qboolean    final;
	qboolean    dags;
	qboolean    expr;
	qboolean    statements;
	qboolean    reaching;
	qboolean    live;
	qboolean    flow;
	qboolean    post;
} blockdot_options_t;

typedef struct {
	code_options_t	code;			// Code generation options
	warn_options_t	warnings;		// Warning options
	notice_options_t notices;		// Notice options
	bug_options_t   bug;			// Bug options
	blockdot_options_t block_dot;	// Statement block flow diagrams

	int				verbosity;		// 0=silent, goes up to 2 currently
	qboolean		single_cpp;		// process progs.src into a series of
									// #include directives and then compile
									// that
	qboolean		no_default_paths;	// no default -I or -L
	qboolean		save_temps;		// save temporary files
	qboolean		files_dat;		// generate files.dat
	qboolean		frames_files;	// generate <basename>.frame files
	qboolean		progdefs_h;		// generate progdefs.h
	qboolean		qccx_escapes;	// use qccx escapes instead of standard C
	int				traditional;	// behave more like qcc
	qboolean		advanced;		// behold the power of Ruamoko
	qboolean		compile;		// serparate compilation mode
	qboolean		partial_link;	// partial linking
	qboolean		preprocess_only;// run only cpp, don't compile
	qboolean		gzip;			// compress qfo files when writing
	int				strip_path;		// number of leading path elements to strip
									// from source file names
	const char     *output_file;
	const char     *debug_file;
} options_t;

extern options_t options;
int DecodeArgs (int argc, char **argv);
extern const char *progs_src;
extern const char **source_files;
extern const char *this_program;
extern const char *sourcedir;

#endif//__options_h
