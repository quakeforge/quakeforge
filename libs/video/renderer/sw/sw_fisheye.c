/*
	sw_fisheye.c

	SW fisheye rendering

	Copyright (C) 2003  Arkadi Shishlov <arkadi@it.lv>
	Copyright (C) 2022  Bill Currie <bill@taniwha.org>

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include <math.h>

#include "QF/cvar.h"
#include "QF/render.h"

#include "compat.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_sw.h"

static void
fisheyelookuptable (byte **buf, int width, int height, framebuffer_t *cube,
					double fov)
{
	int         cube_size = cube->width;

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			double      dx = x - width / 2;
			double      dy = -(y - height / 2);
			double      yaw = sqrt (dx * dx + dy * dy) * fov / width;
			double      roll = atan2 (dy, dx);
			double      sx = sin (yaw) * cos (roll);
			double      sy = sin (yaw) * sin (roll);
			double      sz = cos (yaw);

			// determine which side of the box we need
			double      abs_x = fabs (sx);
			double      abs_y = fabs (sy);
			double      abs_z = fabs (sz);
			int         side;
			double      xs = 0, ys = 0;

			if (abs_x > abs_y) {
				if (abs_x > abs_z) {
					side = ((sx > 0.0) ? BOX_RIGHT : BOX_LEFT);
				} else {
					side = ((sz > 0.0) ? BOX_FRONT : BOX_BEHIND);
				}
			} else {
				if (abs_y > abs_z) {
					side = ((sy > 0.0) ? BOX_TOP   : BOX_BOTTOM);
				} else {
					side = ((sz > 0.0) ? BOX_FRONT : BOX_BEHIND);
				}
			}

			#define RC(x) ((x / 2) + 0.5)
			#define R2(x) ((x / 2) + 0.5)

			// scale up our vector [x,y,z] to the box
			switch(side) {
				case BOX_FRONT:  xs = RC( sx /  sz); ys = R2(-sy /  sz); break;
				case BOX_BEHIND: xs = RC(-sx / -sz); ys = R2(-sy / -sz); break;
				case BOX_LEFT:   xs = RC( sz / -sx); ys = R2(-sy / -sx); break;
				case BOX_RIGHT:  xs = RC(-sz /  sx); ys = R2(-sy /  sx); break;
				case BOX_TOP:    xs = RC( sx /  sy); ys = R2( sz /  sy); break;
				case BOX_BOTTOM: xs = RC( sx / -sy); ys = R2( sz /  sy); break;
			}

			xs = bound (0, xs, 1);
			ys = bound (0, ys, 1);
			int sxs = xs * (cube_size - 1);
			int sys = ys * (cube_size - 1);
			sw_framebuffer_t *fb = cube[side].buffer;
			*buf++ = fb->color + sys * fb->rowbytes + sxs;
		}
	}
}

static void
renderlookup (byte **offs)
{
	framebuffer_t *fb = sw_ctx->framebuffer;
	sw_framebuffer_t *swfb = fb->buffer;
	byte       *p = swfb->color;
	unsigned x, y;
	for (y = 0; y < fb->height; y++) {
		for (x = 0; x < fb->width; x++, offs++)
		    p[x] = **offs;
		p += swfb->rowbytes;
	}
}

void
R_RenderFisheye (framebuffer_t *cube)
{
	int width = r_refdef.vrect.width;
	int height = r_refdef.vrect.height;
	float fov = scr_ffov->value;
	static int pwidth = -1;
	static int pheight = -1;
	static int pfov = -1;
	static unsigned psize = -1;
	static byte **offs = NULL;

	if (fov < 1) fov = 1;

	if (pwidth != width || pheight != height || pfov != fov
		|| psize != cube->width) {
		if (offs) free (offs);
		offs = malloc (width*height*sizeof(byte*));
		SYS_CHECKMEM (offs);
		pwidth = width;
		pheight = height;
		pfov = fov;
		psize = cube->width;
		fisheyelookuptable (offs, width, height, cube, fov*M_PI/180.0);
	}
#if 0
	sw_framebuffer_t *fb = cube[0].buffer;
	int         rowbytes = fb->rowbytes;
	int         csize = cube[0].width;
	for (int s = 0; s < 6; s++) {
		fb = cube[s].buffer;
		memset (fb->color, 31, csize);
		memset (fb->color + (csize - 1) * rowbytes, 31, csize);
		for (int y = 0; y < csize; y++) {
			fb->color[y * rowbytes] = 31;
			fb->color[y * rowbytes + csize - 1] = 31;
		}
	}
#endif
	renderlookup (offs);
}
