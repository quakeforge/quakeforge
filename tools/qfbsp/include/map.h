/*  Copyright (C) 1996-1997  Id Software, Inc.

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



#define	MAX_FACES		16
typedef struct mface_s
{
	struct mface_s	*next;
	plane_t			plane;
	int				texinfo;
} mface_t;

typedef struct mbrush_s
{
	struct mbrush_s	*next;
	mface_t *faces;
} mbrush_t;

typedef struct epair_s
{
	struct epair_s	*next;
	char	*key;
	char	*value;
} epair_t;

typedef struct
{
	vec3_t		origin;
	mbrush_t		*brushes;
	epair_t		*epairs;
} entity_t;

extern	int			nummapbrushes;
extern	mbrush_t	mapbrushes[MAX_MAP_BRUSHES];

extern	int			num_entities;
extern	entity_t	entities[MAX_MAP_ENTITIES];

extern	int			nummiptex;
extern	char		miptex[MAX_MAP_TEXINFO][16];

void 	LoadMapFile (char *filename);

int		FindMiptex (char *name);

void	PrintEntity (entity_t *ent);
char 	*ValueForKey (entity_t *ent, char *key);
void	SetKeyValue (entity_t *ent, char *key, char *value);
float	FloatForKey (entity_t *ent, char *key);
void 	GetVectorForKey (entity_t *ent, char *key, vec3_t vec);

void	WriteEntitiesToString (void);
