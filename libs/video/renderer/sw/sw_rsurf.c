/*
	sw_rsurf.c

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

#include "QF/render.h"
#include "QF/sys.h"

#include "QF/scene/entity.h"

#include "r_internal.h"

#ifdef PIC
# undef USE_INTEL_ASM //XXX asm pic hack
#endif

drawsurf_t  r_drawsurf;

int         sourcesstep, sourcetstep;
#ifndef USE_INTEL_ASM
static int  lightleft;
#endif
static int  blocksize;
int         lightdelta, lightdeltastep;
int         lightright, lightleftstep, lightrightstep, blockdivshift;
static unsigned int blockdivmask;
void       *prowdestbase;
unsigned char *pbasesource;
int         surfrowbytes;				// used by ASM files
unsigned int *r_lightptr;
int         r_stepback;
int         r_lightwidth;
static int         r_numhblocks;
int         r_numvblocks;
static unsigned char *r_source;
unsigned char *r_sourcemax;

void R_DrawSurfaceBlock_mip0 (void);
void R_DrawSurfaceBlock_mip1 (void);
void R_DrawSurfaceBlock_mip2 (void);
void R_DrawSurfaceBlock_mip3 (void);

static void (*surfmiptable[4]) (void) = {
	R_DrawSurfaceBlock_mip0, R_DrawSurfaceBlock_mip1,
	R_DrawSurfaceBlock_mip2, R_DrawSurfaceBlock_mip3};

static unsigned int blocklights[34 * 34];	//FIXME make dynamic


static void
R_AddDynamicLights (transform_t *transform)
{
	msurface_t *surf;
	unsigned int lnum;
	int         sd, td;
	float       dist, rad, minlight;
	vec3_t      impact, local, lightorigin;
	vec4f_t     entorigin = { 0, 0, 0, 1 };
	int         s, t;
	int         i;
	int         smax, tmax;
	mtexinfo_t *tex;

	surf = r_drawsurf.surf;
	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	tex = surf->texinfo;

	if (transform) {
		//FIXME give world entity a transform
		entorigin = Transform_GetWorldPosition (transform);
	}

	for (lnum = 0; lnum < r_maxdlights; lnum++) {
		if (!(surf->dlightbits[lnum / 32] & (1 << (lnum % 32))))
			continue;					// not lit by this light

		VectorSubtract (r_dlights[lnum].origin, entorigin, lightorigin);
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
R_BuildLightMap (transform_t *transform)
{
	int         smax, tmax;
	int         t;
	int         i, size;
	byte       *lightmap;
	unsigned int scale;
	int         maps;
	msurface_t *surf;

	surf = r_drawsurf.surf;

	smax = (surf->extents[0] >> 4) + 1;
	tmax = (surf->extents[1] >> 4) + 1;
	size = smax * tmax;
	lightmap = surf->samples;

	if (!r_refdef.worldmodel->brush.lightdata) {
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
			scale = r_drawsurf.lightadj[maps];	// 8.8 fraction
			for (i = 0; i < size; i++)
				blocklights[i] += lightmap[i] * scale;
			lightmap += size;			// skip to next lightmap
		}
	// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights (transform);

	// bound, invert, and shift
	for (i = 0; i < size; i++) {
		t = (255 * 256 - (int) blocklights[i]) >> (8 - VID_CBITS);

		if (t < (1 << 6))
			t = (1 << 6);

		blocklights[i] = t;
	}
}

void
R_DrawSurface (transform_t *transform)
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
	R_BuildLightMap (transform);

	surfrowbytes = r_drawsurf.rowbytes;

	mt = r_drawsurf.texture;

	r_source = (byte *) mt + mt->offsets[r_drawsurf.surfmip];

	// the fractional light values should range from 0 to
	// (VID_GRADES - 1) << 16 from a source range of 0 - 255

	texwidth = mt->width >> r_drawsurf.surfmip;

	blocksize = 16 >> r_drawsurf.surfmip;
	blockdivshift = 4 - r_drawsurf.surfmip;
	blockdivmask = (1 << blockdivshift) - 1;

	r_lightwidth = (r_drawsurf.surf->extents[0] >> 4) + 1;

	r_numhblocks = r_drawsurf.surfwidth >> blockdivshift;
	r_numvblocks = r_drawsurf.surfheight >> blockdivshift;

//==============================

	pblockdrawer = surfmiptable[r_drawsurf.surfmip];
	// TODO: needs to be set only when there is a display settings change
	horzblockstep = blocksize;

	smax = mt->width >> r_drawsurf.surfmip;
	twidth = texwidth;
	tmax = mt->height >> r_drawsurf.surfmip;
	sourcetstep = texwidth;
	r_stepback = tmax * twidth;

	r_sourcemax = r_source + (tmax * smax);

	soffset = r_drawsurf.surf->texturemins[0];
	basetoffset = r_drawsurf.surf->texturemins[1];

	// << 16 components are to guarantee positive values for %
	soffset = ((soffset >> r_drawsurf.surfmip) + (smax << 16)) % smax;
	basetptr = &r_source[((((basetoffset >> r_drawsurf.surfmip)
							+ (tmax << 16)) % tmax) * twidth)];

	pcolumndest = r_drawsurf.surfdat;

	for (u = 0; u < r_numhblocks; u++) {
		r_lightptr = blocklights + u;

		prowdestbase = pcolumndest;

		pbasesource = basetptr + soffset;

		(*pblockdrawer) ();

		soffset = soffset + blocksize;
		if (soffset >= smax)
			soffset = 0;

		pcolumndest += horzblockstep;
	}
}

#ifndef USE_INTEL_ASM

void
R_DrawSurfaceBlock_mip0 (void)
{
	int         v, i, b, lightstep, lighttemp, light;
	unsigned char pix, *psource, *prowdest;

	psource = pbasesource;
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
			lighttemp = lightleft - lightright;
			lightstep = lighttemp >> 4;

			light = lightright;

			for (b = 15; b >= 0; b--) {
				pix = psource[b];
				prowdest[b] = ((unsigned char *) vid.colormap8)
					[(light & 0xFF00) + pix];
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

void
R_DrawSurfaceBlock_mip1 (void)
{
	int         v, i, b, lightstep, lighttemp, light;
	unsigned char pix, *psource, *prowdest;

	psource = pbasesource;
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
			lighttemp = lightleft - lightright;
			lightstep = lighttemp >> 3;

			light = lightright;

			for (b = 7; b >= 0; b--) {
				pix = psource[b];
				prowdest[b] = ((unsigned char *) vid.colormap8)
					[(light & 0xFF00) + pix];
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

void
R_DrawSurfaceBlock_mip2 (void)
{
	int         v, i, b, lightstep, lighttemp, light;
	unsigned char pix, *psource, *prowdest;

	psource = pbasesource;
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
			lighttemp = lightleft - lightright;
			lightstep = lighttemp >> 2;

			light = lightright;

			for (b = 3; b >= 0; b--) {
				pix = psource[b];
				prowdest[b] = ((unsigned char *) vid.colormap8)
					[(light & 0xFF00) + pix];
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

void
R_DrawSurfaceBlock_mip3 (void)
{
	int         v, i, b, lightstep, lighttemp, light;
	unsigned char pix, *psource, *prowdest;

	psource = pbasesource;
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
			lighttemp = lightleft - lightright;
			lightstep = lighttemp >> 1;

			light = lightright;

			for (b = 1; b >= 0; b--) {
				pix = psource[b];
				prowdest[b] = ((unsigned char *) vid.colormap8)
					[(light & 0xFF00) + pix];
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

#endif

static void
R_GenTurbTile (byte *pbasetex, void *pdest)
{
	int        *turb;
	int         i, j, s, t;
	byte       *pd;

	turb = sintable + ((int) (vr_data.realtime * SPEED) & (CYCLE - 1));
	pd = (byte *) pdest;

	for (i = 0; i < TILE_SIZE; i++) {
		for (j = 0; j < TILE_SIZE; j++) {
			s = (((j << 16) + turb[i & (CYCLE - 1)]) >> 16) & 63;
			t = (((i << 16) + turb[j & (CYCLE - 1)]) >> 16) & 63;
			*pd++ = *(pbasetex + (t << 6) + s);
		}
	}
}

void
R_GenTile (msurface_t *psurf, void *pdest)
{
	if (psurf->flags & SURF_DRAWTURB) {
		R_GenTurbTile (((byte *) psurf->texinfo->texture +
						psurf->texinfo->texture->offsets[0]), pdest);
	} else if (psurf->flags & SURF_DRAWSKY) {
		R_GenSkyTile (pdest);
	} else {
		Sys_Error ("Unknown tile type");
	}
}
