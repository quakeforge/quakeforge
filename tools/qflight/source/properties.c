/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2004 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

static __attribute__ ((used)) const char rcsid[] =
	"$Id$";

#ifdef HAVE_STRING_H
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif
#include <stdlib.h>
#include <errno.h>

#include "QF/mathlib.h"
#include "QF/qfplist.h"
#include "QF/quakeio.h"

#include "compat.h"

#include "entities.h"
#include "options.h"
#include "properties.h"

static plitem_t *properties;

float
parse_float (const char *str)
{
	char       *eptr;
	double      val = strtod (str, &eptr);
	return val;
}

void
parse_color (const char *str, vec3_t color)
{
	double      vec[3];
	vec_t       temp;

	if (sscanf (str, "%lf %lf %lf", &vec[0], &vec[1], &vec[2]) != 3) {
		fprintf (stderr, "not 3 values for color");
		color[0] = color[1] = color[2] = 1.0;
		return;
	}
	// scale the color to have at least one component at 1.0 (or -1.0)
	temp = max (fabs (vec[0]), max (fabs (vec[1]), fabs (vec[2])));
	if (temp != 0.0)
		temp = 1.0 / temp;
	VectorScale (vec, temp, color);
}

float
parse_light (const char *str, vec3_t color)
{
	int         i;
	double      vec[4];

	i = sscanf (str, "%lf %lf %lf %lf", &vec[0], &vec[1], &vec[2], &vec[3]);
	switch (i) {
		case 4:		// HalfLife light
			VectorScale (vec + 1, 1.0 / 255, color);
			return vec[3];
			break;
		case 3:
			VectorCopy (vec, color);
			return 1.0; 
			break;
		case 1:
			color[0] = 1.0;
			color[1] = 1.0;
			color[2] = 1.0;
			return vec[0];
			break;
		default:
			fprintf (stderr, "_light (or light) key must "
					 "be 1 (Quake), 4 (HalfLife), or 3 (HLight) "
					 "values, \"%s\" is not valid\n", str);
			color[0] = 1.0;
			color[1] = 1.0;
			color[2] = 1.0;
			return DEFAULTLIGHTLEVEL;
	}
}

int
parse_attenuation (const char *arg)
{
	if (!strcmp(arg, "linear"))
		return LIGHT_LINEAR;
	else if (!strcmp(arg, "radius"))
		return LIGHT_RADIUS;
	else if (!strcmp(arg, "inverse"))
		return LIGHT_INVERSE;
	else if (!strcmp(arg, "realistic"))
		return LIGHT_REALISTIC;
	else if (!strcmp(arg, "none"))
		return LIGHT_NO_ATTEN;
	else if (!strcmp(arg, "havoc"))
		return LIGHT_LH;
	else {
		char       *eptr;
		int         val = strtol (arg, &eptr, 10);
		if (eptr == arg || *eptr)
			return -1;
		return val;
	}
}

int
parse_noise (const char *arg)
{
	if (!strcmp(arg, "random"))
		return NOISE_RANDOM;
	else if (!strcmp(arg, "smooth"))
		return NOISE_SMOOTH;
	else if (!strcmp(arg, "perlin"))
		return NOISE_PERLIN;
	else {
		char       *eptr;
		int         val = strtol (arg, &eptr, 10);
		if (eptr == arg || *eptr)
			return -1;
		return val;
	}
}

static inline const char *
plstring (plitem_t *pl)
{
	if (pl->type == QFString)
		return pl->data;
	return 0;
}

static plitem_t *
get_item (const char *key, plitem_t *d1, plitem_t *d2)
{
	plitem_t   *p;
	if ((p = PL_ObjectForKey (d1, key)))
		return p;
	if (d2)
		return PL_ObjectForKey (d2, key);
	return 0;
}

void
set_properties (entity_t *ent, plitem_t *dict)
{
	plitem_t   *prop = 0;
	plitem_t   *p;
	const char *str;

	if (properties) {
		prop = PL_ObjectForKey (properties, ent->classname);
		if (!prop)
			prop = PL_ObjectForKey (properties, "default");
	}
	if ((p = get_item ("light", dict, prop))
		|| (p = get_item ("_light", dict, prop))) {
		if ((str = plstring (p))) {
			ent->light = parse_light (str, ent->color);
		}
	}
	if ((p = get_item ("style", dict, prop))) {
		if ((str = plstring (p))) {
			ent->style = atoi (str);
			if ((unsigned) ent->style > 254)
				fprintf (stderr, "Bad light style %i (must be 0-254)",
						 ent->style);
		}
	}
	if ((p = get_item ("angle", dict, prop))) {
		if ((str = plstring (p))) {
			ent->angle = parse_float (str);
		}
	}
	if ((p = get_item ("wait", dict, prop))) {
		if ((str = plstring (p))) {
			ent->falloff = parse_float (str);
			ent->falloff *= ent->falloff;			// presquared
		}
	}
	if ((p = get_item ("_lightradius", dict, prop))) {
		if ((str = plstring (p))) {
			ent->lightradius = parse_float (str);
		}
	}
	if ((p = get_item ("color", dict, prop))
		|| (p = get_item ("_color", dict, prop))) {
		if ((str = plstring (p))) {
			parse_color (str, ent->color2);
		}
	}
	if ((p = get_item ("_attenuation", dict, prop))) {
		if ((str = plstring (p))) {
			ent->attenuation = parse_attenuation (str);
			if (ent->attenuation == -1) {
				ent->attenuation = options.attenuation;
				fprintf (stderr, "Bad light light attenuation: %s\n",
						 str);
			}
		}
	}
	if ((p = get_item ("_radius", dict, prop))) {
		if ((str = plstring (p))) {
			ent->radius = parse_float (str);
		}
	}
	if ((p = get_item ("_noise", dict, prop))) {
		if ((str = plstring (p))) {
			ent->noise = parse_float (str);
		}
	}
	if ((p = get_item ("_noisetype", dict, prop))) {
		if ((str = plstring (p))) {
			ent->noisetype = parse_noise (str);
		}
	}
	if ((p = get_item ("_persistence", dict, prop))) {
		if ((str = plstring (p))) {
			ent->persistence = parse_float (str);
		}
	}
	if ((p = get_item ("_resolution", dict, prop))) {
		if ((str = plstring (p))) {
			ent->resolution = parse_float (str);
		}
	}
}

void
LoadProperties (const char *filename)
{
	QFile      *f;
	char       *buf;
	int         len;

	f = Qopen (filename, "rt");
	if (!f) {
		perror (filename);
		exit (1);
	}
	len = Qfilesize (f);
	buf = malloc (len + 1);
	
	Qread (f, buf, len);
	Qclose (f);
	buf[len] = 0;
	properties = PL_GetPropertyList (buf);
	free (buf);
}
