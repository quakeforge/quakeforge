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
#include "QF/mathlib.h"
#include "QF/progs.h"
#include "QF/qfplist.h"
#include "QF/qtypes.h"
#include "QF/quakefs.h"
#include "QF/script.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "tools/qflight/include/light.h"
#include "tools/qflight/include/threads.h"
#include "tools/qflight/include/entities.h"
#include "tools/qflight/include/options.h"
#include "tools/qflight/include/properties.h"

entity_t *entities;
int num_entities;
entity_t *world_entity;

/*
	ENTITY FILE PARSING

	If a light has a targetname, generate a unique style in the 32-63 range
*/

int numlighttargets;
const char *lighttargets[32];


static int
LightStyleForTargetname (const char *targetname, qboolean alloc)
{
	int		i;

	for (i = 0; i < numlighttargets; i++)
		if (!strcmp (lighttargets[i], targetname))
			return 32 + i;

	if (!alloc)
		return -1;

	lighttargets[i] = targetname;
	numlighttargets++;
	return numlighttargets - 1 + 32;
}

static void
MatchTargets (void)
{
	int			i, j;

	for (i = 0; i < num_entities; i++) {
		if (!entities[i].target || !entities[i].target[0])
			continue;

		for (j = 0; j < num_entities; j++)
			if (entities[j].targetname
				&& !strcmp (entities[j].targetname, entities[i].target)) {
				entities[i].targetent = &entities[j];
				// set up spotlight values for lighting code to use
				VectorSubtract (entities[i].targetent->origin,
								entities[i].origin, entities[i].spotdir);
				VectorNormalize (entities[i].spotdir);
				if (!entities[i].angle)
					entities[i].spotcone = -cos(20 * M_PI / 180);
				else
					entities[i].spotcone = -cos(entities[i].angle * M_PI / 360);
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
			entities[i].style = entities[j].style;
			SetKeyValue (&entities[i], "style", va ("%i", entities[i].style));
		}

		if (entities[i].spotcone >= 0) {
			VectorZero (entities[i].spotdir);
			entities[i].spotcone = 0;
		}
	}
}

static void
WriteLights (void)
{
	int         i;
	FILE       *f;
	entity_t   *e;

	if (!options.lightsfilename)
		return;
	printf ("building .lights file\n");
	f = fopen (options.lightsfilename, "wb");
	for (i = 0; i < num_entities; i++) {
		e = entities + i;
		if (e->light)
			fprintf(f, "%f %f %f %f %f %f %f %f %f %f %f %f %f %d\n",
					e->origin[0], e->origin[1], e->origin[2],
					e->falloff,
					e->color[0], e->color[1], e->color[2],
					e->subbrightness,
					e->spotdir[0], e->spotdir[1], e->spotdir[2],
					e->spotcone, e->lightoffset, e->style);
	}
	fclose (f);
}

