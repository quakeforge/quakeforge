
/*
	gl_view.c

	OpenGL-specific view management functions

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H 
# include <string.h> 
#endif 
#ifdef HAVE_STRINGS_H 
# include <strings.h> 
#endif 

#include "QF/console.h"
#include "QF/compat.h"

#include "client.h"
#include "glquake.h"
#include "host.h"
#include "view.h"

extern byte        gammatable[256];

extern qboolean    V_CheckGamma (void);

extern void        V_CalcIntermissionRefdef (void);
extern void        V_CalcPowerupCshift (void);
extern void        V_CalcRefdef (void);

extern cvar_t     *crosshair;
extern cvar_t     *gl_cshiftpercent;
extern cvar_t     *scr_ofsx;
extern cvar_t     *scr_ofsy;
extern cvar_t     *scr_ofsz;

byte        ramps[3][256];
float       v_blend[4];					// rgba 0.0 - 1.0


/*
	V_CalcBlend

	LordHavoc made this a real, true alpha blend.  Cleaned it up
	a bit, but otherwise this is his code.  --KB
*/
void
V_CalcBlend (void)
{
	float       r = 0;
	float       g = 0;
	float       b = 0;
	float       a = 0;

	float       a2, a3;
	int         i;

	for (i = 0; i < NUM_CSHIFTS; i++) {
		if (!gl_cshiftpercent->value)
			continue;

		a2 =
			((cl.cshifts[i].percent * gl_cshiftpercent->value) / 100.0) / 255.0;

		if (!a2)
			continue;

		a2 = min (a2, 1.0);
		r += (cl.cshifts[i].destcolor[0] - r) * a2;
		g += (cl.cshifts[i].destcolor[1] - g) * a2;
		b += (cl.cshifts[i].destcolor[2] - b) * a2;

		a3 = (1.0 - a) * (1.0 - a2);
		a = 1.0 - a3;
	}

	// LordHavoc: saturate color
	if (a) {
		a2 = 1.0 / a;
		r *= a2;
		g *= a2;
		b *= a2;
		// don't clamp alpha here, we do it below
	}

	v_blend[0] = min (r, 255.0) / 255.0;
	v_blend[1] = min (g, 255.0) / 255.0;
	v_blend[2] = min (b, 255.0) / 255.0;
	v_blend[3] = bound (0.0, a, 1.0);
}


/*
	V_UpdatePalette

	In software, this function (duh) updates the palette. In GL, all it does is
	set up some values for shifting the screen color in a particular direction.
*/
void
V_UpdatePalette (void)
{
	int         i, j;
	qboolean    new;
	qboolean    force;

	V_CalcPowerupCshift ();

	new = false;

	for (i = 0; i < NUM_CSHIFTS; i++) {
		if (cl.cshifts[i].percent != cl.prev_cshifts[i].percent) {
			new = true;
			cl.prev_cshifts[i].percent = cl.cshifts[i].percent;
		}
		for (j = 0; j < 3; j++) {
			if (cl.cshifts[i].destcolor[j] != cl.prev_cshifts[i].destcolor[j]) {
				new = true;
				cl.prev_cshifts[i].destcolor[j] = cl.cshifts[i].destcolor[j];
			}
		}
	}

	// drop the damage value
	cl.cshifts[CSHIFT_DAMAGE].percent -= host_frametime * 150;
	cl.cshifts[CSHIFT_DAMAGE].percent =
		max (cl.cshifts[CSHIFT_DAMAGE].percent, 0);

	// drop the bonus value
	cl.cshifts[CSHIFT_BONUS].percent -= host_frametime * 100;
	cl.cshifts[CSHIFT_BONUS].percent =
		max (cl.cshifts[CSHIFT_BONUS].percent, 0);

	force = V_CheckGamma ();
	if (!new && !force)
		return;

	V_CalcBlend ();
}


/*
	V_RenderView

	The player's clipping box goes from (-16 -16 -24) to (16 16 32) from
	the entity origin, so any view position inside that will be valid
*/
extern vrect_t scr_vrect;

void
V_RenderView (void)
{
	if (!cl.worldmodel || cls.signon != SIGNONS)
		return;

// don't allow cheats in multiplayer
	if (cl.maxclients > 1) {
		Cvar_Set (scr_ofsx, "0");
		Cvar_Set (scr_ofsy, "0");
		Cvar_Set (scr_ofsz, "0");
	}

	if (cl.intermission) {				// intermission / finale rendering
		V_CalcIntermissionRefdef ();
	} else {
		if (!cl.paused					/* && (sv.maxclients > 1 || key_dest
										   == key_game) */ )
			V_CalcRefdef ();
	}

	R_PushDlights (vec3_origin);

	R_RenderView ();
}


/*
	BuildGammaTable

	In software mode, this function gets the palette ready for changing...
	in GL, it does very little as you can see.
*/
void
BuildGammaTable (float b, float c)
{
	int         i;

	for (i = 0; i < 256; i++)
		gammatable[i] = i;
	return;
}
