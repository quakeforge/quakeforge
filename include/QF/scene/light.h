/*
	light.h

	Entity management

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/02/26

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

#ifndef __QF_scene_light_h
#define __QF_scene_light_h

#include "QF/darray.h"
#include "QF/qtypes.h"
#include "QF/simd/types.h"

struct mod_brush_s;

#define NumStyles 64
#define NoStyle -1

#define LM_LINEAR   0	// light - dist (or radius + dist if -ve)
#define LM_INVERSE  1	// distFactor1 * light / dist
#define LM_INVERSE2 2	// distFactor2 * light / (dist * dist)
#define LM_INFINITE 3	// light
#define LM_AMBIENT  4	// light
#define LM_INVERSE3 5	// distFactor2 * light / (dist + distFactor2)**2

typedef struct light_s {
	vec4f_t     color;
	vec4f_t     position;
	vec4f_t     direction;
	vec4f_t     attenuation;
} light_t;

typedef struct lightset_s DARRAY_TYPE (light_t) lightset_t;
typedef struct lightleafset_s DARRAY_TYPE (int) lightintset_t;
typedef struct lightvisset_s DARRAY_TYPE (byte) lightvisset_t;

typedef struct lightingdata_s {
	lightset_t lights;
	lightintset_t lightstyles;
	lightintset_t lightleafs;
	lightvisset_t lightvis;
	struct set_s *sun_pvs;
	// A fat PVS of leafs visible from visible leafs so hidden lights can
	// illuminate the leafs visible to the player
	struct set_s *pvs;
	struct mleaf_s *leaf;	// the last leaf used to generate the pvs
	struct scene_s *scene;
} lightingdata_t;

lightingdata_t *Light_CreateLightingData (struct scene_s *scene);
void Light_DestroyLightingData (lightingdata_t *ldata);
void Light_ClearLights (lightingdata_t *ldata);
void Light_EnableSun (lightingdata_t *ldata);
void Light_FindVisibleLights (lightingdata_t *ldata);

#endif//__QF_scene_light_h
