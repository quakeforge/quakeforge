/*
	sw32_rsurf.c

	surface-related refresh code

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
#include "r_internal.h"

drawsurf_t  sw32_r_drawsurf;

static int         lightleft, blocksize, sourcetstep;
static int         lightright, lightleftstep, lightrightstep, blockdivshift;
static unsigned int blockdivmask;
static byte       *prowdestbase;
static byte       *psource;
static int         surfrowbytes;
static int        *r_lightptr;
static int         r_stepback;
static int         r_lightwidth;
static int         r_numhblocks, r_numvblocks;
static byte       *r_source, *r_sourcemax;

static void R_DrawSurfaceBlock8_mip0 (void);
static void R_DrawSurfaceBlock8_mip1 (void);
static void R_DrawSurfaceBlock8_mip2 (void);
static void R_DrawSurfaceBlock8_mip3 (void);
static void R_DrawSurfaceBlock16_mip0 (void);
static void R_DrawSurfaceBlock16_mip1 (void);
static void R_DrawSurfaceBlock16_mip2 (void);
static void R_DrawSurfaceBlock16_mip3 (void);
static void R_DrawSurfaceBlock32_mip0 (void);
static void R_DrawSurfaceBlock32_mip1 (void);
static void R_DrawSurfaceBlock32_mip2 (void);
static void R_DrawSurfaceBlock32_mip3 (void);

static void (*surfmiptable8[4]) (void) = {
	R_DrawSurfaceBlock8_mip0,
	R_DrawSurfaceBlock8_mip1,
	R_DrawSurfaceBlock8_mip2,
	R_DrawSurfaceBlock8_mip3
};

static void (*surfmiptable16[4]) (void) = {
	R_DrawSurfaceBlock16_mip0,
	R_DrawSurfaceBlock16_mip1,
	R_DrawSurfaceBlock16_mip2,
	R_DrawSurfaceBlock16_mip3
};

static void (*surfmiptable32[4]) (void) = {
	R_DrawSurfaceBlock32_mip0,
	R_DrawSurfaceBlock32_mip1,
	R_DrawSurfaceBlock32_mip2,
	R_DrawSurfaceBlock32_mip3
};

static int blocklights[34 * 34];	//FIXME make dynamic


static void
R_AddDynamicLights (void)
{
	msurface_t *surf;
	unsigned int lnum;
	int         sd, td;
	float       dist, rad, minlight;
	vec3_t      impact, local, lightorigin;
	int         s, t;
	int         i;
	int         smax, tmax;
	mtexinfo_t *tex;

	surf = sw32_r_drawsurf.surf;
	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	tex = surf->texinfo;

	for (lnum = 0; lnum < r_maxdlights; lnum++) {
		if (!(surf->dlightbits[lnum / 32] & (1 << (lnum % 32))))
			continue;					// not lit by this light

		VectorSubtract (r_dlights[lnum].origin, currententity->origin,
						lightorigin);
		rad = r_dlights[lnum].radius;
		dist = DotProduct (lightorigin, surf->plane->normal) -
			surf->plane->dist;
		rad -= fabs (dist);
		minlight = r_dlights[lnum].minlight;
		if (rad < minlight)
			continue;
		minlight = rad - minlight;

		for (i = 0; i < 3; i++)
			impact[i] = lightorigin[i] - surf->plane->normal[i] * dist;

		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3];

		local[0] -= surf->texturemins[0];
		local[1] -= surf->texturemins[1];

		for (t = 0; t < tmax; t++) {
			td = local[1] - t * 16;
			if (td < 0)
				td = -td;
			for (s = 0; s < smax; s++) {
				sd = local[0] - s * 16;
				if (sd < 0)
					sd = -sd;
				if (sd > td)
					dist = sd + (td >> 1);
				else
					dist = td + (sd >> 1);
				if (dist < minlight)
					blocklights[t * smax + s] += (rad - dist) * 256;
			}
		}
	}
}

/*
	R_BuildLightMap

	Combine and scale multiple lightmaps into the 8.8 format in blocklights
*/
static void
R_BuildLightMap (void)
{
	int         smax, tmax;
	int         t;
	int         i, size;
	byte       *lightmap;
	unsigned int scale;
	int         maps;
	msurface_t *surf;

	surf = sw32_r_drawsurf.surf;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax * tmax;
	lightmap = surf->samples;

	if (!r_worldentity.model->brush.lightdata) {
		for (i = 0; i < size; i++)
			blocklights[i] = 0;
		return;
	}
	// clear to ambient
	for (i = 0; i < size; i++)
		blocklights[i] = r_refdef.ambientlight << 8;

	// add all the lightmaps
	if (lightmap)
		for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++) {
			scale = sw32_r_drawsurf.lightadj[maps];	// 8.8 fraction
			for (i = 0; i < size; i++)
				blocklights[i] += lightmap[i] * scale;
			lightmap += size;			// skip to next lightmap
		}
	// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights ();

	/*
	 * JohnnyonFlame:
	 * 32 and 16bpp modes uses the positive lighting, unlike 8bpp
	 */
	switch (sw32_r_pixbytes) {
	case 1:
		// bound, invert, and shift
		for (i = 0; i < size; i++) {
			t = (255 * 256 - blocklights[i]) >> (8 - VID_CBITS);

			if (t < (1 << 6))
				t = (1 << 6);

			blocklights[i] = t;
		}
		break;
	default:
		// LordHavoc: changed to positive (not inverse) lighting
		for (i = 0; i < size; i++) {
			t = bound(256, blocklights[i] >> (8 - VID_CBITS),
					  256 * (VID_GRADES - 1));
			blocklights[i] = t;
		}
		break;
	}
}

