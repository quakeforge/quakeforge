/*
	entities.h

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2002 Colin Thompson

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

	$Id$
*/

#ifndef __entities_h
#define __entities_h

#define DEFAULTLIGHTLEVEL	300
#define DEFAULTFALLOFF 1.0f

typedef struct epair_s {
	struct epair_s *next;
	const char *key;
	const char *value;
} epair_t;

typedef struct entity_s {
	const char *classname;
	vec3_t      origin;
	vec_t       angle;
	int         light;
	// LordHavoc: added falloff (smaller fractions = bigger light area),
	// color, and lightradius (also subbrightness to implement lightradius)
	vec_t       falloff;
	vec_t       lightradius;
	vec_t       subbrightness;
	vec_t       lightoffset;
	vec3_t      color;
	vec3_t      spotdir;
	vec_t       spotcone;
	unsigned short visbyte, visbit;		// which byte and bit to look at in
										// the visdata for the leaf this light
										// is in
	int         style;
	const char *target;
	const char *targetname;
	struct epair_s *epairs;
	struct entity_s *targetent;
} entity_t;

extern entity_t *entities;
extern int num_entities;

const char *ValueForKey (entity_t *ent, const char *key);
void SetKeyValue (entity_t *ent, const char *key, const char *value);
float FloatForKey (entity_t *ent, const char *key);
entity_t *FindEntityWithKeyPair(const char *key, const char *value);
void GetVectorForKey (entity_t *ent, const char *key, vec3_t vec);

void LoadEntities (void);
void WriteEntitiesToString (void);

#endif// __entities_h
