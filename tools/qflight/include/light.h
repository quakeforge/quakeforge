/*
	light.h

	Light tool

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

#ifndef __light_h
#define __light_h

#define	ON_EPSILON	0.1
#define	MAXLIGHTS	1024

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

void LoadNodes (char *file);
qboolean TestLine (vec3_t start, vec3_t stop);

void LightFace (int surfnum);
void LightLeaf (dleaf_t *leaf);

void MakeTnodes (dmodel_t *bm);
byte *GetFileSpace (int size);
void TransformSample (vec3_t in, vec3_t out);
void RotateSample (vec3_t in, vec3_t out);

#endif// __light_h
