/*
	gl_fisheye.c

	GL fisheye rendering

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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "QF/cvar.h"
#include "QF/mathlib.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_draw.h"
#include "QF/GL/qf_fisheye.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "r_internal.h"
#include "varrays.h"
#include "vid_gl.h"

// Algorithm:
// Draw up to six views, one in each direction.
// Save the picture to cube map texture, use GL_ARB_texture_cube_map.
// Create FPOLYCNTxFPOLYCNT polygons sized flat grid.
// Baseing on field of view, tie cube map texture to grid using
// translation function to map texture coordinates to fixed/regular
// grid vertices coordinates.
// Render view. Fisheye is done.

#define FPOLYCNT   128

struct xyz {
	float x, y, z;
};

static struct xyz FisheyeLookupTbl[FPOLYCNT + 1][FPOLYCNT + 1];
//static GLint gl_cube_map_size;
static GLint gl_cube_map_step;

static void
R_BuildFisheyeLookup (int width, int height, float fov)
{
	int x, y;
	struct xyz *v;

	for (y = 0; y <= height; y += gl_cube_map_step) {
		for (x = 0; x <= width; x += gl_cube_map_step) {
			float dx = x - width / 2;
			float dy = y - height / 2;
			float yaw = sqrt (dx * dx + dy * dy) * fov / width;
			float roll = atan2 (dy, dx);
			// X is a first index and Y is a second, because later
			// when we draw QUAD_STRIPs we need next Y vertex coordinate.
			v = &FisheyeLookupTbl[x / gl_cube_map_step][y / gl_cube_map_step];
			v->x = sin (yaw) * cos (roll);
			v->y = sin (yaw) * sin (roll);
			v->z = cos (yaw);
		}
	}
}

#define CHKGLERR(s) \
do { \
	GLint err = qfglGetError(); \
	if (err != GL_NO_ERROR) \
		printf ("%s: gl error %d\n", s, (int) err); \
} while (0);


static GLuint fisheye_grid;

void
gl_InitFisheye (void)
{
	fisheye_grid = qfglGenLists (1);
}

static void
build_display_list (int width, int height, float fov)
{
	int         maxd = max (width, height);
	gl_cube_map_step = maxd / FPOLYCNT;

	R_BuildFisheyeLookup (width, height, fov * M_PI / 180.0);

	qfglNewList (fisheye_grid, GL_COMPILE);

	qfglDisable (GL_DEPTH_TEST);
	qfglFrontFace (GL_CCW);
	qfglClear (GL_COLOR_BUFFER_BIT);

	for (int y = 0; y < height; y += gl_cube_map_step) {
		qfglBegin (GL_QUAD_STRIP);
		// quad_strip, so X is inclusive
		for (int x = 0; x <= width; x += gl_cube_map_step) {
			int         xind = x / gl_cube_map_step;
			int         yind = y / gl_cube_map_step;
			struct xyz *v = &FisheyeLookupTbl[xind][yind + 1];
			qfglTexCoord3f (v->x, v->y, v->z);
			qfglVertex2i (x, y + gl_cube_map_step);
			--v;
			qfglTexCoord3f (v->x, v->y, v->z);
			qfglVertex2i (x, y);
		}
		qfglEnd ();
	}
	qfglEnable (GL_DEPTH_TEST);
	qfglEndList ();
	CHKGLERR("build list");
}

void
gl_FisheyeScreen (framebuffer_t *fb)
{
	static int pwidth = -1;
	static int pheight = -1;
	static float pfov = -1;

	if (pwidth != r_refdef.vrect.width || pheight != r_refdef.vrect.height
		|| pfov != scr_ffov) {
		pwidth = r_refdef.vrect.width;
		pheight = r_refdef.vrect.height;
		pfov = scr_ffov;

		build_display_list (pwidth, pheight, pfov);
	}

	gl_framebuffer_t *buffer = fb->buffer;

	qfglMatrixMode (GL_PROJECTION);
	qfglLoadIdentity ();
	qfglOrtho (0, r_refdef.vrect.width, r_refdef.vrect.height, 0, -9999, 9999);
	qfglMatrixMode (GL_MODELVIEW);
	qfglLoadIdentity ();
	qfglColor3ubv (color_white);

	qfglEnable (GL_TEXTURE_CUBE_MAP_ARB);
	qfglBindTexture (GL_TEXTURE_CUBE_MAP_ARB, buffer->color);
	qfglCallList (fisheye_grid);
	qfglDisable (GL_TEXTURE_CUBE_MAP_ARB);
	CHKGLERR("fisheye");
}
