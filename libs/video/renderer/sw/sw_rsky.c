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

#include "QF/render.h"

#include "r_internal.h"


static int         iskyspeed = 8;
static int         iskyspeed2 = 2;
float       r_skyspeed;

float       r_skytime;

byte       *r_skysource;

int         r_skymade;

// TODO: clean up these routines

byte        bottomsky[128 * 131];
byte        bottommask[128 * 131];
byte        newsky[128 * 256];	// newsky and topsky both pack in here, 128
								// bytes of newsky on the left of each scan,
								// 128 bytes of topsky on the right, because
								// the low-level drawers need 256-byte widths


/*
	R_InitSky

	A sky texture is 256*128, with the right side being a masked overlay
*/
void
R_InitSky (texture_t *mt)
{
	int         i, j;
	byte       *src;

	src = (byte *) mt + mt->offsets[0];

	for (i = 0; i < 128; i++) {
		for (j = 0; j < 128; j++) {
			newsky[(i * 256) + j + 128] = src[i * 256 + j + 128];
		}
	}

	for (i = 0; i < 128; i++) {
		for (j = 0; j < 131; j++) {
			if (src[i * 256 + (j & 0x7F)]) {
				bottomsky[(i * 131) + j] = src[i * 256 + (j & 0x7F)];
				bottommask[(i * 131) + j] = 0;
			} else {
				bottomsky[(i * 131) + j] = 0;
				bottommask[(i * 131) + j] = 0xff;
			}
		}
	}

	r_skysource = newsky;
}


void
R_MakeSky (void)
{
	int         x, y;
	int         ofs, baseofs;
	int         xshift, yshift;
	byte       *pnewsky;
	static int  xlast = -1, ylast = -1;

	xshift = r_skytime * r_skyspeed;
	yshift = r_skytime * r_skyspeed;

	if ((xshift == xlast) && (yshift == ylast))
		return;

	xlast = xshift;
	ylast = yshift;

	pnewsky = &newsky[0];

	for (y = 0; y < SKYSIZE; y++) {
		baseofs = ((y + yshift) & SKYMASK) * 131;
		for (x = 0; x < SKYSIZE; x++) {
			ofs = baseofs + ((x + xshift) & SKYMASK);

			*pnewsky = (*(pnewsky + 128) & bottommask[ofs]) | bottomsky[ofs];
			pnewsky = pnewsky + 1;
		}
		pnewsky += 128;
	}

	r_skymade = 1;
}


void
R_GenSkyTile (void *pdest)
{
	int         x, y;
	int         ofs, baseofs;
	int         xshift, yshift;
	unsigned int *pnewsky;
	unsigned int *pd;

	xshift = r_skytime * r_skyspeed;
	yshift = r_skytime * r_skyspeed;

	pnewsky = (unsigned int *) &newsky[0];
	pd = (unsigned int *) pdest;

	for (y = 0; y < SKYSIZE; y++) {
		baseofs = ((y + yshift) & SKYMASK) * 131;

// FIXME: clean this up
#if UNALIGNED_OK
		for (x = 0; x < SKYSIZE; x += 4) {
			ofs = baseofs + ((x + xshift) & SKYMASK);

			// PORT: unaligned dword access to bottommask and bottomsky

			*pd = (*(pnewsky + (128 / sizeof (unsigned int))) &
				   *(unsigned int *) &bottommask[ofs]) |
				*(unsigned int *) &bottomsky[ofs];

			pnewsky++;
			pd++;
		}
#else
		for (x = 0; x < SKYSIZE; x++) {
			ofs = baseofs + ((x + xshift) & SKYMASK);

			*(byte *) pd = (*((byte *) pnewsky + 128) &
							*(byte *) & bottommask[ofs]) |
				*(byte *) & bottomsky[ofs];
			pnewsky = (unsigned int *) ((byte *) pnewsky + 1);
			pd = (unsigned int *) ((byte *) pd + 1);
		}
#endif
		pnewsky += 128 / sizeof (unsigned int);
	}
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
