/*
	gl_view.c

	player eye positioning

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

#include <stdio.h>

#include "QF/compat.h"
#include "QF/cvar.h"
#include "QF/info.h"

#include "client.h"
#include "glquake.h"
#include "view.h"

extern double    host_frametime;

extern cvar_t   *cl_cshift_powerup;

byte             ramps[3][256];
float            v_blend[4];


/*
	V_CalcBlend

	LordHavoc made this a real, true alpha blend.  Cleaned it up
	a bit, but otherwise this is his code.  --KB
*/
void
V_CalcBlend (void)
{
	float       r = 0, g = 0, b = 0, a = 0;
	float		a2, a3;
	int         i;

	for (i = 0; i < NUM_CSHIFTS; i++) {
		a2 = cl.cshifts[i].percent / 255.0;

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
	}

	v_blend[0] = min (r, 255.0) / 255.0;
	v_blend[1] = min (g, 255.0) / 255.0;
	v_blend[2] = min (b, 255.0) / 255.0;
	v_blend[3] = bound (0.0, a, 1.0);
}


extern void V_CalcGlowCshift (void);
extern void V_CalcItemCshift (void);

void
V_CalcPowerupCshift (void)
{
	if (!cl_cshift_powerup->int_val)
//		|| (atoi (Info_ValueForKey (cl.serverinfo, "cshifts")) & INFO_CSHIFT_POWERUP))
		return;

	if (gl_dlight_polyblend->int_val
		|| !(gl_dlight_lightmap->int_val && gl_dlight_polyblend->int_val)) {
		V_CalcGlowCshift ();
	} else {
		V_CalcItemCshift ();
	}
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
	if (cl.cshifts[CSHIFT_DAMAGE].percent < 0)
		cl.cshifts[CSHIFT_DAMAGE].percent = 0;

	// drop the bonus value
	cl.cshifts[CSHIFT_BONUS].percent -= host_frametime * 100;
	if (cl.cshifts[CSHIFT_BONUS].percent < 0)
		cl.cshifts[CSHIFT_BONUS].percent = 0;

	if (!new && !vid.recalc_refdef)
		return;

	V_CalcBlend ();
}
