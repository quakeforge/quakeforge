/*
	light.h

	Light tool

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

#ifndef __light_h
#define __light_h

/** \defgroup qflight QuakeForge light compiler.
*/

#include "QF/bspfile.h"

/** \defgroup qflight_general General functions
	\ingroup qflight
*/
///@{

#define	ON_EPSILON	0.1
#define	MAXLIGHTS	1024
#define LIGHTDISTBIAS 65536.0
#define BOGUS_RANGE 1000000000

#define NUMHSUNS    32
#define NUMVSUNS    4
#define NUMSUNS     (1 + NUMHSUNS * NUMVSUNS)
#define SHADOWSENSE 0.4

#define	SINGLEMAP	(256*256)

typedef struct {
	vec3_t      v;
	int         samplepos;	// offset into lightmap contributed to
} lightpoint_t;

typedef struct {
	vec3_t      c;
} lightsample_t;

typedef struct {
	vec_t       facedist;
	vec3_t      facenormal;

	vec3_t      testlineimpact;

	int         numpoints;
	int         numsamples;
	// *16 for -extra4x4
	lightpoint_t point[SINGLEMAP*16];
	lightsample_t sample[MAXLIGHTMAPS][SINGLEMAP];

	vec3_t      texorg;
	vec3_t      worldtotex[2];	// s = (world - texorg) . worldtotex[0]
	vec3_t      textoworld[2];	// world = texorg + s * textoworld[0]

	vec_t       exactmins[2], exactmaxs[2];

	int         texmins[2], texsize[2];
	int         lightstyles[MAXLIGHTMAPS];
	dface_t    *face;
} lightinfo_t;

extern float scaledist;
extern float scalecos;
extern float rangescale;

extern int c_culldistplane, c_proper;

extern byte *filebase;

extern vec3_t bsp_origin;
extern vec3_t bsp_xvector;
extern vec3_t bsp_yvector;

extern qboolean extrasamples;

extern float minlights[MAX_MAP_FACES];

void LoadNodes (const char *file);
qboolean TestLine (lightinfo_t *l, const vec3_t start, const vec3_t stop);
qboolean TestSky (lightinfo_t *l, const vec3_t start, const vec3_t stop);

void LightFace (lightinfo_t *l, int surfnum);
void LightLeaf (dleaf_t *leaf);

void MakeTnodes (dmodel_t *bm);
int GetFileSpace (int size);
void TransformSample (vec3_t in, vec3_t out);
void RotateSample (vec3_t in, vec3_t out);

void VisEntity (int ent_index);
void VisStats (void);

extern struct bsp_s *bsp;
extern struct dstring_s *lightdata;
extern struct dstring_s *rgblightdata;

typedef struct lightchain_s {
	struct lightchain_s *next;
	struct entity_s *light;
} lightchain_t;

extern lightchain_t **surfacelightchain;
extern vec3_t *surfaceorgs;
extern struct entity_s **novislights;
extern int num_novislights;

const char *get_tex_name (int texindex) __attribute__((pure));

///@}

#endif// __light_h