void
sw32_R_DrawSurface (void)
{
	byte       *basetptr;
	int         smax, tmax, twidth;
	int         u;
	int         soffset, basetoffset, texwidth;
	int         horzblockstep;
	byte       *pcolumndest;
	void        (*pblockdrawer) (void);
	texture_t  *mt;

	// calculate the lightings
	R_BuildLightMap ();

	surfrowbytes = sw32_r_drawsurf.rowbytes;

	mt = sw32_r_drawsurf.texture;

	r_source = (byte *) mt + mt->offsets[sw32_r_drawsurf.surfmip];

	// the fractional light values should range from 0 to
	// (VID_GRADES - 1) << 16 from a source range of 0 - 255

	texwidth = mt->width >> sw32_r_drawsurf.surfmip;

	blocksize = 16 >> sw32_r_drawsurf.surfmip;
	blockdivshift = 4 - sw32_r_drawsurf.surfmip;
	blockdivmask = (1 << blockdivshift) - 1;

	r_lightwidth = (sw32_r_drawsurf.surf->extents[0] >> 4) + 1;

	r_numhblocks = sw32_r_drawsurf.surfwidth >> blockdivshift;
	r_numvblocks = sw32_r_drawsurf.surfheight >> blockdivshift;

//==============================

	smax = mt->width >> sw32_r_drawsurf.surfmip;
	twidth = texwidth;
	tmax = mt->height >> sw32_r_drawsurf.surfmip;
	sourcetstep = texwidth;
	r_stepback = tmax * twidth;

	soffset = sw32_r_drawsurf.surf->texturemins[0];
	basetoffset = sw32_r_drawsurf.surf->texturemins[1];

	switch (sw32_r_pixbytes) {
	case 1:
		pblockdrawer = surfmiptable8[sw32_r_drawsurf.surfmip];
		break;
	case 2:
		pblockdrawer = surfmiptable16[sw32_r_drawsurf.surfmip];
		break;
	case 4:
		pblockdrawer = surfmiptable32[sw32_r_drawsurf.surfmip];
		break;
	default:
		Sys_Error("R_DrawSurface: unsupported r_pixbytes %i", sw32_r_pixbytes);
		pblockdrawer = NULL;
	}

	horzblockstep = blocksize * sw32_r_pixbytes;

	r_sourcemax = r_source + (tmax * smax);

	// << 16 components are to guarantee positive values for %
	basetptr = r_source + (((basetoffset >> sw32_r_drawsurf.surfmip) +
							(tmax << 16)) % tmax) * twidth;
	soffset = (((soffset >> sw32_r_drawsurf.surfmip) + (smax << 16)) % smax);

	pcolumndest = (byte *) sw32_r_drawsurf.surfdat;

	for (u = 0; u < r_numhblocks; u++) {
		r_lightptr = blocklights + u;

		prowdestbase = pcolumndest;

		psource = basetptr + soffset;

		(*pblockdrawer) ();

		soffset = soffset + blocksize;
		if (soffset >= smax)
			soffset = 0;

		pcolumndest += horzblockstep;
	}
}

