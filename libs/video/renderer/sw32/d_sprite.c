/*
	d_sprite.c

	software top-level rasterization driver module for drawing sprites

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

#include "compat.h"
#include "d_local.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_sw.h"

static int  sprite_height;
static int  minindex, maxindex;
static sspan_t *sprite_spans;



void
sw32_D_SpriteDrawSpans (sspan_t *pspan)
{
	switch(sw32_ctx->pixbytes) {
	case 1:
	{
		int         count, spancount, izistep;
		int         izi;
		byte       *pbase;
		byte       *pdest;
		fixed16_t   s, t, snext, tnext, sstep, tstep;
		float       sdivz, tdivz, zi, z, du, dv, spancountminus1;
		float       sdivz8stepu, tdivz8stepu, zi8stepu;
		byte        btemp;
		short      *pz;

		sstep = 0;							// keep compiler happy
		tstep = 0;							// ditto

		pbase = (byte *) sw32_cacheblock;

		sdivz8stepu = sw32_d_sdivzstepu * 8;
		tdivz8stepu = sw32_d_tdivzstepu * 8;
		zi8stepu = d_zistepu * 8 * 65536.0f;

		// we count on FP exceptions being turned off to avoid range problems
		izistep = (int) (d_zistepu * 0x8000 * 0x10000);

		do {
			pdest = (byte *) sw32_d_viewbuffer + sw32_screenwidth * pspan->v + pspan->u;
			pz = sw32_d_pzbuffer + (sw32_d_zwidth * pspan->v) + pspan->u;

			count = pspan->count;

			if (count <= 0)
				goto NextSpan1;

			// calculate the initial s/z, t/z, 1/z, s, and t and clamp
			du = (float) pspan->u;
			dv = (float) pspan->v;

			sdivz = sw32_d_sdivzorigin + dv * sw32_d_sdivzstepv + du * sw32_d_sdivzstepu;
			tdivz = sw32_d_tdivzorigin + dv * sw32_d_tdivzstepv + du * sw32_d_tdivzstepu;
			zi = (d_ziorigin + dv * d_zistepv + du * d_zistepu) * 65536.0f;
			z = sw32_d_zitable[(int) zi];
			// we count on FP exceptions being turned off to avoid range
			// problems

			izi = (int) (zi * 0x8000);

			s = (int) (sdivz * z) + sw32_sadjust;
			if (s > sw32_bbextents)
				s = sw32_bbextents;
			else if (s < 0)
				s = 0;

			t = (int) (tdivz * z) + sw32_tadjust;
			if (t > sw32_bbextentt)
				t = sw32_bbextentt;
			else if (t < 0)
				t = 0;

			do {
				// calculate s and t at the far end of the span
				if (count >= 8)
					spancount = 8;
				else
					spancount = count;

				count -= spancount;

				if (count) {
					// calculate s/z, t/z, zi->fixed s and t at far end of
					// span, calculate s and t steps across span by shifting
					sdivz += sdivz8stepu;
					tdivz += tdivz8stepu;
					zi += zi8stepu;
					z = sw32_d_zitable[(int) zi];

					snext = (int) (sdivz * z) + sw32_sadjust;
					if (snext > sw32_bbextents)
						snext = sw32_bbextents;
					else if (snext < 8)
						snext = 8;			// prevent round-off error on <0
											// steps from causing overstepping
											// & running off the texture's edge

					tnext = (int) (tdivz * z) + sw32_tadjust;
						if (tnext > sw32_bbextentt)
							tnext = sw32_bbextentt;
						else if (tnext < 8)
							tnext = 8;			// guard against round-off
												// error on <0 steps

						sstep = (snext - s) >> 3;
						tstep = (tnext - t) >> 3;
				} else {
					// calculate s/z, t/z, zi->fixed s and t at last pixel
					// in span (so can't step off polygon), clamp,
					// calculate s and t steps across span by division,
					// biasing steps low so we don't run off the texture
					spancountminus1 = (float) (spancount - 1);
					sdivz += sw32_d_sdivzstepu * spancountminus1;
					tdivz += sw32_d_tdivzstepu * spancountminus1;
					zi += d_zistepu * 65536.0f * spancountminus1;
					z = sw32_d_zitable[(int) zi];
					snext = (int) (sdivz * z) + sw32_sadjust;
					if (snext > sw32_bbextents)
						snext = sw32_bbextents;
					else if (snext < 8)
						snext = 8;			// prevent round-off error on <0
											// steps from from causing
											// overstepping & running off the
											// edge of the texture

					tnext = (int) (tdivz * z) + sw32_tadjust;
					if (tnext > sw32_bbextentt)
						tnext = sw32_bbextentt;
					else if (tnext < 8)
						tnext = 8;			// guard against round-off error on
											// <0 steps

					if (spancount > 1) {
						sstep = (snext - s) / (spancount - 1);
						tstep = (tnext - t) / (spancount - 1);
					}
				}

				do {
					btemp = pbase[(s >> 16) + (t >> 16) * sw32_cachewidth];
					if (btemp != TRANSPARENT_COLOR) {
						if (*pz <= (izi >> 16)) {
							*pz = izi >> 16;
							*pdest = btemp;
						}
					}

					izi += izistep;
					pdest++;
					pz++;
					s += sstep;
					t += tstep;
				} while (--spancount > 0);

				s = snext;
				t = tnext;

			} while (count > 0);

NextSpan1:
			pspan++;

		} while (pspan->count != DS_SPAN_LIST_END);
	}
	break;

	case 2:
	{
		int         count, spancount, izistep;
		int         izi;
		byte       *pbase;
		short      *pdest;
		fixed16_t   s, t, snext, tnext, sstep, tstep;
		float       sdivz, tdivz, zi, z, du, dv, spancountminus1;
		float       sdivz8stepu, tdivz8stepu, zi8stepu;
		byte        btemp;
		short      *pz;

		sstep = 0;							// keep compiler happy
		tstep = 0;							// ditto

		pbase = (byte *) sw32_cacheblock;

		sdivz8stepu = sw32_d_sdivzstepu * 8;
		tdivz8stepu = sw32_d_tdivzstepu * 8;
		zi8stepu = d_zistepu * 8 * 65536;

		// we count on FP exceptions being turned off to avoid range problems
		izistep = (int) (d_zistepu * 0x8000 * 0x10000);

		do {
			pdest = (short *) sw32_d_viewbuffer + sw32_screenwidth * pspan->v + pspan->u;
			pz = sw32_d_pzbuffer + (sw32_d_zwidth * pspan->v) + pspan->u;

			count = pspan->count;

			if (count <= 0)
				goto NextSpan2;

			// calculate the initial s/z, t/z, 1/z, s, and t and clamp
			du = (float) pspan->u;
			dv = (float) pspan->v;

			sdivz = sw32_d_sdivzorigin + dv * sw32_d_sdivzstepv + du * sw32_d_sdivzstepu;
			tdivz = sw32_d_tdivzorigin + dv * sw32_d_tdivzstepv + du * sw32_d_tdivzstepu;
			zi = (d_ziorigin + dv * d_zistepv + du * d_zistepu) * 65536.0f;
			z = sw32_d_zitable[(int) zi];
			// we count on FP exceptions being turned off to avoid range
			// problems
			izi = (int) (zi * 0x8000);

			s = (int) (sdivz * z) + sw32_sadjust;
			if (s > sw32_bbextents)
				s = sw32_bbextents;
			else if (s < 0)
				s = 0;

			t = (int) (tdivz * z) + sw32_tadjust;
			if (t > sw32_bbextentt)
				t = sw32_bbextentt;
			else if (t < 0)
				t = 0;

			do {
				// calculate s and t at the far end of the span
				if (count >= 8)
					spancount = 8;
				else
					spancount = count;

				count -= spancount;

				if (count) {
					// calculate s/z, t/z, zi->fixed s and t at far end of
					// span, calculate s and t steps across span by shifting
					sdivz += sdivz8stepu;
					tdivz += tdivz8stepu;
					zi += zi8stepu;
					z = sw32_d_zitable[(int) zi];

					snext = (int) (sdivz * z) + sw32_sadjust;
					if (snext > sw32_bbextents)
						snext = sw32_bbextents;
					else if (snext < 8)
						snext = 8;			// prevent round-off error on <0
											// steps from causing overstepping
											// & running off the texture's edge

					tnext = (int) (tdivz * z) + sw32_tadjust;
					if (tnext > sw32_bbextentt)
						tnext = sw32_bbextentt;
					else if (tnext < 8)
						tnext = 8;			// guard against round-off error on
											// <0 steps

					sstep = (snext - s) >> 3;
					tstep = (tnext - t) >> 3;
				} else {
					// calculate s/z, t/z, zi->fixed s and t at last pixel in
					// span (so can't step off polygon), clamp, calculate s
					// and t steps across span by division, biasing steps
					// low so we don't run off the texture
					spancountminus1 = (float) (spancount - 1);
					sdivz += sw32_d_sdivzstepu * spancountminus1;
					tdivz += sw32_d_tdivzstepu * spancountminus1;
					zi += d_zistepu * 65536.0f * spancountminus1;
					z = sw32_d_zitable[(int) zi];
					snext = (int) (sdivz * z) + sw32_sadjust;
					if (snext > sw32_bbextents)
						snext = sw32_bbextents;
					else if (snext < 8)
						snext = 8;			// prevent round-off error on <0
											// steps from from causing
											// overstepping & running off the
											// edge of the texture

					tnext = (int) (tdivz * z) + sw32_tadjust;
					if (tnext > sw32_bbextentt)
						tnext = sw32_bbextentt;
					else if (tnext < 8)
						tnext = 8;			// guard against round-off error on
											// <0 steps

					if (spancount > 1) {
						sstep = (snext - s) / (spancount - 1);
						tstep = (tnext - t) / (spancount - 1);
					}
				}

				do {
					btemp = pbase[(s >> 16) + (t >> 16) * sw32_cachewidth];
					if (btemp != TRANSPARENT_COLOR) {
						if (*pz <= (izi >> 16)) {
							*pz = izi >> 16;
							*pdest = sw32_8to16table[btemp];
						}
					}

					izi += izistep;
					pdest++;
					pz++;
					s += sstep;
					t += tstep;
				} while (--spancount > 0);

				s = snext;
				t = tnext;

			} while (count > 0);

NextSpan2:
			pspan++;

		} while (pspan->count != DS_SPAN_LIST_END);
	}
	break;

	case 4:
	{
		int         count, spancount, izistep;
		int         izi;
		byte       *pbase;
		int        *pdest;
		fixed16_t   s, t, snext, tnext, sstep, tstep;
		float       sdivz, tdivz, zi, z, du, dv, spancountminus1;
		float       sdivz8stepu, tdivz8stepu, zi8stepu;
		byte        btemp;
		short      *pz;

		sstep = 0;							// keep compiler happy
		tstep = 0;							// ditto

		pbase = (byte *) sw32_cacheblock;

		sdivz8stepu = sw32_d_sdivzstepu * 8;
		tdivz8stepu = sw32_d_tdivzstepu * 8;
		zi8stepu = d_zistepu * 8 * 65536;

		// we count on FP exceptions being turned off to avoid range problems
		izistep = (int) (d_zistepu * 0x8000 * 0x10000);

		do {
			pdest = (int *) sw32_d_viewbuffer + sw32_screenwidth * pspan->v + pspan->u;
			pz = sw32_d_pzbuffer + (sw32_d_zwidth * pspan->v) + pspan->u;

			count = pspan->count;

			if (count <= 0)
				goto NextSpan4;

			// calculate the initial s/z, t/z, 1/z, s, and t and clamp
			du = (float) pspan->u;
			dv = (float) pspan->v;

			sdivz = sw32_d_sdivzorigin + dv * sw32_d_sdivzstepv + du * sw32_d_sdivzstepu;
			tdivz = sw32_d_tdivzorigin + dv * sw32_d_tdivzstepv + du * sw32_d_tdivzstepu;
			zi = (d_ziorigin + dv * d_zistepv + du * d_zistepu) * 65536.0f;
			z = sw32_d_zitable[(int) zi];
			// we count on FP exceptions being turned off to avoid range
			// problems
			izi = (int) (zi * 0x8000);

			s = (int) (sdivz * z) + sw32_sadjust;
			if (s > sw32_bbextents)
				s = sw32_bbextents;
			else if (s < 0)
				s = 0;

			t = (int) (tdivz * z) + sw32_tadjust;
			if (t > sw32_bbextentt)
				t = sw32_bbextentt;
			else if (t < 0)
				t = 0;

			do {
				// calculate s and t at the far end of the span
				if (count >= 8)
					spancount = 8;
				else
					spancount = count;

				count -= spancount;

				if (count) {
					// calculate s/z, t/z, zi->fixed s and t at far end of
					// span, calculate s and t steps across span by shifting
					sdivz += sdivz8stepu;
					tdivz += tdivz8stepu;
					zi += zi8stepu;
					z = sw32_d_zitable[(int) zi];

					snext = (int) (sdivz * z) + sw32_sadjust;
					if (snext > sw32_bbextents)
						snext = sw32_bbextents;
					else if (snext < 8)
						snext = 8;			// prevent round-off error on <0
											// steps from causing overstepping
											// & running off the texture's edge

					tnext = (int) (tdivz * z) + sw32_tadjust;
					if (tnext > sw32_bbextentt)
						tnext = sw32_bbextentt;
					else if (tnext < 8)
						tnext = 8;			// guard against round-off error on
											// <0 steps

					sstep = (snext - s) >> 3;
					tstep = (tnext - t) >> 3;
				} else {
					// calculate s/z, t/z, zi->fixed s and t at last pixel in
					// span (so can't step off polygon), clamp, calculate s
					// and t steps across span by division, biasing steps low
					// so we don't run off the texture
					spancountminus1 = (float) (spancount - 1);
					sdivz += sw32_d_sdivzstepu * spancountminus1;
					tdivz += sw32_d_tdivzstepu * spancountminus1;
					zi += d_zistepu * 65536.0f * spancountminus1;
					z = sw32_d_zitable[(int) zi];
					snext = (int) (sdivz * z) + sw32_sadjust;
					if (snext > sw32_bbextents)
						snext = sw32_bbextents;
					else if (snext < 8)
						snext = 8;			// prevent round-off error on <0
											// steps fromcausing overstepping
											// & running off the texture's edge

					tnext = (int) (tdivz * z) + sw32_tadjust;
					if (tnext > sw32_bbextentt)
						tnext = sw32_bbextentt;
					else if (tnext < 8)
						tnext = 8;			// guard against round-off error on
											// <0 steps

					if (spancount > 1) {
						sstep = (snext - s) / (spancount - 1);
						tstep = (tnext - t) / (spancount - 1);
					}
				}

				do {
					btemp = pbase[(s >> 16) + (t >> 16) * sw32_cachewidth];
					if (btemp != TRANSPARENT_COLOR) {
						if (*pz <= (izi >> 16)) {
							*pz = izi >> 16;
							*pdest = d_8to24table[btemp];
						}
					}

					izi += izistep;
					pdest++;
					pz++;
					s += sstep;
					t += tstep;
				} while (--spancount > 0);

				s = snext;
				t = tnext;

			} while (count > 0);

NextSpan4:
			pspan++;

		} while (pspan->count != DS_SPAN_LIST_END);
	}
	break;

	default:
		Sys_Error("D_SpriteDrawSpans: unsupported r_pixbytes %i",
				  sw32_ctx->pixbytes);
	}
}

static void
D_SpriteScanLeftEdge (void)
{
	int         i, v, itop, ibottom, lmaxindex;
	emitpoint_t *pvert, *pnext;
	sspan_t    *pspan;
	float       du, dv, vtop, vbottom, slope;
	fixed16_t   u, u_step;

	pspan = sprite_spans;
	i = minindex;
	if (i == 0)
		i = sw32_r_spritedesc.nump;

	lmaxindex = maxindex;
	if (lmaxindex == 0)
		lmaxindex = sw32_r_spritedesc.nump;

	vtop = ceil (sw32_r_spritedesc.pverts[i].v);

	do {
		pvert = &sw32_r_spritedesc.pverts[i];
		pnext = pvert - 1;

		vbottom = ceil (pnext->v);

		if (vtop < vbottom) {
			du = pnext->u - pvert->u;
			dv = pnext->v - pvert->v;
			slope = du / dv;
			u_step = (int) (slope * 0x10000);
			// adjust u to ceil the integer portion
			u = (int) ((pvert->u + (slope * (vtop - pvert->v))) * 0x10000) +
				(0x10000 - 1);
			itop = (int) vtop;
			ibottom = (int) vbottom;

			for (v = itop; v < ibottom; v++) {
				pspan->u = u >> 16;
				pspan->v = v;
				u += u_step;
				pspan++;
			}
		}

		vtop = vbottom;

		i--;
		if (i == 0)
			i = sw32_r_spritedesc.nump;

	} while (i != lmaxindex);
}

static void
D_SpriteScanRightEdge (void)
{
	int         i, v, itop, ibottom;
	emitpoint_t *pvert, *pnext;
	sspan_t    *pspan;
	float       du, dv, vtop, vbottom, slope, uvert, unext, vvert, vnext;
	fixed16_t   u, u_step;

	pspan = sprite_spans;
	i = minindex;

	vvert = sw32_r_spritedesc.pverts[i].v;
	if (vvert < r_refdef.fvrecty_adj)
		vvert = r_refdef.fvrecty_adj;
	if (vvert > r_refdef.fvrectbottom_adj)
		vvert = r_refdef.fvrectbottom_adj;

	vtop = ceil (vvert);

	do {
		pvert = &sw32_r_spritedesc.pverts[i];
		pnext = pvert + 1;

		vnext = pnext->v;
		if (vnext < r_refdef.fvrecty_adj)
			vnext = r_refdef.fvrecty_adj;
		if (vnext > r_refdef.fvrectbottom_adj)
			vnext = r_refdef.fvrectbottom_adj;

		vbottom = ceil (vnext);

		if (vtop < vbottom) {
			uvert = pvert->u;
			if (uvert < r_refdef.fvrectx_adj)
				uvert = r_refdef.fvrectx_adj;
			if (uvert > r_refdef.fvrectright_adj)
				uvert = r_refdef.fvrectright_adj;

			unext = pnext->u;
			if (unext < r_refdef.fvrectx_adj)
				unext = r_refdef.fvrectx_adj;
			if (unext > r_refdef.fvrectright_adj)
				unext = r_refdef.fvrectright_adj;

			du = unext - uvert;
			dv = vnext - vvert;
			slope = du / dv;
			u_step = (int) (slope * 0x10000);
			// adjust u to ceil the integer portion
			u = (int) ((uvert + (slope * (vtop - vvert))) * 0x10000) +
				(0x10000 - 1);
			itop = (int) vtop;
			ibottom = (int) vbottom;

			for (v = itop; v < ibottom; v++) {
				pspan->count = (u >> 16) - pspan->u;
				u += u_step;
				pspan++;
			}
		}

		vtop = vbottom;
		vvert = vnext;

		i++;
		if (i == sw32_r_spritedesc.nump)
			i = 0;

	} while (i != maxindex);

	pspan->count = DS_SPAN_LIST_END;	// mark the end of the span list
}

static void
D_SpriteCalculateGradients (void)
{
	vec3_t      p_normal, p_saxis, p_taxis, p_temp1;
	float       distinv;

	sw32_TransformVector (sw32_r_spritedesc.vpn, p_normal);
	sw32_TransformVector (sw32_r_spritedesc.vright, p_saxis);
	sw32_TransformVector (sw32_r_spritedesc.vup, p_taxis);
	VectorNegate (p_taxis, p_taxis);

	distinv = 1.0 / (-DotProduct (modelorg, sw32_r_spritedesc.vpn));
	distinv = min (distinv, 1.0);

	sw32_d_sdivzstepu = p_saxis[0] * sw32_xscaleinv;
	sw32_d_tdivzstepu = p_taxis[0] * sw32_xscaleinv;

	sw32_d_sdivzstepv = -p_saxis[1] * sw32_yscaleinv;
	sw32_d_tdivzstepv = -p_taxis[1] * sw32_yscaleinv;

	d_zistepu = p_normal[0] * sw32_xscaleinv * distinv;
	d_zistepv = -p_normal[1] * sw32_yscaleinv * distinv;

	sw32_d_sdivzorigin = p_saxis[2] - sw32_xcenter * sw32_d_sdivzstepu -
		sw32_ycenter * sw32_d_sdivzstepv;
	sw32_d_tdivzorigin = p_taxis[2] - sw32_xcenter * sw32_d_tdivzstepu -
		sw32_ycenter * sw32_d_tdivzstepv;
	d_ziorigin = p_normal[2] * distinv - sw32_xcenter * d_zistepu -
		sw32_ycenter * d_zistepv;

	sw32_TransformVector (modelorg, p_temp1);

	sw32_sadjust = ((fixed16_t) (DotProduct (p_temp1, p_saxis) * 0x10000 + 0.5)) -
		(-(sw32_cachewidth >> 1) << 16);
	sw32_tadjust = ((fixed16_t) (DotProduct (p_temp1, p_taxis) * 0x10000 + 0.5)) -
		(-(sprite_height >> 1) << 16);

	// -1 (-epsilon) so we never wander off the edge of the texture
	sw32_bbextents = (sw32_cachewidth << 16) - 1;
	sw32_bbextentt = (sprite_height << 16) - 1;
}

void
sw32_D_DrawSprite (void)
{
	int         i, nump;
	float       ymin, ymax;
	emitpoint_t *pverts;
	sspan_t     spans[MAXHEIGHT + 1];

	sprite_spans = spans;

	// find the top and bottom vertices, and make sure there's at least one
	// scan to draw
	ymin = 999999.9;
	ymax = -999999.9;
	pverts = sw32_r_spritedesc.pverts;

	for (i = 0; i < sw32_r_spritedesc.nump; i++) {
		if (pverts->v < ymin) {
			ymin = pverts->v;
			minindex = i;
		}

		if (pverts->v > ymax) {
			ymax = pverts->v;
			maxindex = i;
		}

		pverts++;
	}

	ymin = ceil (ymin);
	ymax = ceil (ymax);

	if (ymin >= ymax)
		return;							// doesn't cross any scans at all

	sw32_cachewidth = sw32_r_spritedesc.pspriteframe->width;
	sprite_height = sw32_r_spritedesc.pspriteframe->height;
	sw32_cacheblock = &sw32_r_spritedesc.pspriteframe->pixels[0];

	// copy the first vertex to the last vertex, so we don't have to deal with
	// wrapping
	nump = sw32_r_spritedesc.nump;
	pverts = sw32_r_spritedesc.pverts;
	pverts[nump] = pverts[0];

	D_SpriteCalculateGradients ();
	D_SpriteScanLeftEdge ();
	D_SpriteScanRightEdge ();
	sw32_D_SpriteDrawSpans (sprite_spans);
}
