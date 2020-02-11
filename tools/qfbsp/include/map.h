/*
	Copyright (C) 1996-1997  Id Software, Inc.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

	See file, 'COPYING', for details.

*/

#ifndef qfbsp_map_h
#define qfbsp_map_h

#include "bsp5.h"

/**	\defgroup qfbsp_map Map Parser
	\ingroup qfbsp
*/
///@{

#define MAX_FACES 256
typedef struct mface_s {
	struct mface_s *next;
	plane_t     plane;
	int         texinfo;
} mface_t;

typedef struct mbrush_s {
	struct mbrush_s *next;
	mface_t    *faces;
	qboolean    detail;			///< true if brush is detail brush
} mbrush_t;

typedef struct epair_s {
	struct epair_s *next;
	char       *key;
	char       *value;
} epair_t;

/**	In-memory representation of an entity as parsed from the map script.
*/
typedef struct {
	int         line;			///< Map line of entity start (for messages)
	vec3_t      origin;			///< Location of this entity in world-space.
	mbrush_t   *brushes;		///< Nul terminated list of brushes.
	epair_t    *epairs;			///< Nul terminated list of key=value pairs.
} entity_t;

extern int      num_entities;
extern entity_t *entities;

extern int      nummiptexnames;
extern const char **miptexnames;

/**	Load and parse the map script.

	Fills in the ::entities and ::miptexnames arrays.

	\param filename	Path of the map script to parse.
*/
void LoadMapFile (const char *filename);

/**	Allocate a miptex handle for the named miptex.

	If the a miptex with the same name already exists, the previous handle
	will be returned. \c hint and \c skip textures are handled specially,
	returning \c TEX_HINT and \c TEX_SKIP respectively.

	\param name		The name of the texture.
	\return			The handle for the texture.
*/
int FindMiptex (const char *name);

/**	Dump an entity's data to stdout.

	\param ent		The entity to dump.
*/
void PrintEntity (const entity_t *ent);

/**	Get the value for the specified key from an entity.

	\param ent		The entity from which to fetch the value.
	\param key		The key for which to fetch the value.
	\return			The value for the key, or the empty string if the key
					does not exist in this entity.
*/
const char *ValueForKey (const entity_t *ent, const char *key) __attribute__((pure));

/**	Set the value of the entity's key.
	If the key does not exist, one will be added.

	\param ent		The entity owning the key.
	\param key		The key to set.
	\param value	The value to which the key is to be set.
*/
void SetKeyValue (entity_t *ent, const char *key, const char *value);

/**	Parse a vector value from an entity's key.
	\param ent		The entity from which to fetch the key.
	\param key		The key of wich to parse the vector value.
	\param vec		The destination of the vector value.
*/
void GetVectorForKey (const entity_t *ent, const char *key, vec3_t vec);

/**	Write all valid entities to the bsp file.

	Writes all entities that still have \c key=value pairs to the bsp file.
	Only the key=value pairs are written: any brush data is left out.
*/
void WriteEntitiesToString (void);

///@}

#endif//qfbsp_map_h
