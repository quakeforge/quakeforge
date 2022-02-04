/**
	\defgroup debug Debugging Functions
	\{
*/

/*
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
*/

#ifndef __ruamoko_debug_h
#define __ruamoko_debug_h

/**
	Tell the engine to abort (stop) code processing.
	\note In QuakeC, this was break().
*/
@extern void abort (void);

/**
	Tell the engine to print all edicts (entities)
*/
@extern void coredump (void);

/**
	Enable instruction trace in the interpreter
*/
@extern void traceon (void);

/**
	traceoff

	Disable instruction trace in the interpreter
*/
@extern void traceoff (void);

/**
	Print all information on an entity to the console
*/
@extern void eprint (entity e);

/**
	Print a string to the console if the "developer" Cvar is nonzero.
*/
@extern void dprint (.../*string str*/);

/**
	Abort (crash) the server. "str" is the message the server crashes with.
*/
@extern void error (.../*string str*/);

/**
	Prints info on the "self" ENTITY (not object), and error message "e".
	The entity is freed.
*/
@extern void objerror (.../*string e*/);

//\}
#endif //__ruamoko_debug_h
