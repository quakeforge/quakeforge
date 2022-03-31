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

*/

#ifndef __entities_h
#define __entities_h

#include "light.h"

/** \defgroup qflight_entities Light entity data.
	\ingroup qflight
*/
///@{

#define DEFAULTLIGHTLEVEL	300
#define DEFAULTFALLOFF 1.0f

// Open Quartz - attenuation types
#define LIGHT_LINEAR	0	// regular attenuation (light - distance)
#define LIGHT_RADIUS	1	// light * (distance - radius) / radius
#define LIGHT_INVERSE	2	// light / distance
#define LIGHT_REALISTIC 3	// light / (distance * distance)
#define LIGHT_NO_ATTEN	4	// no attenuation
#define LIGHT_LH		5	// LordHavocs version of realistic

// Open Quartz - noise type flags
#define NOISE_RANDOM	0	// completely random noise (default)
#define	NOISE_SMOOTH	1	// low res noise with interpolation
#define NOISE_PERLIN	2	// combines several noise frequencies

typedef struct entity_s {
	const char *classname;
	vec3_t      origin;
	vec_t       angle;
	vec_t       light;

	int         sun_light[2];
	vec3_t      sun_color[2];
	int         num_suns;
	vec3_t      sun_dir[NUMSUNS];
	float       shadow_sense;

	// LordHavoc: added falloff (smaller fractions = bigger light area),
	// color, and lightradius (also subbrightness to implement lightradius)
	vec_t       falloff;
	vec_t       lightradius;
	vec_t       subbrightness;
	vec_t       lightoffset;
	vec3_t      color, color2;
	vec3_t      spotdir;
	vec_t       spotcone;
	unsigned short visbyte, visbit;		// which byte and bit to look at in
										// the visdata for the leaf this light
										// is in
	int         style;

	// Open Quartz - special light feilds
	int         noisetype;				// random noise type
	float       noise;					// noise intensity
	float       resolution;				// noise blockiness
	float       persistence;			// perlin noise decay per octave
	int         attenuation;			// light attenuation type
	float       radius;					// the light's maximum range, minimum
										// of cutoff range and _radius key, 0
										// for no maximum

	const char *target;
	const char *targetname;
	struct entity_s *targetent;
	struct plitem_s *dict;
} entity_t;

extern entity_t *entities;
extern int num_entities;
extern entity_t *world_entity;

const char *ValueForKey (entity_t *ent, const char *key);
void SetKeyValue (entity_t *ent, const char *key, const char *value);
entity_t *FindEntityWithKeyPair(const char *key, const char *value);
void GetVectorForKey (entity_t *ent, const char *key, vec3_t vec);

void LoadEntities (void);
void WriteEntitiesToString (void);

///@}

#endif// __entities_h
