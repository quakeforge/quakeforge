/*
	debug.h

	Debugging function definitions

	Copyright (C) 2002 Bill Currie <taniwha@quakeforge.net>
	Copyright (C) 2002 Jeff Teunissen <deek@quakeforge.net>

	This file is part of the Ruamoko Standard Library.

	This library is free software; you can redistribute it and/or modify it
	under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation; either version 2.1 of the License, or (at
	your option) any later version.

	This library is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id$
*/
#ifndef __ruamoko_debug_h
#define __ruamoko_debug_h

#ifdef __RUA_INTERNAL_IMPLEMENT
# undef BUILTIN
# define BUILTIN(name, number, rettype, ...) \
	rettype (__VA_ARGS__) name = number
#else
# undef BUILTIN
# define BUILTIN(name, number, rettype, ...) \
	rettype (__VA_ARGS__) name
@extern {
#endif	// __RUA_INTERNAL_IMPLEMENT

	/*
		abort (in QuakeC, this was break)

		Tell the engine to abort (stop) code processing.
	*/
	BUILTIN (abort, #6, void, void);

	/*
		coredump

		Tell the engine to print all edicts (entities)
	*/
	BUILTIN (coredump, #28, void, void);

	/*
		traceon

		Enable instruction trace in the interpreter
	*/
	BUILTIN (traceon, #29, void, void);

	/*
		traceoff

		Disable instruction trace in the interpreter
	*/
	BUILTIN (traceoff, #30, void, void);

	/*
		eprint

		Print all information on an entity to the console
	*/
	BUILTIN (eprint, #31, void, entity e);

	/*
		dprint

		Print a string to the console if the "developer" Cvar is nonzero.
	*/
	BUILTIN (dprint, #25, void, string str);

	/*
		error

		Abort (crash) the server. "str" is the message the server crashes with.
	*/
	BUILTIN (error, #10, void, string str);

	/*
		objerror

		Prints info on the "self" ENTITY (not object), and error message "e".
		The entity is freed.
	*/
	BUILTIN (objerror, #10, void, string e);

#ifndef __RUA_INTERNAL_IMPLEMENT
};
#endif	// __RUA_INTERNAL_IMPLEMENT

#endif //__ruamoko_debug_h
