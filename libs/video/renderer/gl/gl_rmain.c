/*
	gl_rmain.c

	(no description)

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
#include "QF/draw.h"
#include "QF/mathlib.h"
#include "QF/qargs.h"
#include "QF/render.h"
#include "QF/skin.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "QF/scene/entity.h"

#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_alias.h"
#include "QF/GL/qf_draw.h"
#include "QF/GL/qf_iqm.h"
#include "QF/GL/qf_particles.h"
#include "QF/GL/qf_rlight.h"
#include "QF/GL/qf_rmain.h"
#include "QF/GL/qf_rsurf.h"
#include "QF/GL/qf_sprite.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "r_internal.h"
#include "varrays.h"
#include "vid_gl.h"

qboolean    gl_envmap;					// true during envmap command capture

int         gl_mirrortexturenum;		// quake texturenum, not gltexturenum
plane_t    *gl_mirror_plane;

float       gl_r_world_matrix[16];
//FIXME static float r_base_world_matrix[16];

//vec3_t		gl_shadecolor;					// Ender (Extend) Colormod
float		gl_modelalpha;					// Ender (Extend) Alpha

/* Unknown renamed to GLErr_Unknown to solve conflict with winioctl.h */
static unsigned int GLErr_InvalidEnum;
static unsigned int GLErr_InvalidValue;
static unsigned int GLErr_InvalidOperation;
static unsigned int GLErr_OutOfMemory;
static unsigned int GLErr_StackOverflow;
static unsigned int GLErr_StackUnderflow;
static unsigned int GLErr_Unknown;

static unsigned int
R_TestErrors (unsigned int numerous)
{
	switch (qfglGetError ()) {
	case GL_NO_ERROR:
		return numerous;
		break;
	case GL_INVALID_ENUM:
		GLErr_InvalidEnum++;
		R_TestErrors (numerous++);
		break;
	case GL_INVALID_VALUE:
		GLErr_InvalidValue++;
		R_TestErrors (numerous++);
		break;
	case GL_INVALID_OPERATION:
		GLErr_InvalidOperation++;
		R_TestErrors (numerous++);
		break;
	case GL_STACK_OVERFLOW:
		GLErr_StackOverflow++;
		R_TestErrors (numerous++);
		break;
	case GL_STACK_UNDERFLOW:
		GLErr_StackUnderflow++;
		R_TestErrors (numerous++);
		break;
	case GL_OUT_OF_MEMORY:
		GLErr_OutOfMemory++;
		R_TestErrors (numerous++);
		break;
	default:
		GLErr_Unknown++;
		R_TestErrors (numerous++);
		break;
	}

	return numerous;
}

static void
R_DisplayErrors (void)
{
	if (GLErr_InvalidEnum)
		printf ("%d OpenGL errors: Invalid Enum!\n", GLErr_InvalidEnum);
	if (GLErr_InvalidValue)
		printf ("%d OpenGL errors: Invalid Value!\n", GLErr_InvalidValue);
	if (GLErr_InvalidOperation)
		printf ("%d OpenGL errors: Invalid Operation!\n", GLErr_InvalidOperation);
	if (GLErr_StackOverflow)
		printf ("%d OpenGL errors: Stack Overflow!\n", GLErr_StackOverflow);
	if (GLErr_StackUnderflow)
		printf ("%d OpenGL errors: Stack Underflow\n!", GLErr_StackUnderflow);
	if (GLErr_OutOfMemory)
		printf ("%d OpenGL errors: Out Of Memory!\n", GLErr_OutOfMemory);
	if (GLErr_Unknown)
		printf ("%d Unknown OpenGL errors!\n", GLErr_Unknown);
}

static void
R_ClearErrors (void)
{
	GLErr_InvalidEnum = 0;
	GLErr_InvalidValue = 0;
	GLErr_InvalidOperation = 0;
	GLErr_OutOfMemory = 0;
	GLErr_StackOverflow = 0;
	GLErr_StackUnderflow = 0;
	GLErr_Unknown = 0;
}

void
glrmain_init (void)
{
	gldepthmin = 0;
	gldepthmax = 1;
	qfglDepthFunc (GL_LEQUAL);
	qfglDepthRange (gldepthmin, gldepthmax);
	if (gl_multitexture)
		gl_multitexture_f (gl_multitexture);
	if (gl_overbright)
		gl_overbright_f (gl_overbright);
}

