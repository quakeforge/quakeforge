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

/*
	setmodel

	Sets the model name for entity e to string m.
	Set the entity's move type and solid type before calling this.
*/
@extern void (entity e, string m) setmodel;

/*
	setorigin

	Sets origin for entity e to vector o.
*/
@extern void (entity e, vector o) setorigin;

/*
	setsize

	Set the size of entity e to a cube with the bounds ( x1 y1 z1 ) ( x2 y2 z2 )
*/
@extern void (entity e, vector min, vector max) setsize;

/*
	spawn

	Creates a new entity and returns it.
*/
@extern entity () spawn;

/*
	remove

	Remove entity e.
*/
@extern void (entity e) remove;

@extern entity (entity start, .string fld, string match) find;
@extern entity (vector org, float rad) findradius;
@extern entity (entity e) nextent;

/*
	makestatic

	Make entity e static (part of the world).
	Static entities do not interact with the game.
*/
@extern void (entity e) makestatic;

@extern void (entity e) setspawnparms;

#endif //__ruamoko_entities_h
