/*
	modelgen.h

	header file for model generation program

	Copyright (C) 1996-1997  Id Software, Inc.

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

// *********************************************************
// * This file must be identical in the modelgen directory *
// * and in the Quake directory, because it's used to      *
// * pass data from one to the other via model files.      *
// *********************************************************

#ifndef __QF_modelgen_h
#define __QF_modelgen_h

#include "QF/mathlib.h"

#define ALIAS_VERSION_MDL	6		// .mdl
#define ALIAS_VERSION_MD2	8		// .md2

#define ALIAS_ONSEAM				0x0020

// must match definition in spritegn.h
#ifndef SYNCTYPE_T
#define SYNCTYPE_T
typedef enum {ST_SYNC=0, ST_RAND } synctype_t;
#endif

typedef enum { ALIAS_SINGLE=0, ALIAS_GROUP } aliasframetype_t;

typedef enum { ALIAS_SKIN_SINGLE=0, ALIAS_SKIN_GROUP } aliasskintype_t;

typedef struct {
	int			ident;
	int			version;
	vec3_t		scale;
	vec3_t		scale_origin;
	float		boundingradius;
	vec3_t		eyeposition;
	int			numskins;
	int			skinwidth;
	int			skinheight;
	int			numverts;
	int			numtris;
	int			numframes;
	synctype_t	synctype;
	int			flags;
	float		size;
} mdl_t;

// TODO: could be shorts

typedef struct {
	float	st[2];
} tex_coord_t;

typedef struct {
	int		onseam;
	int		s;
	int		t;
} stvert_t;

typedef struct dtriangle_s {
	int					facesfront;
	int					vertindex[3];
} dtriangle_t;

#define DT_FACES_FRONT				0x0010

// This mirrors trivert_t in trilib.h, is present so Quake knows how to
// load this data

typedef struct {
	byte	v[3];
	byte	lightnormalindex;
} trivertx_t;

typedef struct {
	unsigned short	v[3];
	unsigned short	lightnormalindex;
} trivertx16_t;

typedef struct {
	trivertx_t	bboxmin;	// lightnormal isn't used
	trivertx_t	bboxmax;	// lightnormal isn't used
	char		name[16];	// frame name from grabbing
} daliasframe_t;

typedef struct {
	int			numframes;
	trivertx_t	bboxmin;	// lightnormal isn't used
	trivertx_t	bboxmax;	// lightnormal isn't used
} daliasgroup_t;

typedef struct {
	int			numskins;
} daliasskingroup_t;

typedef struct {
	float	interval;
} daliasinterval_t;

typedef struct {
	float	interval;
} daliasskininterval_t;

typedef struct {
	aliasframetype_t	type;
} daliasframetype_t;

typedef struct {
	aliasskintype_t	type;
} daliasskintype_t;

// little-endian "IDPO"
#define IDHEADER_MDL	(('O'<<24)+('P'<<16)+('D'<<8)+'I')

// little-endian "MD16" -- 16 bit vertices
#define HEADER_MDL16	(('6'<<24)+('1'<<16)+('D'<<8)+'M')

// little-endian "IDP2"
#define IDHEADER_MD2	(('2'<<24)+('P'<<16)+('D'<<8)+'I')

#endif//__QF_modelgen_h