void
gl_R_RotateForEntity (entity_t *e)
{
	mat4f_t     mat;
	Transform_GetWorldMatrix (e->transform, mat);
	qfglMultMatrixf (&mat[0][0]);
}

/*
	R_DrawEntitiesOnList

	Draw all the entities we have information on.
*/
static void
R_DrawEntitiesOnList (void)
{
	if (!r_drawentities->int_val)
		return;

	// LordHavoc: split into 3 loops to simplify state changes

	if (gl_mtex_active_tmus >= 2) {
		qglActiveTexture (gl_mtex_enum + 1);
		qfglEnable (GL_TEXTURE_2D);
		qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
		qfglDisable (GL_TEXTURE_2D);
		qglActiveTexture (gl_mtex_enum + 0);
	}
	if (gl_affinemodels->int_val)
		qfglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	if (gl_tess)
		qfglEnable (GL_PN_TRIANGLES_ATI);
	qfglEnable (GL_CULL_FACE);

	if (gl_vector_light->int_val) {
		qfglEnable (GL_LIGHTING);
		qfglEnable (GL_NORMALIZE);
	} else if (gl_tess) {
		qfglEnable (GL_NORMALIZE);
	}

	for (size_t i = 0; i < r_ent_queue->ent_queues[mod_alias].size; i++) { \
		entity_t   *ent = r_ent_queue->ent_queues[mod_alias].a[i]; \
		gl_R_DrawAliasModel (ent);
	}
	qfglColor3ubv (color_white);

	qfglDisable (GL_NORMALIZE);
	qfglDisable (GL_LIGHTING);

	if (gl_tess)
		qfglDisable (GL_PN_TRIANGLES_ATI);
	if (gl_affinemodels->int_val)
		qfglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_DONT_CARE);
	if (gl_mtex_active_tmus >= 2) { // FIXME: Ugly, but faster than cleaning
									// up in every R_DrawAliasModel()!
		qglActiveTexture (gl_mtex_enum + 1);
		qfglEnable (GL_TEXTURE_2D);
		if (gl_combine_capable && gl_overbright->int_val) {
			qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
			qfglTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
			qfglTexEnvf (GL_TEXTURE_ENV, GL_RGB_SCALE, gl_rgb_scale);
		} else {
			qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		}
		qfglDisable (GL_TEXTURE_2D);

		qglActiveTexture (gl_mtex_enum + 0);
	}

	for (size_t i = 0; i < r_ent_queue->ent_queues[mod_iqm].size; i++) { \
		entity_t   *ent = r_ent_queue->ent_queues[mod_iqm].a[i]; \
		gl_R_DrawIQMModel (ent);
	}
	qfglColor3ubv (color_white);

	qfglDisable (GL_CULL_FACE);
	qfglEnable (GL_ALPHA_TEST);
	if (gl_va_capable)
		qfglInterleavedArrays (GL_T2F_C4UB_V3F, 0, gl_spriteVertexArray);
	for (size_t i = 0; i < r_ent_queue->ent_queues[mod_sprite].size; i++) { \
		entity_t   *ent = r_ent_queue->ent_queues[mod_sprite].a[i]; \
		gl_R_DrawSpriteModel (ent);
	}
	qfglDisable (GL_ALPHA_TEST);
}

