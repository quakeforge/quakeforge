/*
	trace.c

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

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((unused)) const char rcsid[] =
	"$Id$";

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
#include "QF/quakefs.h"
#include "QF/sys.h"

#include "light.h"
#include "threads.h"
#include "entities.h"
#include "options.h"

entity_t *entities;
int num_entities;
static int max_entities;

/*
	ENTITY FILE PARSING

	If a light has a targetname, generate a unique style in the 32-63 range
*/

int numlighttargets;
char lighttargets[32][64];


static int
LightStyleForTargetname (const char *targetname, qboolean alloc)
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

static void
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
	const char *data;
	const char *key;
	double      vec[3];
	entity_t   *entity;
	epair_t    *epair;

	data = bsp->entdata;

	// start parsing
	max_entities = num_entities = 0;
	entities = 0;

	// go through all the entities
	while (1) {
		// parse the opening brace      
		data = COM_Parse (data);
		if (!data)
			break;
		if (com_token[0] != '{')
			fprintf (stderr, "LoadEntities: found %s when expecting {",
					 com_token);

		if (num_entities == max_entities) {
			max_entities += 128;
			entities = realloc (entities, max_entities * sizeof (entity_t));
		}
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
			key = strdup (com_token);

			// parse value
			data = COM_Parse (data);
			if (!data)
				fprintf (stderr, "LoadEntities: EOF without closing brace");
			c = com_token[0];
			if (c == '}')
				fprintf (stderr, "LoadEntities: closing brace without data");

			epair = calloc (1, sizeof (epair_t));
			epair->key = key;
			epair->value = strdup (com_token);
			epair->next = entity->epairs;
			entity->epairs = epair;

			if (!strcmp (key, "classname"))
				entity->classname = epair->value;
			else if (!strcmp (key, "target"))
				entity->target = epair->value;
			else if (!strcmp (key, "targetname"))
				entity->targetname = epair->value;
			else if (!strcmp (key, "origin")) {   		
				// scan into doubles, then assign
				// which makes it vec_t size independent
				if (sscanf (com_token, "%lf %lf %lf",
							&vec[0], &vec[1], &vec[2]) != 3)
					fprintf (stderr, "LoadEntities: not 3 values for origin");
				VectorCopy (vec, entity->origin);
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

const char *
ValueForKey (entity_t *ent, const char *key)
{
	epair_t		*ep;
	
	for (ep = ent->epairs; ep; ep = ep->next)
		if (!strcmp (ep->key, key))
			return ep->value;
	return "";
}

void
SetKeyValue (entity_t *ent, const char *key, const char *value)
{
	epair_t		*ep;

	for (ep = ent->epairs; ep; ep = ep->next)
		if (!strcmp (ep->key, key)) {
			ep->value = strdup (value);
			return;
		}
	ep = malloc (sizeof (*ep));
	ep->next = ent->epairs;
	ent->epairs = ep;
	ep->key = strdup (key);
	ep->value = strdup (value);
}

float
FloatForKey (entity_t *ent, const char *key)
{
	const char*k;

	k = ValueForKey (ent, key);
	return atof (k);
}

void
GetVectorForKey (entity_t *ent, const char *key, vec3_t vec)
{
	const char *k;

	k = ValueForKey (ent, key);
	sscanf (k, "%f %f %f", &vec[0], &vec[1], &vec[2]);
}

void
WriteEntitiesToString (void)
{
	dstring_t  *buf;
	char		line[128];
	epair_t		*ep;
	int			i;

	buf = dstring_newstr ();

	if (options.verbosity >= 0)
		printf ("%i switchable light styles\n", numlighttargets);

	for (i = 0; i < num_entities; i++) {
		ep = entities[i].epairs;
		if (!ep)
			continue;		// ent got removed

		dstring_appendstr (buf, "{\n");

		for (ep = entities[i].epairs; ep; ep = ep->next) {
			sprintf (line, "\"%s\" \"%s\"\n", ep->key, ep->value);
			dstring_appendstr (buf, line);
		}
		dstring_appendstr (buf, "}\n");
	}
	BSP_AddEntities (bsp, buf->str, buf->size);
}