void
LoadEntities (void)
{
	double      vec[4];
	entity_t   *entity;
	float       cutoff_range;
	float       intensity;
	plitem_t   *dict;
	plitem_t   *entity_list;
	plitem_t   *keys;
	int         count;
	script_t   *script;
	const char *field_name;
	const char *value;
	int         i, j;

	script = Script_New ();
	Script_Start (script, "ent data", bsp->entdata);
	entity_list = ED_ConvertToPlist (script, 1);
	Script_Delete (script);

	// start parsing
	num_entities = PL_A_NumObjects (entity_list);
	entities = malloc (num_entities * sizeof (entity_t));

	// go through all the entities
	for (i = 0; i < num_entities; i++) {
		entity = &entities[i];
		memset (entity, 0, sizeof (*entity));

		VectorSet (1, 1, 1, entity->color);
		VectorCopy (entity->color, entity->color2);
		entity->falloff = DEFAULTFALLOFF * DEFAULTFALLOFF;
		entity->lightradius = 0;
		entity->lightoffset = LIGHTDISTBIAS;
		entity->attenuation = options.attenuation;
		entity->sun_light[0] = -1;
		entity->sun_light[1] = -1;
		VectorSet (1, 1, 1, entity->sun_color[0]);
		VectorSet (1, 1, 1, entity->sun_color[1]);
		entity->num_suns = 1 + NUMHSUNS * NUMVSUNS;
		for (j = 0; j < entity->num_suns; j++) {
			// default to straight down
			VectorSet (0, 0, 1, entity->sun_dir[j]);
		}
		entity->dict = PL_ObjectAtIndex (entity_list, i);

		dict = PL_NewDictionary ();

		// go through all the keys in this entity
		keys = PL_D_AllKeys (entity->dict);
		count = PL_A_NumObjects (keys);
		while (count-- > 0) {
			field_name = PL_String (PL_ObjectAtIndex (keys, count));
			value = PL_String (PL_ObjectForKey (entity->dict, field_name));

			if (!strcmp (field_name, "classname"))
				entity->classname = value;
			else if (!strcmp (field_name, "target"))
				entity->target = value;
			else if (!strcmp (field_name, "targetname"))
				entity->targetname = value;
			else if (!strcmp (field_name, "origin")) {
				// scan into doubles, then assign
				// which makes it vec_t size independent
				if (sscanf (value, "%lf %lf %lf",
							&vec[0], &vec[1], &vec[2]) != 3)
					fprintf (stderr, "LoadEntities: not 3 values for origin");
				else
					VectorCopy (vec, entity->origin);
			}

			// the leading _ is so the engine doesn't search for the field,
			// but it's not wanted in the properties dictionary
			if (*field_name == '_')
				field_name++;
			PL_D_AddObject (dict, field_name, PL_NewString (value));
		}

		if (options.verbosity > 1 && entity->targetname)
			printf ("%s %d %d\n", entity->targetname, entity->light,
					entity->style);

		// all fields have been parsed
		if (entity->classname) {
			if (!strcmp (entity->classname, "worldspawn")) {
				int         hsuns = NUMHSUNS;
				if (world_entity)
					Sys_Error ("More than one worldspawn entity");
				set_sun_properties (entity, dict);
				if (options.extrabit == 1)
					hsuns = 6;
				else if (options.extrabit == 2)
					hsuns = 16;
				entity->num_suns = 1 + hsuns * NUMVSUNS;
				if (entity->sun_light[1] <= 0)
					entity->num_suns = 1;
				if (entity->sun_light[0] <= 0)
					entity->num_suns = 0;
				world_entity = entity;
			} else {
				entity->num_suns = 0;
			}
			if (!strncmp (entity->classname, "light", 5)) {
				set_properties (entity, dict);
				if (!entity->light)
					entity->light = DEFAULTLIGHTLEVEL;
				if (!entity->noise)
					entity->noise = options.noise;
				if (!entity->persistence)
					entity->persistence = 1;
			}
		}
		PL_Free (dict);

		if (entity->light) {
			// convert to subtraction to the brightness for the whole light,
			// so it will fade nicely, rather than being clipped off
			VectorScale (entity->color2,
						 entity->light * 16384.0 * options.globallightscale,
						 entity->color2);
			entity->color[0] *= entity->color2[0];
			entity->color[1] *= entity->color2[1];
			entity->color[2] *= entity->color2[2];

			if (entity->lightradius)
				entity->subbrightness = 1.0 / (entity->lightradius
											   * entity->lightradius
											   * entity->falloff
											   + LIGHTDISTBIAS);
			if (entity->subbrightness < (1.0 / 1048576.0))
				entity->subbrightness = (1.0 / 1048576.0);

			intensity = entity->light;
			if (intensity < 0)
				intensity = -intensity;
			switch (entity->attenuation) {
				case LIGHT_RADIUS:
					// default radius is intensity (same as LIGHT_LINEAR)
					if (!entity->radius)
						entity->radius = intensity;
					break;
				case LIGHT_LINEAR:
					// don't allow radius to be greater than intensity
					if (entity->radius > intensity || entity->radius == 0)
						entity->radius = intensity;
					break;
				case LIGHT_REALISTIC:
					if (options.cutoff) {
						// approximate limit for faster lighting
						cutoff_range = sqrt (intensity / options.cutoff);
						if (!entity->radius || cutoff_range < entity->radius)
							entity->radius = cutoff_range;
					}
					break;
				case LIGHT_INVERSE:
					if (options.cutoff) {
						// approximate limit for faster lighting
						cutoff_range = intensity / options.cutoff;
						if (!entity->radius || cutoff_range < entity->radius)
							entity->radius = cutoff_range;
					}
					break;
				case LIGHT_NO_ATTEN:
					break;
				case LIGHT_LH:
					break;
			}
		}

		if (entity->classname && !strcmp (entity->classname, "light")) {
			if (entity->targetname && entity->targetname[0]
				&& !entity->style) {
				char s[16];

				entity->style = LightStyleForTargetname (entity->targetname,
														 true);
				sprintf (s, "%i", entity->style);
				SetKeyValue (entity, "style", s);
			}
		}
	}
	if (!world_entity)
		Sys_Error ("no worldspawn entity");

	if (options.verbosity >= 0)
		printf ("%d entities read\n", num_entities);

	MatchTargets ();

	WriteLights();

	novislights = (entity_t **)calloc (num_entities, sizeof (entity_t *));
}

const char *
ValueForKey (entity_t *ent, const char *key)
{
	plitem_t   *obj = PL_ObjectForKey (ent->dict, key);
	const char *val;

	if (!obj)
		return "";
	val = PL_String (obj);
	if (!val)
		return "";
	return val;
}

void
SetKeyValue (entity_t *ent, const char *key, const char *value)
{
	plitem_t   *obj;

	obj = PL_NewString (value);
	PL_D_AddObject (ent->dict, key, obj);
}

entity_t *
FindEntityWithKeyPair (const char *key, const char *value)
{
	int         i;
	entity_t   *ent;
	plitem_t   *obj;
	const char *val;

	for (i = 0; i < num_entities; i++) {
		ent = &entities[i];
		obj = PL_ObjectForKey (ent->dict, key);
		if (!obj)
			continue;
		val = PL_String (obj);
		if (!val)
			break;
		if (!strcmp (val, value))
			return ent;
	}
	return 0;
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
	int			i;
	plitem_t   *keys;
	int         count;
	const char *field_name;
	const char *value;

	buf = dstring_newstr ();

	if (options.verbosity >= 0)
		printf ("%i switchable light styles\n", numlighttargets);

	for (i = 0; i < num_entities; i++) {
		if (!entities[i].dict)
			continue;		// ent got removed

		dstring_appendstr (buf, "{\n");

		keys = PL_D_AllKeys (entities[i].dict);
		count = PL_A_NumObjects (keys);
		while (count-- > 0) {
			field_name = PL_String (PL_ObjectAtIndex (keys, count));
			value = PL_String (PL_ObjectForKey (entities[i].dict, field_name));
			dasprintf (buf, "\"%s\" \"%s\"\n", field_name, value);
		}
		dstring_appendstr (buf, "}\n");
	}
	BSP_AddEntities (bsp, buf->str, buf->size);
}