static void
R_DrawViewModel (void)
{
	entity_t   *ent = vr_data.view_model;
	if (vr_data.inhibit_viewmodel
		|| !r_drawviewmodel->int_val
		|| gl_envmap
		|| !r_drawentities->int_val
		|| !ent->renderer.model)
		return;

	// hack the depth range to prevent view model from poking into walls
	qfglDepthRange (gldepthmin, gldepthmin + 0.3 * (gldepthmax - gldepthmin));
	qfglEnable (GL_CULL_FACE);

	if (gl_vector_light->int_val) {
		qfglEnable (GL_LIGHTING);
		qfglEnable (GL_NORMALIZE);
	} else if (gl_tess) {
		qfglEnable (GL_NORMALIZE);
	}

	if (gl_affinemodels->int_val)
		qfglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	if (gl_mtex_active_tmus >= 2) {
		qglActiveTexture (gl_mtex_enum + 1);
		qfglEnable (GL_TEXTURE_2D);
		qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
		qfglDisable (GL_TEXTURE_2D);
		qglActiveTexture (gl_mtex_enum + 0);
	}

	gl_R_DrawAliasModel (ent);

	qfglColor3ubv (color_white);
	if (gl_mtex_active_tmus >= 2) { // FIXME: Ugly, but faster than cleaning
									// up in every R_DrawAliasModel()!
		qglActiveTexture (gl_mtex_enum + 1);
		qfglEnable (GL_TEXTURE_2D);
		if (gl_combine_capable && gl_overbright->int_val) {
			qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
			qfglTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
			qfglTexEnvf (GL_TEXTURE_ENV, GL_RGB_SCALE, gl_rgb_scale);
		} else {
			qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		}
		qfglDisable (GL_TEXTURE_2D);

		qglActiveTexture (gl_mtex_enum + 0);
	}
	if (gl_affinemodels->int_val)
		qfglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_DONT_CARE);

	qfglDisable (GL_NORMALIZE);
	qfglDisable (GL_LIGHTING);


	qfglDisable (GL_CULL_FACE);
	qfglDepthRange (gldepthmin, gldepthmax);
}

static void
gl_R_SetupFrame (void)
{
	R_AnimateLight ();
	EntQueue_Clear (r_ent_queue);
	r_framecount++;

	vec4f_t     position = r_refdef.frame.position;

	R_SetFrustum ();

	r_refdef.viewleaf = Mod_PointInLeaf (&position[0], r_refdef.worldmodel);
}

static void
MYgluPerspective (GLdouble fovy, GLdouble aspect, GLdouble zNear,
				  GLdouble zFar)
{
	GLdouble    xmin, xmax, ymin, ymax;

	ymax = zNear * tan (fovy * M_PI / 360.0);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = -xmin;

//	printf ("glFrustum (%f, %f, %f, %f)\n", xmin, xmax, ymin, ymax);
	qfglFrustum (xmin, xmax, ymin, ymax, zNear, zFar);
}

static void
R_SetupGL_Viewport_and_Perspective (void)
{
	float		screenaspect;
	int			x, y2, w, h;

	// set up viewpoint
	qfglMatrixMode (GL_PROJECTION);
	qfglLoadIdentity ();

	if (gl_envmap) {
		x = y2 = 0;
		w = h = 256;
	} else {
		x = r_refdef.vrect.x;
		y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height));

		w = r_refdef.vrect.width;
		h = r_refdef.vrect.height;
	}
//	printf ("glViewport(%d, %d, %d, %d)\n", glx + x, gly + y2, w, h);
	qfglViewport (x, y2, w, h);
	screenaspect = r_refdef.vrect.width / (float) r_refdef.vrect.height;
	MYgluPerspective (r_refdef.fov_y, screenaspect, r_nearclip->value,
					  r_farclip->value);
}

static void
R_SetupGL (void)
{
	R_SetupGL_Viewport_and_Perspective ();

	qfglFrontFace (GL_CW);

	qfglMatrixMode (GL_MODELVIEW);
	qfglLoadIdentity ();

	static mat4f_t z_up = {
		{ 0, 0, -1, 0},
		{-1, 0,  0, 0},
		{ 0, 1,  0, 0},
		{ 0, 0,  0, 1},
	};
	mat4f_t view;
	mmulf (view, z_up, r_refdef.camera_inverse);
	qfglLoadMatrixf (&view[0][0]);

	qfglGetFloatv (GL_MODELVIEW_MATRIX, gl_r_world_matrix);

	// set drawing parms
//	qfglEnable (GL_CULL_FACE);
	qfglDisable (GL_ALPHA_TEST);
	qfglAlphaFunc (GL_GREATER, 0.5);
	qfglEnable (GL_DEPTH_TEST);
	if (gl_dlight_smooth->int_val)
		qfglShadeModel (GL_SMOOTH);
	else
		qfglShadeModel (GL_FLAT);
}

