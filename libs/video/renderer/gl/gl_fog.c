/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
//gl_fog.c -- global and volumetric fog

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"

#include "compat.h"
#include "r_internal.h"
#include "vid_gl.h"

/*
	Fog_SetupFrame

	called at the beginning of each frame
*/
void
gl_Fog_SetupFrame (void)
{
	quat_t      fogcolor;

	Fog_GetColor (fogcolor);
	qfglFogfv (GL_FOG_COLOR, fogcolor);
	qfglFogf (GL_FOG_DENSITY, Fog_GetDensity () / 64.0);
	qfglFogi (GL_FOG_MODE, GL_EXP2);
}


/*
	Fog_EnableGFog

	called before drawing stuff that should be fogged
*/
void
gl_Fog_EnableGFog (void)
{
	if (Fog_GetDensity () > 0)
		qfglEnable (GL_FOG);
}

/*
	Fog_DisableGFog

	called after drawing stuff that should be fogged
*/
void
gl_Fog_DisableGFog (void)
{
	if (Fog_GetDensity () > 0)
		qfglDisable (GL_FOG);
}

/*
	Fog_StartAdditive

	called before drawing stuff that is additive blended -- sets fog color
	to black
*/
void
gl_Fog_StartAdditive (void)
{
	vec3_t      color = {0, 0, 0};

	qfglFogi (GL_FOG_MODE, GL_EXP2);
	if (Fog_GetDensity () > 0)
		qfglFogfv (GL_FOG_COLOR, color);
}

/*
	Fog_StopAdditive

	called after drawing stuff that is additive blended -- restores fog color
*/
void
gl_Fog_StopAdditive (void)
{
	if (Fog_GetDensity () > 0) {
		quat_t      fogcolor;
		Fog_GetColor (fogcolor);
		qfglFogfv (GL_FOG_COLOR, fogcolor);
	}
}
