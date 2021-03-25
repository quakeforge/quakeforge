/*
	d_sky.c

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

#include "QF/render.h"
#include "QF/sys.h"

#include "d_local.h"
#include "r_internal.h"

#define SKY_SPAN_SHIFT	5
#define SKY_SPAN_MAX	(1 << SKY_SPAN_SHIFT)


static void
D_Sky_uv_To_st (int u, int v, fixed16_t *s, fixed16_t *t)
{
	float       wu, wv, temp;
	vec3_t      end;

	if (r_refdef.vrect.width >= r_refdef.vrect.height)
		temp = (float) r_refdef.vrect.width;
	else
		temp = (float) r_refdef.vrect.height;

	wu = 8192.0 * (float) (u - (vid.width >> 1)) / temp;
	wv = 8192.0 * (float) ((vid.height >> 1) - v) / temp;

	end[0] = 4096 * vpn[0] + wu * vright[0] + wv * vup[0];
	end[1] = 4096 * vpn[1] + wu * vright[1] + wv * vup[1];
	end[2] = 4096 * vpn[2] + wu * vright[2] + wv * vup[2];
	end[2] *= 3;
	VectorNormalize (end);

	temp = sw32_r_skytime * sw32_r_skyspeed;	// TODO: add D_SetupFrame & set this there
	*s = (int) ((temp + 6 * (SKYSIZE / 2 - 1) * end[0]) * 0x10000);
	*t = (int) ((temp + 6 * (SKYSIZE / 2 - 1) * end[1]) * 0x10000);
}

void
sw32_D_DrawSkyScans (espan_t *pspan)
{
	switch(sw32_r_pixbytes) {
	case 1:
	{
		int         count, spancount, u, v;
		byte       *pdest;
		fixed16_t   s, t, snext, tnext, sstep, tstep;
		int         spancountminus1;

		sstep = 0;							// keep compiler happy
		tstep = 0;							// ditto
		snext = 0;							// ditto
		tnext = 0;							// ditto

		do {
			pdest = (byte *) sw32_d_viewbuffer + sw32_screenwidth * pspan->v + pspan->u;

			count = pspan->count;

			// calculate the initial s & t
			u = pspan->u;
			v = pspan->v;
			D_Sky_uv_To_st (u, v, &s, &t);

			do {
				if (count >= SKY_SPAN_MAX)
					spancount = SKY_SPAN_MAX;
				else
					spancount = count;

				count -= spancount;

				if (count) {
					u += spancount;

					// calculate s and t at far end of span,
					// calculate s and t steps across span by shifting
					D_Sky_uv_To_st (u, v, &snext, &tnext);

					sstep = (snext - s) >> SKY_SPAN_SHIFT;
					tstep = (tnext - t) >> SKY_SPAN_SHIFT;
				} else {
					// calculate s and t at last pixel in span,
					// calculate s and t steps across span by division
					spancountminus1 = (float) (spancount - 1);

					if (spancountminus1 > 0) {
						u += spancountminus1;
						D_Sky_uv_To_st (u, v, &snext, &tnext);

						sstep = (snext - s) / spancountminus1;
						tstep = (tnext - t) / spancountminus1;
					}
				}

				do {
					*pdest++ = ((byte *) sw32_r_skysource)
						[((t & R_SKY_TMASK) >> 8) + ((s & R_SKY_SMASK) >> 16)];
					s += sstep;
					t += tstep;
				} while (--spancount > 0);

				s = snext;
				t = tnext;

			} while (count > 0);

		} while ((pspan = pspan->pnext) != NULL);
	}
	break;

	case 2:
	{
		int         count, spancount, u, v;
		short      *pdest;
		fixed16_t   s, t, snext, tnext, sstep, tstep;
		int         spancountminus1;

		sstep = 0;							// keep compiler happy
		tstep = 0;							// ditto
		snext = 0;							// ditto
		tnext = 0;							// ditto

		do {
			pdest = (short *) sw32_d_viewbuffer + sw32_screenwidth * pspan->v + pspan->u;

			count = pspan->count;

			// calculate the initial s & t
			u = pspan->u;
			v = pspan->v;
			D_Sky_uv_To_st (u, v, &s, &t);

			do {
				if (count >= SKY_SPAN_MAX)
					spancount = SKY_SPAN_MAX;
				else
					spancount = count;

				count -= spancount;

				if (count) {
					u += spancount;

					// calculate s and t at far end of span,
					// calculate s and t steps across span by shifting
					D_Sky_uv_To_st (u, v, &snext, &tnext);

					sstep = (snext - s) >> SKY_SPAN_SHIFT;
					tstep = (tnext - t) >> SKY_SPAN_SHIFT;
				} else {
					// calculate s and t at last pixel in span,
					// calculate s and t steps across span by division
					spancountminus1 = (float) (spancount - 1);

					if (spancountminus1 > 0) {
						u += spancountminus1;
						D_Sky_uv_To_st (u, v, &snext, &tnext);

						sstep = (snext - s) / spancountminus1;
						tstep = (tnext - t) / spancountminus1;
					}
				}

				do {
					*pdest++ = ((short *) sw32_r_skysource)
						[((t & R_SKY_TMASK) >> 8) + ((s & R_SKY_SMASK) >> 16)];
					s += sstep;
					t += tstep;
				} while (--spancount > 0);

				s = snext;
				t = tnext;

			} while (count > 0);

		} while ((pspan = pspan->pnext) != NULL);
	}
	break;

	case 4:
	{
		int         count, spancount, u, v;
		int        *pdest;
		fixed16_t   s, t, snext, tnext, sstep, tstep;
		int         spancountminus1;

		sstep = 0;							// keep compiler happy
		tstep = 0;							// ditto
		snext = 0;							// ditto
		tnext = 0;							// ditto

		do {
			pdest = (int *) sw32_d_viewbuffer + sw32_screenwidth * pspan->v + pspan->u;

			count = pspan->count;

			// calculate the initial s & t
			u = pspan->u;
			v = pspan->v;
			D_Sky_uv_To_st (u, v, &s, &t);

			do {
				if (count >= SKY_SPAN_MAX)
					spancount = SKY_SPAN_MAX;
				else
					spancount = count;

				count -= spancount;

				if (count) {
					u += spancount;

					// calculate s and t at far end of span,
					// calculate s and t steps across span by shifting
					D_Sky_uv_To_st (u, v, &snext, &tnext);

					sstep = (snext - s) >> SKY_SPAN_SHIFT;
					tstep = (tnext - t) >> SKY_SPAN_SHIFT;
				} else {
					// calculate s and t at last pixel in span,
					// calculate s and t steps across span by division
					spancountminus1 = (float) (spancount - 1);

					if (spancountminus1 > 0) {
						u += spancountminus1;
						D_Sky_uv_To_st (u, v, &snext, &tnext);

						sstep = (snext - s) / spancountminus1;
						tstep = (tnext - t) / spancountminus1;
					}
				}

				do {
					*pdest++ = ((int *) sw32_r_skysource)
						[((t & R_SKY_TMASK) >> 8) + ((s & R_SKY_SMASK) >> 16)];
					s += sstep;
					t += tstep;
				} while (--spancount > 0);

				s = snext;
				t = tnext;

			} while (count > 0);

		} while ((pspan = pspan->pnext) != NULL);
	}
	break;

	default:
		Sys_Error("D_DrawSkyScans: unsupported r_pixbytes %i", sw32_r_pixbytes);
	}
}