static void
R_RenderScene (void)
{
	if (r_timegraph->int_val || r_speeds->int_val || r_dspeeds->int_val)
		r_time1 = Sys_DoubleTime ();

	gl_R_SetupFrame ();
	R_SetupGL ();
	gl_Fog_EnableGFog ();

	R_MarkLeaves ();			// done here so we know if we're in water
	R_PushDlights (vec3_origin);
	gl_R_DrawWorld ();				// adds static entities to the list
	S_ExtraUpdate ();			// don't let sound get messed up if going slow
	R_DrawEntitiesOnList ();
	gl_R_RenderDlights ();
	gl_R_DrawWaterSurfaces ();
	R_DrawViewModel ();
	gl_R_DrawParticles ();

	gl_Fog_DisableGFog ();

	if (R_TestErrors (0))
		R_DisplayErrors ();
	R_ClearErrors ();
}

/*
	R_RenderView_

	r_refdef must be set before the first call
*/
static void
R_RenderView_ (void)
{
	if (r_norefresh->int_val) {
		return;
	}
	if (!r_refdef.worldmodel) {
		return;
	}

	// render normal view
	R_RenderScene ();

	if (r_timegraph->int_val)
		R_TimeGraph ();
	if (r_zgraph->int_val)
		R_ZGraph ();
}

static void R_RenderViewFishEye (void);

void
gl_R_RenderView (void)
{
	if(!scr_fisheye->int_val)
		R_RenderView_ ();
	else
		R_RenderViewFishEye ();
}

//FIXME
// Algorithm:
// Draw up to six views, one in each direction.
// Save the picture to cube map texture, use GL_ARB_texture_cube_map.
// Create FPOLYCNTxFPOLYCNT polygons sized flat grid.
// Baseing on field of view, tie cube map texture to grid using
// translation function to map texture coordinates to fixed/regular
// grid vertices coordinates.
// Render view. Fisheye is done.

#define BOX_FRONT  0
#define BOX_RIGHT  1
#define BOX_BEHIND 2
#define BOX_LEFT   3
#define BOX_TOP    4
#define BOX_BOTTOM 5

static mat4f_t box_rotations[] = {
	{ { 1, 0, 0, 0},		// front
	  { 0, 1, 0, 0},
	  { 0, 0, 1, 0},
	  { 0, 0, 0, 1} },
	{ { 0,-1, 0, 0},		// right
	  { 1, 0, 0, 0},
	  { 0, 0, 1, 0},
	  { 0, 0, 0, 1} },
	{ {-1, 0, 0, 0},		// back
	  { 0,-1, 0, 0},
	  { 0, 0, 1, 0},
	  { 0, 0, 0, 1} },
	{ { 0, 1, 0, 0},		// left
	  {-1, 0, 0, 0},
	  { 0, 0, 1, 0},
	  { 0, 0, 0, 1} },
	{ { 0, 0, 1, 0},		// top
	  { 0, 1, 0, 0},
	  {-1, 0, 0, 0},
	  { 0, 0, 0, 1} },
	{ { 0, 0,-1, 0},		// bottom
	  { 0, 1, 0, 0},
	  { 1, 0, 0, 0},
	  { 0, 0, 0, 1} },
};

#define FPOLYCNT   128

struct xyz {
	float x, y, z;
};

static struct xyz FisheyeLookupTbl[FPOLYCNT + 1][FPOLYCNT + 1];
static GLuint cube_map_tex;
static GLint gl_cube_map_size;
static GLint gl_cube_map_step;

static const GLenum box2cube_map[] = {
	GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB,
	GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB
};

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
			v->y = -sin (yaw) * sin (roll);
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


