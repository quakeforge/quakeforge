/*
	vid_common_sw.c

	Common software video driver functions

	Copyright (C) 2001 Jeff Teunissen <deek@quakeforge.net>

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

#include <math.h>

#include "QF/cvar.h"
#include "QF/compat.h"
#include "QF/qargs.h"
#include "vid.h"
#include "view.h"

extern byte 	gammatable[256];
extern cvar_t	*vid_system_gamma;
extern qboolean vid_gamma_avail;	// hardware gamma availability

cvar_t	*vid_gamma;

/*
	VID_InitGamma

	Initialize the gamma lookup table

	This function is less complex than the GL version.
*/
void
VID_InitGamma (unsigned char *pal)
{
	int 	i;
	double	gamma;

	vid_gamma = Cvar_Get ("vid_gamma", "1.45", CVAR_ARCHIVE, VID_UpdateGamma,
						  "Gamma correction");

	if ((i = COM_CheckParm ("-gamma"))) {
		gamma = atof (com_argv[i + 1]);
	} else {
		gamma = vid_gamma->value;
	}
	gamma = bound (0.1, gamma, 9.9);

	Cvar_SetValue (vid_gamma, gamma);

	if (gamma == 1.0) {	// screw the math, 1.0 is linear
		for (i = 0; i < 256; i++) {
			gammatable[i] = i;
		}
	} else {
		if (vid_system_gamma->int_val) {
			VID_SetGamma (gamma);
		} else {
			double	g = 1.0 / gamma;
			int 	v;

			for (i = 0; i < 256; i++) {	// Create the gamma-correction table
				v = (int) ((255.0 * pow ((double) i / 255.0, g)) + 0.5);
				gammatable[i] = bound (0, v, 255);
			}
		}
	}

	for (i = 0; i < 768; i++) {	// correct the palette
		pal[i] = gammatable[pal[i]];
	}
}

/*
	VID_UpdateGamma

	This is a callback to update the palette or system gamma whenever the
	vid_gamma Cvar is changed.
*/
void
VID_UpdateGamma (cvar_t *vid_gamma)
{
	int 	i;
	
	if (vid_gamma->flags & CVAR_ROM)	// System gamma unavailable
		return;

	Cvar_SetValue (vid_gamma, bound (0.1, vid_gamma->value, 9.9));

	if (vid_gamma_avail && vid_system_gamma->int_val) {	// Have sys gamma, use it
		for (i = 0; i < 256; i++) { 	// linear, to keep things kosher
			gammatable[i] = i;
		}
		VID_SetGamma (vid_gamma->value);
	} else {	// We have to hack the palette
		if (vid_gamma->value == 1.0) {	// screw the math, 1.0 is linear
			for (i = 0; i < 256; i++) {
				gammatable[i] = i;
			}
		} else {
			double	g = 1.0 / vid_gamma->value;
			int 	v;

			for (i = 0; i < 256; i++) {	// Update the gamma LUT
				v = (int) ((255.0 * pow ((double) i / 255.0, g)) + 0.5);
				gammatable[i] = bound (0, v, 255);
			}
		}
		
		V_UpdatePalette (); // update with the new palette
	}
}
