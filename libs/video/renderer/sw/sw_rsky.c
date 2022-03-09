/*
	r_sky.c

	(description)

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>

#include "QF/render.h"

#include "r_internal.h"
#include "vid_internal.h"
#include "vid_sw.h"


static int         iskyspeed = 8;
static int         iskyspeed2 = 2;
float       r_skyspeed;

float       r_skytime;

byte       *r_skysource;

int         r_skymade;

// TODO: clean up these routines

byte        bottomsky[128 * 131];
byte        bottommask[128 * 131];
// sky and topsky both pack in here, 128
// bytes of sky on the left of each scan,
// 128 bytes of topsky on the right, because
// the low-level drawers need 256-byte widths
byte        skydata[128 * 256];
byte        skytex[128 * 256 * 4];


/*
	R_InitSky

	A sky texture is 256*128, with the right side being a masked overlay
*/
void
R_InitSky (texture_t *mt)
{
	memcpy (skydata, (byte *) mt + mt->offsets[0], 128 * 256);
	r_skysource = skytex;
}

void
make_sky_8 (void)
{
	int x, y, xshift1, yshift1, xshift2, yshift2;
	byte *base1, *base2;
	static int  xlast = -1, ylast = -1;

	xshift2 = r_skytime * r_skyspeed * 2.0f;
	yshift2 = r_skytime * r_skyspeed * 2.0f;

	if ((xshift2 == xlast) && (yshift2 == ylast))
		return;

	xlast = xshift2;
	ylast = yshift2;
	xshift1 = xshift2 >> 1;
	yshift1 = yshift2 >> 1;

	byte *out = (byte *) skytex;
	for (y = 0;y < 128;y++)
	{
		base1 = &skydata[((y + yshift1) & 127) * 256];
		base2 = &skydata[((y + yshift2) & 127) * 256 + 128];
		for (x = 0;x < 128;x++)
		{
			if (base1[(x + xshift1) & 127])
				*out = base1[(x + xshift1) & 127];
			else
				*out = base2[(x + xshift2) & 127];
			out++;
		}
		out += 128;
	}
	r_skymade = 1;
}

void
make_sky_16 (void)
{
	int x, y, xshift1, yshift1, xshift2, yshift2;
	byte *base1, *base2;
	static int  xlast = -1, ylast = -1;

	xshift2 = r_skytime * r_skyspeed * 2.0f;
	yshift2 = r_skytime * r_skyspeed * 2.0f;

	if ((xshift2 == xlast) && (yshift2 == ylast))
		return;

	xlast = xshift2;
	ylast = yshift2;
	xshift1 = xshift2 >> 1;
	yshift1 = yshift2 >> 1;

	unsigned short *out = (unsigned short *) skytex;
	for (y = 0;y < 128;y++)
	{
		base1 = &skydata[((y + yshift1) & 127) * 256];
		base2 = &skydata[((y + yshift2) & 127) * 256 + 128];
		for (x = 0;x < 128;x++)
		{
			if (base1[(x + xshift1) & 127])
				*out = d_8to16table[base1[(x + xshift1) & 127]];
			else
				*out = d_8to16table[base2[(x + xshift2) & 127]];
			out++;
		}
		out += 128;
	}
	r_skymade = 1;
}

void
make_sky_32 (void)
{
	int x, y, xshift1, yshift1, xshift2, yshift2;
	byte *base1, *base2;
	static int  xlast = -1, ylast = -1;

	xshift2 = r_skytime * r_skyspeed * 2.0f;
	yshift2 = r_skytime * r_skyspeed * 2.0f;

	if ((xshift2 == xlast) && (yshift2 == ylast))
		return;

	xlast = xshift2;
	ylast = yshift2;
	xshift1 = xshift2 >> 1;
	yshift1 = yshift2 >> 1;

	unsigned int *out = (unsigned int *) skytex;
	for (y = 0;y < 128;y++) {
		base1 = &skydata[((y + yshift1) & 127) * 256];
		base2 = &skydata[((y + yshift2) & 127) * 256 + 128];
		for (x = 0;x < 128;x++) {
			if (base1[(x + xshift1) & 127])
				*out = d_8to24table[base1[(x + xshift1) & 127]];
			else
				*out = d_8to24table[base2[(x + xshift2) & 127]];
			out++;
		}
		out += 128;
	}
	r_skymade = 1;
}

void
R_SetSkyFrame (void)
{
	int         g, s1, s2;
	float       temp;

	r_skyspeed = iskyspeed;

	g = GreatestCommonDivisor (iskyspeed, iskyspeed2);
	s1 = iskyspeed / g;
	s2 = iskyspeed2 / g;
	temp = SKYSIZE * s1 * s2;

	r_skytime = vr_data.realtime - ((int) (vr_data.realtime / temp) * temp);

	r_skymade = 0;
}


/*
   Stub function for loading a skybox.  Currently we have support for
   skyboxes only in GL targets, so we just do nothing here.  --KB
*/
void
R_LoadSkys (const char *name)
{
}