static void
R_RenderCubeSide (int side)
{
	mat4f_t     camera;
	mat4f_t     camera_inverse;
	mat4f_t     rotinv;

	memcpy (camera, r_refdef.camera, sizeof (camera));
	memcpy (camera_inverse, r_refdef.camera_inverse, sizeof (camera_inverse));
	mmulf (r_refdef.camera, camera, box_rotations[side]);
	mat4ftranspose (rotinv, box_rotations[side]);
	mmulf (r_refdef.camera_inverse, rotinv, camera_inverse);

	//FIXME see fixme in r_screen.c
	r_refdef.frame.mat[0] = -r_refdef.camera[1];
	r_refdef.frame.mat[1] =  r_refdef.camera[0];
	r_refdef.frame.mat[2] =  r_refdef.camera[2];
	r_refdef.frame.mat[3] =  r_refdef.camera[3];
#if 0
	printf ("side: %d\n", side);
	printf ("c: " VEC4F_FMT "\n", MAT4_ROW(r_refdef.camera, 0));
	printf ("   " VEC4F_FMT "\n", MAT4_ROW(r_refdef.camera, 1));
	printf ("   " VEC4F_FMT "\n", MAT4_ROW(r_refdef.camera, 2));
	printf ("   " VEC4F_FMT "\n", MAT4_ROW(r_refdef.camera, 3));
	printf ("i: " VEC4F_FMT "\n", MAT4_ROW(r_refdef.camera_inverse, 0));
	printf ("   " VEC4F_FMT "\n", MAT4_ROW(r_refdef.camera_inverse, 1));
	printf ("   " VEC4F_FMT "\n", MAT4_ROW(r_refdef.camera_inverse, 2));
	printf ("   " VEC4F_FMT "\n", MAT4_ROW(r_refdef.camera_inverse, 3));
	printf ("f: " VEC4F_FMT "\n", MAT4_ROW(r_refdef.frame.mat, 0));
	printf ("   " VEC4F_FMT "\n", MAT4_ROW(r_refdef.frame.mat, 1));
	printf ("   " VEC4F_FMT "\n", MAT4_ROW(r_refdef.frame.mat, 2));
	printf ("   " VEC4F_FMT "\n", MAT4_ROW(r_refdef.frame.mat, 3));
#endif
	qfglClear (GL_DEPTH_BUFFER_BIT);
	R_RenderView_ ();
	qfglEnable (GL_TEXTURE_CUBE_MAP_ARB);
	qfglBindTexture (GL_TEXTURE_CUBE_MAP_ARB, cube_map_tex);
	qfglCopyTexSubImage2D (box2cube_map[side], 0, 0, 0, 0, 0,
						   gl_cube_map_size, gl_cube_map_size);
	CHKGLERR ("qfglCopyTexSubImage2D");
	qfglDisable (GL_TEXTURE_CUBE_MAP_ARB);

	memcpy (r_refdef.camera, camera, sizeof (camera));
	memcpy (r_refdef.camera_inverse, camera_inverse, sizeof (camera_inverse));
}

static qboolean gl_cube_map_capable = false;
static GLint gl_cube_map_maxtex;
static GLuint fisheye_grid;

static int
R_InitFishEyeOnce (void)
{
	static qboolean fisheye_init_once_completed = false;

	if (fisheye_init_once_completed)
		return 1;
	Sys_MaskPrintf (SYS_dev, "GL_ARB_texture_cube_map ");
	if (QFGL_ExtensionPresent ("GL_ARB_texture_cube_map")) {
		qfglGetIntegerv (GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB,
						 &gl_cube_map_maxtex);
		Sys_MaskPrintf (SYS_dev, "present, max texture size %d.\n",
						(int) gl_cube_map_maxtex);
		gl_cube_map_capable = true;
	} else {
		Sys_MaskPrintf (SYS_dev, "not found.\n");
		gl_cube_map_capable = false;
	}
	fisheye_init_once_completed = true;
	return 1;
}

