/*
	d_polyse.c

	routines for drawing sets of polygons sharing the same texture
	(used for Alias models)

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

#include "QF/sys.h"

#include "d_local.h"
#include "r_internal.h"
#include "vid_sw.h"

static int  ubasestep, errorterm, erroradjustup, erroradjustdown;

// TODO: put in span spilling to shrink list size
// !!! if this is changed, it must be changed in d_polysa.s too !!!
#define DPS_MAXSPANS		MAXHEIGHT+1		// +1 for spanpackage marking end

// !!! if this is changed, it must be changed in asm_draw.h too !!!
typedef struct {
	int         pdest;
	short      *pz;
	int         count;
	byte       *ptex;
	int         sfrac, tfrac, light, zi;
} spanpackage_t;

typedef struct {
	int         isflattop;
	int         numleftedges;
	int        *pleftedgevert0;
	int        *pleftedgevert1;
	int        *pleftedgevert2;
	int         numrightedges;
	int        *prightedgevert0;
	int        *prightedgevert1;
	int        *prightedgevert2;
} edgetable;

static int         r_p0[6], r_p1[6], r_p2[6];

static int         d_xdenom;

static edgetable  *pedgetable;

static edgetable   edgetables[12] = {
	{0, 1, r_p0, r_p2, NULL, 2, r_p0, r_p1, r_p2},
	{0, 2, r_p1, r_p0, r_p2, 1, r_p1, r_p2, NULL},
	{1, 1, r_p0, r_p2, NULL, 1, r_p1, r_p2, NULL},
	{0, 1, r_p1, r_p0, NULL, 2, r_p1, r_p2, r_p0},
	{0, 2, r_p0, r_p2, r_p1, 1, r_p0, r_p1, NULL},
	{0, 1, r_p2, r_p1, NULL, 1, r_p2, r_p0, NULL},
	{0, 1, r_p2, r_p1, NULL, 2, r_p2, r_p0, r_p1},
	{0, 2, r_p2, r_p1, r_p0, 1, r_p2, r_p0, NULL},
	{0, 1, r_p1, r_p0, NULL, 1, r_p1, r_p2, NULL},
	{1, 1, r_p2, r_p1, NULL, 1, r_p0, r_p1, NULL},
	{1, 1, r_p1, r_p0, NULL, 1, r_p2, r_p0, NULL},
	{0, 1, r_p0, r_p2, NULL, 1, r_p0, r_p1, NULL},
};

static int         r_sstepx, r_tstepx, r_lstepx, r_lstepy, r_sstepy, r_tstepy;
static int         r_zistepx, r_zistepy;
static int         d_aspancount, d_countextrastep;

static spanpackage_t *a_spans;
static spanpackage_t *d_pedgespanpackage;
static int  ystart;
static int  d_pdest;
static byte       *d_ptex;
static short      *d_pz;
static int         d_sfrac, d_tfrac, d_light, d_zi;
static int         d_ptexextrastep, d_sfracextrastep;
static int         d_tfracextrastep, d_lightextrastep, d_pdestextrastep;
static int         d_lightbasestep, d_pdestbasestep, d_ptexbasestep;
static int         d_sfracbasestep, d_tfracbasestep;
static int         d_ziextrastep, d_zibasestep;
static int         d_pzextrastep, d_pzbasestep;

typedef struct {
	int         quotient;
	int         remainder;
} adivtab_t;

static adivtab_t adivtab[32 * 32] = {
#include "adivtab.h"
};


void
sw32_D_PolysetSetEdgeTable (void)
{
	int         edgetableindex;

	// assume the vertices are already in top to bottom order
	edgetableindex = 0;

	// determine which edges are right & left, and the order in which
	// to rasterize them
	if (r_p0[1] >= r_p1[1]) {
		if (r_p0[1] == r_p1[1]) {
			if (r_p0[1] < r_p2[1])
				pedgetable = &edgetables[2];
			else
				pedgetable = &edgetables[5];

			return;
		} else {
			edgetableindex = 1;
		}
	}

	if (r_p0[1] == r_p2[1]) {
		if (edgetableindex)
			pedgetable = &edgetables[8];
		else
			pedgetable = &edgetables[9];

		return;
	} else if (r_p1[1] == r_p2[1]) {
		if (edgetableindex)
			pedgetable = &edgetables[10];
		else
			pedgetable = &edgetables[11];

		return;
	}

	if (r_p0[1] > r_p2[1])
		edgetableindex += 2;

	if (r_p1[1] > r_p2[1])
		edgetableindex += 4;

	pedgetable = &edgetables[edgetableindex];
}

static void
D_DrawNonSubdiv (void)
{
	mtriangle_t *ptri;
	finalvert_t *pfv, *index0, *index1, *index2;
	int         i;
	int         lnumtriangles;

	pfv = sw32_r_affinetridesc.pfinalverts;
	ptri = sw32_r_affinetridesc.ptriangles;
	lnumtriangles = sw32_r_affinetridesc.numtriangles;

	for (i = 0; i < lnumtriangles; i++, ptri++) {
		index0 = pfv + ptri->vertindex[0];
		index1 = pfv + ptri->vertindex[1];
		index2 = pfv + ptri->vertindex[2];

		d_xdenom =
			(index0->v[1] - index1->v[1]) * (index0->v[0] - index2->v[0]) -
			(index0->v[0] - index1->v[0]) * (index0->v[1] - index2->v[1]);

		if (d_xdenom >= 0)
			continue;

		r_p0[0] = index0->v[0];			// u
		r_p0[1] = index0->v[1];			// v
		r_p0[2] = index0->v[2];			// s
		r_p0[3] = index0->v[3];			// t
		r_p0[4] = index0->v[4];			// light
		r_p0[5] = index0->v[5];			// iz

		r_p1[0] = index1->v[0];
		r_p1[1] = index1->v[1];
		r_p1[2] = index1->v[2];
		r_p1[3] = index1->v[3];
		r_p1[4] = index1->v[4];
		r_p1[5] = index1->v[5];

		r_p2[0] = index2->v[0];
		r_p2[1] = index2->v[1];
		r_p2[2] = index2->v[2];
		r_p2[3] = index2->v[3];
		r_p2[4] = index2->v[4];
		r_p2[5] = index2->v[5];

		if (!ptri->facesfront) {
			if (index0->flags & ALIAS_ONSEAM)
				r_p0[2] += sw32_r_affinetridesc.seamfixupX16;
			if (index1->flags & ALIAS_ONSEAM)
				r_p1[2] += sw32_r_affinetridesc.seamfixupX16;
			if (index2->flags & ALIAS_ONSEAM)
				r_p2[2] += sw32_r_affinetridesc.seamfixupX16;
		}

		sw32_D_PolysetSetEdgeTable ();
		sw32_D_RasterizeAliasPolySmooth ();
	}
}

void
sw32_D_PolysetDraw (void)
{
	spanpackage_t spans[DPS_MAXSPANS + 1 +
						((CACHE_SIZE - 1) / sizeof (spanpackage_t)) + 1];

	// one extra because of cache line pretouching

	a_spans = (spanpackage_t *)
		(((intptr_t) &spans[0] + CACHE_SIZE - 1) & ~(CACHE_SIZE - 1));

	D_DrawNonSubdiv ();
}

void
sw32_D_PolysetScanLeftEdge (int height)
{

	do {
		d_pedgespanpackage->pdest = d_pdest;
		d_pedgespanpackage->pz = d_pz;
		d_pedgespanpackage->count = d_aspancount;
		d_pedgespanpackage->ptex = d_ptex;

		d_pedgespanpackage->sfrac = d_sfrac;
		d_pedgespanpackage->tfrac = d_tfrac;

		// FIXME: need to clamp l, s, t, at both ends?
		d_pedgespanpackage->light = d_light;
		d_pedgespanpackage->zi = d_zi;

		d_pedgespanpackage++;

		errorterm += erroradjustup;
		if (errorterm >= 0) {
			d_pdest += d_pdestextrastep;
			d_pz += d_pzextrastep;
			d_aspancount += d_countextrastep;
			d_ptex += d_ptexextrastep;
			d_sfrac += d_sfracextrastep;
			d_ptex += d_sfrac >> 16;

			d_sfrac &= 0xFFFF;
			d_tfrac += d_tfracextrastep;
			if (d_tfrac & 0x10000) {
				d_ptex += sw32_r_affinetridesc.skinwidth;
				d_tfrac &= 0xFFFF;
			}
			d_light += d_lightextrastep;
			d_zi += d_ziextrastep;
			errorterm -= erroradjustdown;
		} else {
			d_pdest += d_pdestbasestep;
			d_pz += d_pzbasestep;
			d_aspancount += ubasestep;
			d_ptex += d_ptexbasestep;
			d_sfrac += d_sfracbasestep;
			d_ptex += d_sfrac >> 16;
			d_sfrac &= 0xFFFF;
			d_tfrac += d_tfracbasestep;
			if (d_tfrac & 0x10000) {
				d_ptex += sw32_r_affinetridesc.skinwidth;
				d_tfrac &= 0xFFFF;
			}
			d_light += d_lightbasestep;
			d_zi += d_zibasestep;
		}
	} while (--height);
}

static void
D_PolysetSetUpForLineScan (fixed8_t startvertu, fixed8_t startvertv,
						   fixed8_t endvertu, fixed8_t endvertv)
{
	double      dm, dn;
	int         tm, tn;
	adivtab_t  *ptemp;

	// TODO: implement x86 version

	errorterm = -1;

	tm = endvertu - startvertu;
	tn = endvertv - startvertv;

	if (((tm <= 16) && (tm >= -15)) && ((tn <= 16) && (tn >= -15))) {
		ptemp = &adivtab[((tm + 15) << 5) + (tn + 15)];
		ubasestep = ptemp->quotient;
		erroradjustup = ptemp->remainder;
		erroradjustdown = tn;
	} else {
		dm = (double) tm;
		dn = (double) tn;

		FloorDivMod (dm, dn, &ubasestep, &erroradjustup);

		erroradjustdown = dn;
	}
}

void
sw32_D_PolysetCalcGradients (int skinwidth)
{
	float       xstepdenominv, ystepdenominv, t0, t1;
	float       p01_minus_p21, p11_minus_p21, p00_minus_p20, p10_minus_p20;

	p00_minus_p20 = r_p0[0] - r_p2[0];
	p01_minus_p21 = r_p0[1] - r_p2[1];
	p10_minus_p20 = r_p1[0] - r_p2[0];
	p11_minus_p21 = r_p1[1] - r_p2[1];

	xstepdenominv = 1.0 / (float) d_xdenom;

	ystepdenominv = -xstepdenominv;

	// ceil () for light so positive steps are exaggerated, negative steps
	// diminished,  pushing us away from underflow toward overflow. Underflow
	// is very visible, overflow is very unlikely, because of ambient lighting
	t0 = r_p0[4] - r_p2[4];
	t1 = r_p1[4] - r_p2[4];
	r_lstepx = (int)
		ceil ((t1 * p01_minus_p21 - t0 * p11_minus_p21) * xstepdenominv);
	r_lstepy = (int)
		ceil ((t1 * p00_minus_p20 - t0 * p10_minus_p20) * ystepdenominv);

	t0 = r_p0[2] - r_p2[2];
	t1 = r_p1[2] - r_p2[2];
	r_sstepx = (int) ((t1 * p01_minus_p21 - t0 * p11_minus_p21) *
					  xstepdenominv);
	r_sstepy = (int) ((t1 * p00_minus_p20 - t0 * p10_minus_p20) *
					  ystepdenominv);

	t0 = r_p0[3] - r_p2[3];
	t1 = r_p1[3] - r_p2[3];
	r_tstepx = (int) ((t1 * p01_minus_p21 - t0 * p11_minus_p21) *
					  xstepdenominv);
	r_tstepy = (int) ((t1 * p00_minus_p20 - t0 * p10_minus_p20) *
					  ystepdenominv);

	t0 = r_p0[5] - r_p2[5];
	t1 = r_p1[5] - r_p2[5];
	r_zistepx = (int) ((t1 * p01_minus_p21 - t0 * p11_minus_p21) *
					   xstepdenominv);
	r_zistepy = (int) ((t1 * p00_minus_p20 - t0 * p10_minus_p20) *
					   ystepdenominv);

//	a_sstepxfrac = r_sstepx & 0xFFFF;
//	a_tstepxfrac = r_tstepx & 0xFFFF;

//	a_ststepxwhole = skinwidth * (r_tstepx >> 16) + (r_sstepx >> 16);
}


static void
D_PolysetDrawSpans (spanpackage_t * pspanpackage)
{
	int i, j, texscantable[2*MAX_LBM_HEIGHT], *texscan;
	// LordHavoc: compute skin row table
	for (i = 0, j = -sw32_r_affinetridesc.skinheight * sw32_r_affinetridesc.skinwidth;
		 i < sw32_r_affinetridesc.skinheight*2;i++, j += sw32_r_affinetridesc.skinwidth)
		texscantable[i] = j;
	texscan = texscantable + sw32_r_affinetridesc.skinheight;

	switch(sw32_ctx->pixbytes) {
	case 1:
	{
		int         lcount, count = 0;
		byte       *lpdest;
		byte       *lptex;
		int         lsfrac, ltfrac;
		int         llight;
		int         lzi;
		short      *lpz;

		do
		{
			lcount = d_aspancount - pspanpackage->count;

			errorterm += erroradjustup;
			if (errorterm >= 0)
			{
				d_aspancount += d_countextrastep;
				errorterm -= erroradjustdown;
			}
			else
				d_aspancount += ubasestep;

			if (lcount)
			{
				lpdest = (byte *) sw32_d_viewbuffer + pspanpackage->pdest;
				lptex = pspanpackage->ptex;
				lpz = pspanpackage->pz;
				lsfrac = pspanpackage->sfrac;
				ltfrac = pspanpackage->tfrac;
				llight = pspanpackage->light;
				lzi = pspanpackage->zi;

				// LordHavoc: optimized zbuffer check (switchs between
				// loops when state changes, and quickly skips groups
				// of hidden pixels)
				do
				{
					if ((lzi >> 16) < *lpz) // hidden
					{
						count = 0;
						goto skiploop8;
					}
drawloop8:
					*lpz++ = lzi >> 16;
					*lpdest++ = ((byte *)sw32_acolormap)
						[(llight & 0xFF00) | lptex[texscan[ltfrac >> 16] +
												   (lsfrac >> 16)]];
					lzi += r_zistepx;
					lsfrac += r_sstepx;
					ltfrac += r_tstepx;
					llight += r_lstepx;
				}
				while (--lcount);
				goto done8;

				do
				{
					if ((lzi >> 16) >= *lpz) // draw
					{
						lsfrac += r_sstepx * count;
						ltfrac += r_tstepx * count;
						llight += r_lstepx * count;
						lpdest += count;
						goto drawloop8;
					}
skiploop8:
					count++;
					lzi += r_zistepx;
					lpz++;
				}
				while (--lcount);
done8:			;
			}

			pspanpackage++;
		}
		while (pspanpackage->count != -999999);
	}
	break;

	case 2:
	{
		int         lcount, count = 0;
		short      *lpdest;
		byte       *lptex;
		int         lsfrac, ltfrac;
		int         llight;
		int         lzi;
		short      *lpz;

		do
		{
			lcount = d_aspancount - pspanpackage->count;

			errorterm += erroradjustup;
			if (errorterm >= 0)
			{
				d_aspancount += d_countextrastep;
				errorterm -= erroradjustdown;
			}
			else
				d_aspancount += ubasestep;

			if (lcount)
			{
				lpdest = (short *) sw32_d_viewbuffer + pspanpackage->pdest;
				lptex = pspanpackage->ptex;
				lpz = pspanpackage->pz;
				lsfrac = pspanpackage->sfrac;
				ltfrac = pspanpackage->tfrac;
				llight = pspanpackage->light;
				lzi = pspanpackage->zi;

				do
				{
					if ((lzi >> 16) < *lpz) // hidden
					{
						count = 0;
						goto skiploop16;
					}
drawloop16:
					*lpz++ = lzi >> 16;
					*lpdest++ = ((short *)sw32_acolormap)[(llight & 0xFF00) | lptex[texscan[ltfrac >> 16] + (lsfrac >> 16)]];
					lzi += r_zistepx;
					lsfrac += r_sstepx;
					ltfrac += r_tstepx;
					llight += r_lstepx;
				}
				while (--lcount);
				goto done16;

				do
				{
					if ((lzi >> 16) >= *lpz) // draw
					{
						lsfrac += r_sstepx * count;
						ltfrac += r_tstepx * count;
						llight += r_lstepx * count;
						lpdest += count;
						goto drawloop16;
					}
skiploop16:
					count++;
					lzi += r_zistepx;
					lpz++;
				}
				while (--lcount);
done16:			;
			}

			pspanpackage++;
		}
		while (pspanpackage->count != -999999);
	}
	break;

	case 4:
	{
		int         lcount, count = 0;
		int        *lpdest;
		byte       *lptex;
		int         lsfrac, ltfrac;
		int         llight;
		int         lzi;
		short      *lpz;

		do
		{
			lcount = d_aspancount - pspanpackage->count;

			errorterm += erroradjustup;
			if (errorterm >= 0)
			{
				d_aspancount += d_countextrastep;
				errorterm -= erroradjustdown;
			}
			else
				d_aspancount += ubasestep;

			if (lcount)
			{
				lpdest = (int *) sw32_d_viewbuffer + pspanpackage->pdest;
				lptex = pspanpackage->ptex;
				lpz = pspanpackage->pz;
				lsfrac = pspanpackage->sfrac;
				ltfrac = pspanpackage->tfrac;
				llight = pspanpackage->light;
				lzi = pspanpackage->zi;

				do
				{
					if ((lzi >> 16) < *lpz) // hidden
					{
						count = 0;
						goto skiploop32;
					}
drawloop32:
					*lpz++ = lzi >> 16;
					*lpdest++ =
						vid.colormap32[(llight & 0xFF00) |
									   lptex[texscan[ltfrac >> 16] +
											 (lsfrac >> 16)]];
					lzi += r_zistepx;
					lsfrac += r_sstepx;
					ltfrac += r_tstepx;
					llight += r_lstepx;
				}
				while (--lcount);
				goto done32;

				do
				{
					if ((lzi >> 16) >= *lpz) // draw
					{
						lsfrac += r_sstepx * count;
						ltfrac += r_tstepx * count;
						llight += r_lstepx * count;
						lpdest += count;
						goto drawloop32;
					}
skiploop32:
					count++;
					lzi += r_zistepx;
					lpz++;
				}
				while (--lcount);
done32:			;
			}

			pspanpackage++;
		}
		while (pspanpackage->count != -999999);
	}
	break;

	default:
		Sys_Error("D_PolysetDrawSpans: unsupported r_pixbytes %i",
				  sw32_ctx->pixbytes);
	}
}


void
sw32_D_RasterizeAliasPolySmooth (void)
{
	int         initialleftheight, initialrightheight;
	int        *plefttop, *prighttop, *pleftbottom, *prightbottom;
	int         working_lstepx, originalcount;

	plefttop = pedgetable->pleftedgevert0;
	prighttop = pedgetable->prightedgevert0;

	pleftbottom = pedgetable->pleftedgevert1;
	prightbottom = pedgetable->prightedgevert1;

	initialleftheight = pleftbottom[1] - plefttop[1];
	initialrightheight = prightbottom[1] - prighttop[1];

	// set the s, t, and light gradients, which are consistent across the
	// triangle, because being a triangle, things are affine
	sw32_D_PolysetCalcGradients (sw32_r_affinetridesc.skinwidth);

// rasterize the polygon

	// scan out the top (and possibly only) part of the left edge
	D_PolysetSetUpForLineScan (plefttop[0], plefttop[1],
							   pleftbottom[0], pleftbottom[1]);

	d_pedgespanpackage = a_spans;

	ystart = plefttop[1];
	d_aspancount = plefttop[0] - prighttop[0];

	d_ptex = (byte *) sw32_r_affinetridesc.pskin + (plefttop[2] >> 16) +
		(plefttop[3] >> 16) * sw32_r_affinetridesc.skinwidth;
	d_sfrac = plefttop[2] & 0xFFFF;
	d_tfrac = plefttop[3] & 0xFFFF;
	d_pzbasestep = sw32_d_zwidth + ubasestep;
	d_pzextrastep = d_pzbasestep + 1;
	d_light = plefttop[4];
	d_zi = plefttop[5];

	d_pdestbasestep = sw32_screenwidth + ubasestep;
	d_pdestextrastep = d_pdestbasestep + 1;
	// LordHavoc: d_pdest has been changed to pixel offset
	d_pdest = ystart * sw32_screenwidth + plefttop[0];
	d_pz = sw32_d_pzbuffer + ystart * sw32_d_zwidth + plefttop[0];

// TODO: can reuse partial expressions here

	// for negative steps in x along left edge, bias toward overflow rather
	// than underflow (sort of turning the floor () we did in the gradient
	// calcs into ceil (), but plus a little bit)
	if (ubasestep < 0)
		working_lstepx = r_lstepx - 1;
	else
		working_lstepx = r_lstepx;

	d_countextrastep = ubasestep + 1;
	d_ptexbasestep = ((r_sstepy + r_sstepx * ubasestep) >> 16) +
		((r_tstepy + r_tstepx * ubasestep) >> 16) * sw32_r_affinetridesc.skinwidth;
	d_sfracbasestep = (r_sstepy + r_sstepx * ubasestep) & 0xFFFF;
	d_tfracbasestep = (r_tstepy + r_tstepx * ubasestep) & 0xFFFF;
	d_lightbasestep = r_lstepy + working_lstepx * ubasestep;
	d_zibasestep = r_zistepy + r_zistepx * ubasestep;

	d_ptexextrastep = ((r_sstepy + r_sstepx * d_countextrastep) >> 16) +
		((r_tstepy + r_tstepx * d_countextrastep) >> 16) *
		sw32_r_affinetridesc.skinwidth;
	d_sfracextrastep = (r_sstepy + r_sstepx * d_countextrastep) & 0xFFFF;
	d_tfracextrastep = (r_tstepy + r_tstepx * d_countextrastep) & 0xFFFF;
	d_lightextrastep = d_lightbasestep + working_lstepx;
	d_ziextrastep = d_zibasestep + r_zistepx;

	sw32_D_PolysetScanLeftEdge (initialleftheight);

	// scan out the bottom part of the left edge, if it exists
	if (pedgetable->numleftedges == 2) {
		int         height;

		plefttop = pleftbottom;
		pleftbottom = pedgetable->pleftedgevert2;

		D_PolysetSetUpForLineScan (plefttop[0], plefttop[1],
								   pleftbottom[0], pleftbottom[1]);

		height = pleftbottom[1] - plefttop[1];

// TODO: make this a function; modularize this function in general

		ystart = plefttop[1];
		d_aspancount = plefttop[0] - prighttop[0];
		d_ptex = (byte *) sw32_r_affinetridesc.pskin + (plefttop[2] >> 16) +
			(plefttop[3] >> 16) * sw32_r_affinetridesc.skinwidth;
		d_sfrac = 0;
		d_tfrac = 0;
		d_light = plefttop[4];
		d_zi = plefttop[5];

		d_pdestbasestep = sw32_screenwidth + ubasestep;
		d_pdestextrastep = d_pdestbasestep + 1;
		// LordHavoc: d_pdest and relatives have been changed to pixel
		// offsets into framebuffer
		d_pdest = ystart * sw32_screenwidth + plefttop[0];
		d_pzbasestep = sw32_d_zwidth + ubasestep;
		d_pzextrastep = d_pzbasestep + 1;
		d_pz = sw32_d_pzbuffer + ystart * sw32_d_zwidth + plefttop[0];

		if (ubasestep < 0)
			working_lstepx = r_lstepx - 1;
		else
			working_lstepx = r_lstepx;

		d_countextrastep = ubasestep + 1;
		d_ptexbasestep = ((r_sstepy + r_sstepx * ubasestep) >> 16) +
			((r_tstepy + r_tstepx * ubasestep) >> 16) *
			sw32_r_affinetridesc.skinwidth;
		d_sfracbasestep = (r_sstepy + r_sstepx * ubasestep) & 0xFFFF;
		d_tfracbasestep = (r_tstepy + r_tstepx * ubasestep) & 0xFFFF;
		d_lightbasestep = r_lstepy + working_lstepx * ubasestep;
		d_zibasestep = r_zistepy + r_zistepx * ubasestep;

		d_ptexextrastep = ((r_sstepy + r_sstepx * d_countextrastep) >> 16) +
			((r_tstepy + r_tstepx * d_countextrastep) >> 16) *
			sw32_r_affinetridesc.skinwidth;
		d_sfracextrastep = (r_sstepy + r_sstepx * d_countextrastep) & 0xFFFF;
		d_tfracextrastep = (r_tstepy + r_tstepx * d_countextrastep) & 0xFFFF;
		d_lightextrastep = d_lightbasestep + working_lstepx;
		d_ziextrastep = d_zibasestep + r_zistepx;

		sw32_D_PolysetScanLeftEdge (height);
	}
	// scan out the top (and possibly only) part of the right edge, updating
	// the count field
	d_pedgespanpackage = a_spans;

	D_PolysetSetUpForLineScan (prighttop[0], prighttop[1],
							   prightbottom[0], prightbottom[1]);
	d_aspancount = 0;
	d_countextrastep = ubasestep + 1;
	originalcount = a_spans[initialrightheight].count;
	a_spans[initialrightheight].count = -999999;	// mark end of the
													// spanpackages
	D_PolysetDrawSpans (a_spans);

	// scan out the bottom part of the right edge, if it exists
	if (pedgetable->numrightedges == 2) {
		int         height;
		spanpackage_t *pstart;

		pstart = a_spans + initialrightheight;
		pstart->count = originalcount;

		d_aspancount = prightbottom[0] - prighttop[0];

		prighttop = prightbottom;
		prightbottom = pedgetable->prightedgevert2;

		height = prightbottom[1] - prighttop[1];

		D_PolysetSetUpForLineScan (prighttop[0], prighttop[1],
								   prightbottom[0], prightbottom[1]);

		d_countextrastep = ubasestep + 1;
		a_spans[initialrightheight + height].count = -999999;
		// mark end of the spanpackages
		D_PolysetDrawSpans (pstart);
	}
}
