/*
	trace.c

	(description)

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

*/
static const char rcsid[] =
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_IO_H
# include <io.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdlib.h>

#include "QF/bspfile.h"
#include "QF/dstring.h"
#include "QF/idparse.h"
#include "QF/mathlib.h"
#include "QF/qtypes.h"
#include "QF/sys.h"
#include "QF/vfile.h"
#include "QF/vfs.h"

#include "light.h"
#include "threads.h"
#include "entities.h"
#include "options.h"

entity_t entities[MAX_MAP_ENTITIES];
int num_entities;

/*
	ENTITY FILE PARSING

	If a light has a targetname, generate a unique style in the 32-63 range
*/

int numlighttargets;
char lighttargets[32][64];


int
LightStyleForTargetname (char *targetname, qboolean alloc)
{
    int		i;
	
    for (i = 0; i < numlighttargets; i++)
		if (!strcmp (lighttargets[i], targetname))
			return 32 + i;
		
    if (!alloc)
		return -1;
	
    strcpy (lighttargets[i], targetname);
    numlighttargets++;
    return numlighttargets - 1 + 32;
}

void
MatchTargets (void)
{
    int			i, j;

	
    for (i = 0; i < num_entities; i++) {
		if (!entities[i].target[0])
			continue;

		for (j = 0; j < num_entities; j++)
			if (!strcmp (entities[j].targetname, entities[i].target)) {
				entities[i].targetent = &entities[j];
				break;
			}
		if (j == num_entities) {
			printf ("WARNING: entity at (%i,%i,%i) (%s) has unmatched "
					"target\n",
					(int) entities[i].origin[0], (int) entities[i].origin[1],
					(int) entities[i].origin[2], entities[i].classname);
			continue;
		}
		// set the style on the source ent for switchable lights
		if (entities[j].style) {
			char s[16];

			entities[i].style = entities[j].style;
			sprintf (s, "%i", entities[i].style);
			SetKeyValue (&entities[i], "style", s);
		}
    }
}

void
LoadEntities (void)
{
    const char	*data;
    entity_t	*entity;
    char		key[64];
    epair_t		*epair;
    double		vec[3];
    int			i;
	
    data = dentdata;

	// start parsing
    num_entities = 0;

	// go through all the entities
    while (1) {
		// parse the opening brace      
		data = COM_Parse (data);
		if (!data)
			break;
		if (com_token[0] != '{')
			fprintf (stderr, "LoadEntities: found %s when expecting {",
					 com_token);

		if (num_entities == MAX_MAP_ENTITIES)
			fprintf (stderr, "LoadEntities: MAX_MAP_ENTITIES");
		entity = &entities[num_entities];
		num_entities++;

		// go through all the keys in this entity
		while (1) {
			int c;

			// parse key
			data = COM_Parse (data);
			if (!data)
				fprintf (stderr, "LoadEntities: EOF without closing brace");
			if (!strcmp (com_token, "}"))
				break;
			strcpy (key, com_token);

			// parse value
			data = COM_Parse (data);
			if (!data)
				fprintf (stderr, "LoadEntities: EOF without closing brace");
			c = com_token[0];
			if (c == '}')
				fprintf (stderr, "LoadEntities: closing brace without data");

			epair = malloc (sizeof (epair_t));
			memset (epair, 0, sizeof (epair));
			strcpy (epair->key, key);
			strcpy (epair->value, com_token);
			epair->next = entity->epairs;
			entity->epairs = epair;

			if (!strcmp (key, "classname"))
				strcpy (entity->classname, com_token);
			else if (!strcmp (key, "target"))
				strcpy (entity->target, com_token);
			else if (!strcmp (key, "targetname"))
				strcpy (entity->targetname, com_token);
			else if (!strcmp (key, "origin")) {   		
				// scan into doubles, then assign
				// which makes it vec_t size independent
				if (sscanf (com_token, "%lf %lf %lf",
						&vec[0], &vec[1], &vec[2]) != 3)
					fprintf (stderr, "LoadEntities: not 3 values for origin");
				for (i = 0; i < 3; i++)
					entity->origin[i] = vec[i];
			} else if (!strncmp (key, "light", 5) || !strcmp (key, "_light")) {
				entity->light = atof(com_token);
			} else if (!strcmp (key, "style")) {
				entity->style = atof (com_token);
				if ((unsigned) entity->style > 254)
					fprintf (stderr, "Bad light style %i (must be 0-254)",
							 entity->style);
			} else if (!strcmp (key, "angle")) {
				entity->angle = atof(com_token);
			}
		}

		// all fields have been parsed
		if (!strncmp (entity->classname, "light", 5) && !entity->light)
			entity->light = DEFAULTLIGHTLEVEL;

		if (!strcmp (entity->classname, "light")) {
			if (entity->targetname[0] && !entity->style) {
				char s[16];

				entity->style = LightStyleForTargetname (entity->targetname,
														 true);
				sprintf (s, "%i", entity->style);
				SetKeyValue (entity, "style", s);
			}
		}
    }

	if (options.verbosity >= 0)
		printf ("%d entities read\n", num_entities);

    MatchTargets ();
}

char	*
ValueForKey (entity_t *ent, char *key)
{
    epair_t		*ep;
	
    for (ep = ent->epairs; ep; ep = ep->next)
		if (!strcmp (ep->key, key))
			return ep->value;
    return "";
}

void
SetKeyValue (entity_t *ent, char *key, char *value)
{
	epair_t		*ep;

    for (ep = ent->epairs; ep; ep = ep->next)
		if (!strcmp (ep->key, key)) {
			strcpy (ep->value, value);
			return;
		}
    ep = malloc (sizeof (*ep));
    ep->next = ent->epairs;
    ent->epairs = ep;
    strcpy (ep->key, key);
    strcpy (ep->value, value);
}

float
FloatForKey (entity_t *ent, char *key)
{
    char		*k;

	k = ValueForKey (ent, key);
    return atof (k);
}

void
GetVectorForKey (entity_t *ent, char *key, vec3_t vec)
{
    char		*k;

    k = ValueForKey (ent, key);
    sscanf (k, "%f %f %f", &vec[0], &vec[1], &vec[2]);
}

void
WriteEntitiesToString (void)
{
    char		*buf, *end;
    char		line[128];
    epair_t		*ep;
    int			i;

    buf = dentdata;
    end = buf;
    *end = 0;

	if (options.verbosity >= 0)
		printf ("%i switchable light styles\n", numlighttargets);

    for (i = 0; i < num_entities; i++) {
		ep = entities[i].epairs;
		if (!ep)
			continue;		// ent got removed

		strcat (end, "{\n");
		end += 2;

		for (ep = entities[i].epairs; ep; ep = ep->next) {
			sprintf (line, "\"%s\" \"%s\"\n", ep->key, ep->value);
			strcat (end, line);
			end += strlen (line);
		}
		strcat (end, "}\n");
		end += 2;

		if (end > buf + MAX_MAP_ENTSTRING)
			fprintf (stderr, "Entity text too long");
    }
    entdatasize = end - buf + 1;
}
