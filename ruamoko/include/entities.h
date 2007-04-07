/*
	entities.h

	Entity function prototypes

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
#ifndef __ruamoko_entities_h
#define __ruamoko_entities_h

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
		setmodel

		Sets the model name for entity e to string m.
		Set the entity's move type and solid type before calling.
		SERVER ONLY
	*/
	BUILTIN (setmodel, #3, void, entity e, string m);

	/*
		setorigin

		Sets origin for entity e to vector o.
		SERVER ONLY
	*/
	BUILTIN (setorigin, #2, void, entity e, vector o);

	/*
		setsize

		Set the size of entity e to a cube with the bounds ( x1 y1 z1 ) ( x2 y2 z2 )
		SERVER ONLY
	*/
	BUILTIN (setsize, #4, void, entity e, vector min, vector max);

	/*
		spawn

		Creates a new entity and returns it.
		SERVER ONLY
	*/
	BUILTIN (spawn, #14, entity, void);

	/*
		remove

		Remove entity e.
		SERVER ONLY
	*/
	BUILTIN (remove, #15, void, entity e);

	/*
		find

		Search all entities for a field with contents matching a value and
		return the first matching entity.

		start:	the edict from which to start.
		field:	The field to search.
		match:	The contents to search for.

		This function must be called multiple times to get multiple results.
		Stupid, but functional.
	*/
#ifdef __VERSION6__
	BUILTIN (find, #18, entity, entity start, .string field, string match);
#else
	BUILTIN (find, #18, entity, entity start, ...);
#endif

	/*
		findradius

		Search for entities within radius of origin.

		If none found, world is returned.
		If >1 found, the next will be linked using the "chain" field.
		SERVER ONLY
	*/
	BUILTIN (findradius, #22, entity, vector origin, float radius);

	/*
		nextent

		Return next entity after e. Use for traversing all entities.
		Entity zero (the world) is returned if no more exist.
	*/
	BUILTIN (nextent, #47, entity, entity e);

	/*
		makestatic

		Make entity e static (part of the world).
		Static entities do not interact with the game.
		SERVER ONLY
	*/
	BUILTIN (makestatic, #69, void, entity e);

	BUILTIN (setspawnparms, #78, void, entity e);

	BUILTIN (EntityParseFunction, #0, void, void (string ent_data) func);

#ifndef __RUA_INTERNAL_IMPLEMENT
};
#endif	// __RUA_INTERNAL_IMPLEMENT

#endif //__ruamoko_entities_h