//=============================================================================

static void
R_DrawSurfaceBlock8_mip0 (void)
{
	int         v, i, b, lightstep, light;
	unsigned char pix, *prowdest;

	prowdest = prowdestbase;

	for (v = 0; v < r_numvblocks; v++) {
		// FIXME: make these locals?
		// FIXME: use delta rather than both right and left, like ASM?
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 4;
		lightrightstep = (r_lightptr[1] - lightright) >> 4;

		for (i = 0; i < 16; i++) {
			lightstep = (lightleft - lightright) >> 4;

			light = lightright;

			for (b = 15; b >= 0; b--) {
				pix = psource[b];
				prowdest[b] = vid.colormap8[(light & 0xFF00) + pix];
				light += lightstep;
			}

			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += surfrowbytes;
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}

static void
R_DrawSurfaceBlock8_mip1 (void)
{
	int         v, i, b, lightstep, light;
	unsigned char pix, *prowdest;

	prowdest = prowdestbase;

	for (v = 0; v < r_numvblocks; v++) {
		// FIXME: make these locals?
		// FIXME: use delta rather than both right and left, like ASM?
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 3;
		lightrightstep = (r_lightptr[1] - lightright) >> 3;

		for (i = 0; i < 8; i++) {
			lightstep = (lightleft - lightright) >> 3;

			light = lightright;

			for (b = 7; b >= 0; b--) {
				pix = psource[b];
				prowdest[b] = vid.colormap8[(light & 0xFF00) + pix];
				light += lightstep;
			}

			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += surfrowbytes;
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}

static void
R_DrawSurfaceBlock8_mip2 (void)
{
	int         v, i, b, lightstep, light;
	unsigned char pix, *prowdest;

	prowdest = prowdestbase;

	for (v = 0; v < r_numvblocks; v++) {
		// FIXME: make these locals?
		// FIXME: use delta rather than both right and left, like ASM?
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 2;
		lightrightstep = (r_lightptr[1] - lightright) >> 2;

		for (i = 0; i < 4; i++) {
			lightstep = (lightleft - lightright) >> 2;

			light = lightright;

			for (b = 3; b >= 0; b--) {
				pix = psource[b];
				prowdest[b] = vid.colormap8[(light & 0xFF00) + pix];
				light += lightstep;
			}

			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += surfrowbytes;
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}

static void
R_DrawSurfaceBlock8_mip3 (void)
{
	int         v, i, b, lightstep, light;
	unsigned char pix, *prowdest;

	prowdest = prowdestbase;

	for (v = 0; v < r_numvblocks; v++) {
		// FIXME: make these locals?
		// FIXME: use delta rather than both right and left, like ASM?
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 1;
		lightrightstep = (r_lightptr[1] - lightright) >> 1;

		for (i = 0; i < 2; i++) {
			lightstep = (lightleft - lightright) >> 1;

			light = lightright;

			for (b = 1; b >= 0; b--) {
				pix = psource[b];
				prowdest[b] = vid.colormap8[(light & 0xFF00) + pix];
				light += lightstep;
			}

			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += surfrowbytes;
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}

static void
R_DrawSurfaceBlock16_mip0 (void)
{
	int         k, v;
	int         lightstep, light;
	unsigned short *prowdest;

	prowdest = (unsigned short *) prowdestbase;

	for (v = 0; v < r_numvblocks; v++)
	{
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 4;
		lightrightstep = (r_lightptr[1] - lightright) >> 4;

		for (k = 0; k < 16; k++)
		{
			light = lightleft;
			lightstep = (lightright - lightleft) >> 4;

			prowdest[0] = vid.colormap16[(light & 0xFF00) + psource[0]];
			light += lightstep;
			prowdest[1] = vid.colormap16[(light & 0xFF00) + psource[1]];
			light += lightstep;
			prowdest[2] = vid.colormap16[(light & 0xFF00) + psource[2]];
			light += lightstep;
			prowdest[3] = vid.colormap16[(light & 0xFF00) + psource[3]];
			light += lightstep;
			prowdest[4] = vid.colormap16[(light & 0xFF00) + psource[4]];
			light += lightstep;
			prowdest[5] = vid.colormap16[(light & 0xFF00) + psource[5]];
			light += lightstep;
			prowdest[6] = vid.colormap16[(light & 0xFF00) + psource[6]];
			light += lightstep;
			prowdest[7] = vid.colormap16[(light & 0xFF00) + psource[7]];
			light += lightstep;
			prowdest[8] = vid.colormap16[(light & 0xFF00) + psource[8]];
			light += lightstep;
			prowdest[9] = vid.colormap16[(light & 0xFF00) + psource[9]];
			light += lightstep;
			prowdest[10] = vid.colormap16[(light & 0xFF00) + psource[10]];
			light += lightstep;
			prowdest[11] = vid.colormap16[(light & 0xFF00) + psource[11]];
			light += lightstep;
			prowdest[12] = vid.colormap16[(light & 0xFF00) + psource[12]];
			light += lightstep;
			prowdest[13] = vid.colormap16[(light & 0xFF00) + psource[13]];
			light += lightstep;
			prowdest[14] = vid.colormap16[(light & 0xFF00) + psource[14]];
			light += lightstep;
			prowdest[15] = vid.colormap16[(light & 0xFF00) + psource[15]];

			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += (surfrowbytes >> 1);
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}

static void
R_DrawSurfaceBlock16_mip1 (void)
{
	int         k, v;
	int         lightstep, light;
	unsigned short *prowdest;

	prowdest = (unsigned short *) prowdestbase;

	for (v = 0; v < r_numvblocks; v++)
	{
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 3;
		lightrightstep = (r_lightptr[1] - lightright) >> 3;

		for (k = 0; k < 8; k++)
		{
			light = lightleft;
			lightstep = (lightright - lightleft) >> 3;

			prowdest[0] = vid.colormap16[(light & 0xFF00) + psource[0]];
			light += lightstep;
			prowdest[1] = vid.colormap16[(light & 0xFF00) + psource[1]];
			light += lightstep;
			prowdest[2] = vid.colormap16[(light & 0xFF00) + psource[2]];
			light += lightstep;
			prowdest[3] = vid.colormap16[(light & 0xFF00) + psource[3]];
			light += lightstep;
			prowdest[4] = vid.colormap16[(light & 0xFF00) + psource[4]];
			light += lightstep;
			prowdest[5] = vid.colormap16[(light & 0xFF00) + psource[5]];
			light += lightstep;
			prowdest[6] = vid.colormap16[(light & 0xFF00) + psource[6]];
			light += lightstep;
			prowdest[7] = vid.colormap16[(light & 0xFF00) + psource[7]];

			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += (surfrowbytes >> 1);
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}

static void
R_DrawSurfaceBlock16_mip2 (void)
{
	int         k, v;
	int         lightstep, light;
	unsigned short *prowdest;

	prowdest = (unsigned short *) prowdestbase;

	for (v = 0; v < r_numvblocks; v++)
	{
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 2;
		lightrightstep = (r_lightptr[1] - lightright) >> 2;

		for (k = 0; k < 4; k++)
		{
			light = lightleft;
			lightstep = (lightright - lightleft) >> 2;

			prowdest[0] = vid.colormap16[(light & 0xFF00) + psource[0]];
			light += lightstep;
			prowdest[1] = vid.colormap16[(light & 0xFF00) + psource[1]];
			light += lightstep;
			prowdest[2] = vid.colormap16[(light & 0xFF00) + psource[2]];
			light += lightstep;
			prowdest[3] = vid.colormap16[(light & 0xFF00) + psource[3]];

			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += (surfrowbytes >> 1);
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}

static void
R_DrawSurfaceBlock16_mip3 (void)
{
	int v;
	unsigned short *prowdest;

	prowdest = (unsigned short *) prowdestbase;

	for (v = 0; v < r_numvblocks; v++)
	{
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 1;
		lightrightstep = (r_lightptr[1] - lightright) >> 1;

		prowdest[0] = vid.colormap16[(lightleft & 0xFF00) + psource[0]];
		prowdest[1] = vid.colormap16[(((lightleft + lightright) >> 1) &
									  0xFF00) + psource[1]];
		psource += sourcetstep;
		lightright += lightrightstep;
		lightleft += lightleftstep;
		prowdest += (surfrowbytes >> 1);

		prowdest[0] = vid.colormap16[(lightleft & 0xFF00) + psource[0]];
		prowdest[1] = vid.colormap16[(((lightleft + lightright) >> 1) &
									  0xFF00) + psource[1]];
		psource += sourcetstep;
		prowdest += (surfrowbytes >> 1);

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}

static void
R_DrawSurfaceBlock32_mip0 (void)
{
	int         k, v;
	int         lightstep, light;
	unsigned int *prowdest;

	prowdest = (unsigned int *) prowdestbase;

	for (v = 0; v < r_numvblocks; v++)
	{
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 4;
		lightrightstep = (r_lightptr[1] - lightright) >> 4;

		for (k = 0; k < 16; k++)
		{
			light = lightleft;
			lightstep = (lightright - lightleft) >> 4;

			prowdest[0] = vid.colormap32[(light & 0xFF00) + psource[0]];
			light += lightstep;
			prowdest[1] = vid.colormap32[(light & 0xFF00) + psource[1]];
			light += lightstep;
			prowdest[2] = vid.colormap32[(light & 0xFF00) + psource[2]];
			light += lightstep;
			prowdest[3] = vid.colormap32[(light & 0xFF00) + psource[3]];
			light += lightstep;
			prowdest[4] = vid.colormap32[(light & 0xFF00) + psource[4]];
			light += lightstep;
			prowdest[5] = vid.colormap32[(light & 0xFF00) + psource[5]];
			light += lightstep;
			prowdest[6] = vid.colormap32[(light & 0xFF00) + psource[6]];
			light += lightstep;
			prowdest[7] = vid.colormap32[(light & 0xFF00) + psource[7]];
			light += lightstep;
			prowdest[8] = vid.colormap32[(light & 0xFF00) + psource[8]];
			light += lightstep;
			prowdest[9] = vid.colormap32[(light & 0xFF00) + psource[9]];
			light += lightstep;
			prowdest[10] = vid.colormap32[(light & 0xFF00) + psource[10]];
			light += lightstep;
			prowdest[11] = vid.colormap32[(light & 0xFF00) + psource[11]];
			light += lightstep;
			prowdest[12] = vid.colormap32[(light & 0xFF00) + psource[12]];
			light += lightstep;
			prowdest[13] = vid.colormap32[(light & 0xFF00) + psource[13]];
			light += lightstep;
			prowdest[14] = vid.colormap32[(light & 0xFF00) + psource[14]];
			light += lightstep;
			prowdest[15] = vid.colormap32[(light & 0xFF00) + psource[15]];

			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += (surfrowbytes >> 2);
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}

static void
R_DrawSurfaceBlock32_mip1 (void)
{
	int         k, v;
	int         lightstep, light;
	unsigned int *prowdest;

	prowdest = (unsigned int *) prowdestbase;

	for (v = 0; v < r_numvblocks; v++)
	{
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 3;
		lightrightstep = (r_lightptr[1] - lightright) >> 3;

		for (k = 0; k < 8; k++)
		{
			light = lightleft;
			lightstep = (lightright - lightleft) >> 3;

			prowdest[0] = vid.colormap32[(light & 0xFF00) + psource[0]];
			light += lightstep;
			prowdest[1] = vid.colormap32[(light & 0xFF00) + psource[1]];
			light += lightstep;
			prowdest[2] = vid.colormap32[(light & 0xFF00) + psource[2]];
			light += lightstep;
			prowdest[3] = vid.colormap32[(light & 0xFF00) + psource[3]];
			light += lightstep;
			prowdest[4] = vid.colormap32[(light & 0xFF00) + psource[4]];
			light += lightstep;
			prowdest[5] = vid.colormap32[(light & 0xFF00) + psource[5]];
			light += lightstep;
			prowdest[6] = vid.colormap32[(light & 0xFF00) + psource[6]];
			light += lightstep;
			prowdest[7] = vid.colormap32[(light & 0xFF00) + psource[7]];

			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += (surfrowbytes >> 2);
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}

static void
R_DrawSurfaceBlock32_mip2 (void)
{
	int         k, v;
	int         lightstep, light;
	unsigned int *prowdest;

	prowdest = (unsigned int *) prowdestbase;

	for (v = 0; v < r_numvblocks; v++)
	{
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 2;
		lightrightstep = (r_lightptr[1] - lightright) >> 2;

		for (k = 0; k < 4; k++)
		{
			light = lightleft;
			lightstep = (lightright - lightleft) >> 2;

			prowdest[0] = vid.colormap32[(light & 0xFF00) + psource[0]];
			light += lightstep;
			prowdest[1] = vid.colormap32[(light & 0xFF00) + psource[1]];
			light += lightstep;
			prowdest[2] = vid.colormap32[(light & 0xFF00) + psource[2]];
			light += lightstep;
			prowdest[3] = vid.colormap32[(light & 0xFF00) + psource[3]];

			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += (surfrowbytes >> 2);
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}

static void
R_DrawSurfaceBlock32_mip3 (void)
{
	int v;
	unsigned int *prowdest;

	prowdest = (unsigned int *) prowdestbase;

	for (v = 0; v < r_numvblocks; v++)
	{
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 1;
		lightrightstep = (r_lightptr[1] - lightright) >> 1;

		prowdest[0] = vid.colormap32[(lightleft & 0xFF00) + psource[0]];
		prowdest[1] = vid.colormap32[(((lightleft + lightright) >> 1) &
									  0xFF00) + psource[1]];
		psource += sourcetstep;
		lightright += lightrightstep;
		lightleft += lightleftstep;
		prowdest += (surfrowbytes >> 2);

		prowdest[0] = vid.colormap32[(lightleft & 0xFF00) + psource[0]];
		prowdest[1] = vid.colormap32[(((lightleft + lightright) >> 1) &
									  0xFF00) + psource[1]];
		psource += sourcetstep;
		lightright += lightrightstep;
		lightleft += lightleftstep;
		prowdest += (surfrowbytes >> 2);

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}

/*
void
R_DrawSurfaceBlock32 (void)
{
	int         k, v;
	int         lightstep, light;
	unsigned int *prowdest;

	prowdest = prowdestbase;

	for (v = 0; v < r_numvblocks; v++) {

		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> blockdivshift;
		lightrightstep = (r_lightptr[1] - lightright) >> blockdivshift;

		for (k = 0; k < blocksize; k++) {
			int         b;

			lightstep = (lightright - lightleft) >> blockdivshift;

			light = lightleft;

			for (b = 0;b < blocksize;b++, light += lightstep)
				prowdest[b] = vid.colormap32[(light & 0xFF00) + psource[b]];

			psource += sourcetstep;
			lightright += lightrightstep;
			lightleft += lightleftstep;
			prowdest += (surfrowbytes >> 2);
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}
*/
