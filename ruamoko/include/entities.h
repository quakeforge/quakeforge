/**
	\defgroup entities Entity Handling
	\{

	Built-in functions for dealing with Quake entities.
	These builtin functions create, modify, delete, etc. Quake entities.
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
#ifndef __ruamoko_entities_h
#define __ruamoko_entities_h

///\name Server-only entity functions
///\{
/**
	Sets the model name for %entity \a e to string \a m.
	Set the entity's move type and solid type before calling.
*/
@extern void setmodel (entity e, string m);

/**
	Sets origin for %entity \a e to vector \a o.
*/
@extern void setorigin (entity e, vector o);

/**
	Set the size of %entity \a e to a cube with the bounds
	( xmin ymin zmin ) ( xmax ymax zmax )
*/
@extern void setsize (entity e, vector min, vector max);

/**
	Creates a new %entity and returns it.
*/
@extern entity spawn (void);

/**
	Remove %entity e.
*/
@extern void remove (entity e);

/**
	If none found, world is returned.
	If >1 found, the next will be linked using the "chain" field.
*/
@extern entity findradius (vector origin, float radius);

/**
	Make %entity e static (part of the world).
	Static entities do not interact with the game.
*/
@extern void makestatic (entity e);

@extern void setspawnparms (entity e);
//\}

///\name Client/Server entity functions
///\{
#ifdef __VERSION6__
/**
	Search all entities for a field with contents matching a value and
	return the first matching entity.

	\param start	the edict from which to start.
	\param field	The field to search.
	\param match	The contents to search for.

	This function must be called multiple times to get multiple results.
	Stupid, but functional.
*/
@extern entity find (entity start, .string field, string match);
#else
/**
	Search all entities for a field with contents matching a value and
	return the first matching entity.

	\param start	the edict from which to start.

	This function must be called multiple times to get multiple results.
	Stupid, but functional.
*/
@extern @attribute(no_va_list) entity find (entity start, ...);
#endif

/**
	Return next entity after e. Use for traversing all entities.
	%Entity zero (the world) is returned if no more exist.
*/
@extern entity nextent (entity e);

@extern void EntityParseFunction (void func (string ent_data));

//\}
//\}
#endif //__ruamoko_entities_h
