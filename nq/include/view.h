/*
	view.h

	@description@

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

	$Id$
*/

#ifndef __view_h
#define __view_h

#include "QF/qtypes.h"
#include "QF/cvar.h"

extern	byte		gammatable[256];	// palette is sent through this
extern	byte		ramps[3][256];
extern float v_blend[4];

extern	cvar_t	*brightness;
extern	cvar_t	*contrast;
extern	cvar_t	*crosshair;


void V_Init (void);
void V_RenderView (void);
float V_CalcRoll (vec3_t angles, vec3_t velocity);
void V_UpdatePalette (void);
void V_CalcBlend (void);

void V_CalcIntermissionRefdef (void);
void V_CalcRefdef (void);
void V_CalcPowerupCshift (void);
qboolean V_CheckGamma (void);

extern	cvar_t	*scr_ofsx;
extern	cvar_t	*scr_ofsy;
extern	cvar_t	*scr_ofsz;
extern	cvar_t	*cl_crossx;
extern	cvar_t	*cl_crossy;
extern	cvar_t	*gl_cshiftpercent;

extern	vec3_t	forward, right, up;

#endif // __cvar_h
