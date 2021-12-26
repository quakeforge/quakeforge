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

#define NH_DEFINE
#include "namehack.h"

#ifdef HAVE_STRING_H
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include "QF/sys.h"
#include "QF/render.h"

#include "r_internal.h"
#include "vid_internal.h"
#include "vid_sw.h"


static int         iskyspeed = 8;
static int         iskyspeed2 = 2;
float       sw32_r_skyspeed;

float       sw32_r_skytime;

byte       *sw32_r_skysource;

int         sw32_r_skymade;

// TODO: clean up these routines

/*
byte        bottomsky[128 * 131];
byte        bottommask[128 * 131];
byte        newsky[128 * 256];			// newsky and topsky both pack in here,
										// 128 bytes of newsky on the left of
										// each scan, 128 bytes of topsky on
										// the right, because the low-level
										// drawers need 256-byte scan widths
*/
static byte skydata[128*256]; // sky layers for making skytex
static byte skytex[128*256*4]; // current sky texture


/*
	R_InitSky

	A sky texture is 256*128, with the right side being a masked overlay
*/
void
sw32_R_InitSky (texture_t *mt)
{
	/*
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

	sw32_r_skysource = newsky;
	*/

	// LordHavoc: save sky for use
	memcpy(skydata, (byte *) mt + mt->offsets[0], 128*256);
	sw32_r_skysource = skytex;
}

void
sw32_R_MakeSky (void)
{
	int x, y, xshift1, yshift1, xshift2, yshift2;
	byte *base1, *base2;
	static int  xlast = -1, ylast = -1;

	xshift2 = sw32_r_skytime * sw32_r_skyspeed * 2.0f;
	yshift2 = sw32_r_skytime * sw32_r_skyspeed * 2.0f;

	if ((xshift2 == xlast) && (yshift2 == ylast))
		return;

	xlast = xshift2;
	ylast = yshift2;
	xshift1 = xshift2 >> 1;
	yshift1 = yshift2 >> 1;

	switch(sw32_ctx->pixbytes)
	{
	case 1:
		{
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
		}
		break;
	case 2:
		{
			unsigned short *out = (unsigned short *) skytex;
			for (y = 0;y < 128;y++)
			{
				base1 = &skydata[((y + yshift1) & 127) * 256];
				base2 = &skydata[((y + yshift2) & 127) * 256 + 128];
				for (x = 0;x < 128;x++)
				{
					if (base1[(x + xshift1) & 127])
						*out = sw32_8to16table[base1[(x + xshift1) & 127]];
					else
						*out = sw32_8to16table[base2[(x + xshift2) & 127]];
					out++;
				}
				out += 128;
			}
		}
		break;
	case 4:
		{
			unsigned int *out = (unsigned int *) skytex;
			for (y = 0;y < 128;y++)
			{
				base1 = &skydata[((y + yshift1) & 127) * 256];
				base2 = &skydata[((y + yshift2) & 127) * 256 + 128];
				for (x = 0;x < 128;x++)
				{
					if (base1[(x + xshift1) & 127])
						*out = d_8to24table[base1[(x + xshift1) & 127]];
					else
						*out = d_8to24table[base2[(x + xshift2) & 127]];
					out++;
				}
				out += 128;
			}
		}
		break;
	default:
		Sys_Error("R_MakeSky: unsupported r_pixbytes %i", sw32_ctx->pixbytes);
	}
	sw32_r_skymade = 1;
}


void
sw32_R_SetSkyFrame (void)
{
	int         g, s1, s2;
	float       temp;

	sw32_r_skyspeed = iskyspeed;

	g = GreatestCommonDivisor (iskyspeed, iskyspeed2);
	s1 = iskyspeed / g;
	s2 = iskyspeed2 / g;
	temp = SKYSIZE * s1 * s2;

	sw32_r_skytime = vr_data.realtime - ((int) (vr_data.realtime / temp) * temp);

	sw32_r_skymade = 0;
}


/*
   Stub function for loading a skybox.  Currently we have support for
   skyboxes only in GL targets, so we just do nothing here.  --KB
*/
void
sw32_R_LoadSkys (const char *name)
{
}