static int
R_InitFishEye (void)
{
	int width = vid.width;
	int height = vid.height;
	int fov = scr_ffov->int_val;
	int views = scr_fviews->int_val;

	static int pwidth = -1;
	static int pheight = -1;
	static int pfov = -1;
	static int pviews = -1;

	int i, x, y, min_wh, wh_changed = 0;

	if (!R_InitFishEyeOnce())
		return 0;
	if (!gl_cube_map_capable)
		return 0;

	// There is a problem when max texture size is bigger than
	// min(width, height), it shows up as black fat stripes at the edges
	// of box polygons, probably due to missing texture fragment. Try
	// to play in 640x480 with gl_cube_map_size == 512.
	if (pwidth != width || pheight != height) {
		wh_changed = 1;
		min_wh = (height < width) ? height : width;
		gl_cube_map_size = gl_cube_map_maxtex;
		while (gl_cube_map_size > min_wh)
			gl_cube_map_size /= 2;
		gl_cube_map_step = gl_cube_map_size / FPOLYCNT;
	}
	if (pviews != views) {
		qfglEnable (GL_TEXTURE_CUBE_MAP_ARB);
		if (pviews != -1)
			qfglDeleteTextures (1, &cube_map_tex);
		pviews = views;
		qfglGenTextures (1, &cube_map_tex);
		qfglBindTexture (GL_TEXTURE_CUBE_MAP_ARB, cube_map_tex);
		for (i = 0; i < 6; ++i) {
			qfglTexImage2D (box2cube_map[i], 0, 3, gl_cube_map_size,
							gl_cube_map_size, 0, GL_RGB, GL_UNSIGNED_SHORT,
							NULL);
		}
		qfglTexParameteri (GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MAG_FILTER,
						   GL_LINEAR);
		qfglTexParameteri (GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER,
						   GL_LINEAR);
		qfglDisable (GL_TEXTURE_CUBE_MAP_ARB);
	}
	if (wh_changed || pfov != fov) {
		if (pfov != -1)
			qfglDeleteLists (fisheye_grid, 1);
		pwidth = width;
		pheight = height;
		pfov = fov;

		R_BuildFisheyeLookup (gl_cube_map_size, gl_cube_map_size,
							  ((float) fov) * M_PI / 180.0);

		fisheye_grid = qfglGenLists (1);
		qfglNewList (fisheye_grid, GL_COMPILE);
		qfglLoadIdentity ();
		qfglTranslatef (-gl_cube_map_size / 2, -gl_cube_map_size / 2,
						-gl_cube_map_size / 2);

		qfglDisable (GL_DEPTH_TEST);
		qfglFrontFace (GL_CCW);
		qfglClear (GL_COLOR_BUFFER_BIT);

		qfglEnable (GL_TEXTURE_CUBE_MAP_ARB);
		qfglBindTexture (GL_TEXTURE_CUBE_MAP_ARB, cube_map_tex);
		for (y = 0; y < gl_cube_map_size; y += gl_cube_map_step) {
			qfglBegin (GL_QUAD_STRIP);
			for (x = 0; x <= gl_cube_map_size; x += gl_cube_map_step) { // quad_strip, X should be inclusive
				struct xyz *v = &FisheyeLookupTbl[x / gl_cube_map_step]
					[y / gl_cube_map_step + 1];
				qfglTexCoord3f (v->x, v->y, v->z);
				qfglVertex2i (x, y + gl_cube_map_step);
				--v;
				qfglTexCoord3f (v->x, v->y, v->z);
				qfglVertex2i (x, y);
			}
			qfglEnd ();
		}
		qfglDisable (GL_TEXTURE_CUBE_MAP_ARB);
		qfglEnable (GL_DEPTH_TEST);
		qfglEndList ();
	}
	return 1;
}

static void
R_RenderViewFishEye (void)
{
	float s_fov_x, s_fov_y;
	int s_vid_w, s_vid_h, s_rect_w, s_rect_h;

	if (!R_InitFishEye()) return;

	// save values
	s_fov_x = r_refdef.fov_x;
	s_fov_y = r_refdef.fov_y;
	s_vid_w = vid.width;
	s_vid_h = vid.height;
	s_rect_w = r_refdef.vrect.width;
	s_rect_h = r_refdef.vrect.height;
	// the view should be square
	r_refdef.fov_x = r_refdef.fov_y = 90;
	vid.width = vid.height =
		r_refdef.vrect.height = r_refdef.vrect.width =
		gl_cube_map_size;
	switch (scr_fviews->int_val) {
		case 6:   R_RenderCubeSide (BOX_BEHIND);
		case 5:   R_RenderCubeSide (BOX_BOTTOM);
		case 4:   R_RenderCubeSide (BOX_TOP);
		case 3:   R_RenderCubeSide (BOX_LEFT);
		case 2:   R_RenderCubeSide (BOX_RIGHT);
		default:  R_RenderCubeSide (BOX_FRONT);
	}
	// restore
	r_refdef.fov_x = s_fov_x;
	r_refdef.fov_y = s_fov_y;
	vid.width = s_vid_w;
	vid.height = s_vid_h;
	r_refdef.vrect.width = s_rect_w;
	r_refdef.vrect.height = s_rect_h;
	R_SetupGL_Viewport_and_Perspective ();
	qfglMatrixMode (GL_MODELVIEW);
	qfglCallList (fisheye_grid);
}

void
gl_R_ClearState (void)
{
	r_refdef.worldmodel = 0;
	R_ClearEfrags ();
	R_ClearDlights ();
	R_ClearParticles ();
}
