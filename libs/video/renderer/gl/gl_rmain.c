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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

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
#include "QF/locs.h"
#include "QF/mathlib.h"
#include "QF/qargs.h"
#include "QF/render.h"
#include "QF/skin.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/vid.h"
#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_rlight.h"
#include "QF/GL/qf_rmain.h"
#include "QF/GL/qf_rsurf.h"
#include "QF/GL/qf_screen.h"
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "gl_draw.h"
#include "r_cvar.h"
#include "r_dynamic.h"
#include "r_local.h"
#include "varrays.h"
#include "view.h"

entity_t    r_worldentity;

qboolean    r_cache_thrash;				// compatability

vec3_t      modelorg, r_entorigin;
entity_t   *currententity;

int         r_visframecount;			// bumped when going to a new PVS
VISIBLE int         r_framecount;				// used for dlight push checking

int         c_brush_polys, c_alias_polys;

qboolean    envmap;						// true during envmap command capture

int         mirrortexturenum;			// quake texturenum, not gltexturenum
qboolean    mirror;
mplane_t   *mirror_plane;

// view origin
VISIBLE vec3_t      vup;
VISIBLE vec3_t      vpn;
VISIBLE vec3_t      vright;
VISIBLE vec3_t      r_origin;

float       r_world_matrix[16];
float       r_base_world_matrix[16];

// screen size info
VISIBLE refdef_t    r_refdef;

mleaf_t    *r_viewleaf, *r_oldviewleaf;

int         d_lightstylevalue[256];		// 8.8 fraction of base light value

vec3_t		shadecolor;					// Ender (Extend) Colormod
float		modelalpha;					// Ender (Extend) Alpha

/* Unknown renamed to GLErr_Unknown to solve conflict with winioctl.h */
unsigned int	GLErr_InvalidEnum;
unsigned int	GLErr_InvalidValue;
unsigned int	GLErr_InvalidOperation;
unsigned int	GLErr_OutOfMemory;
unsigned int	GLErr_StackOverflow;
unsigned int	GLErr_StackUnderflow;
unsigned int	GLErr_Unknown;

extern void (*R_DrawSpriteModel) (struct entity_s *ent);


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
R_RotateForEntity (entity_t *e)
{
	qfglTranslatef (e->origin[0], e->origin[1], e->origin[2]);

	qfglRotatef (e->angles[1], 0, 0, 1);
	qfglRotatef (-e->angles[0], 0, 1, 0);
	// ZOID: fixed z angle
	qfglRotatef (e->angles[2], 1, 0, 0);
}

/*
	R_DrawEntitiesOnList

	Draw all the entities we have information on.
*/
static void
R_DrawEntitiesOnList (void)
{
	int		i;

	if (!r_drawentities->int_val)
		return;

	// LordHavoc: split into 3 loops to simplify state changes
	for (i = 0; i < r_numvisedicts; i++) {
		if (r_visedicts[i]->model->type != mod_brush)
			continue;
		currententity = r_visedicts[i];

		R_DrawBrushModel (currententity);
	}

	if (gl_mtex_active_tmus >= 2) {
		qglActiveTexture (gl_mtex_enum + 1);
		qfglEnable (GL_TEXTURE_2D);
		qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
		qfglDisable (GL_TEXTURE_2D);
		qglActiveTexture (gl_mtex_enum + 0);
	}
	if (gl_affinemodels->int_val)
		qfglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	if (tess)
		qfglEnable (GL_PN_TRIANGLES_ATI);
	qfglEnable (GL_CULL_FACE);
	
	if (gl_vector_light->int_val) {
		qfglEnable (GL_LIGHTING);
		qfglEnable (GL_NORMALIZE);
	} else if (tess) {
		qfglEnable (GL_NORMALIZE);
	}
	
	for (i = 0; i < r_numvisedicts; i++) {
		if (r_visedicts[i]->model->type != mod_alias)
			continue;
		currententity = r_visedicts[i];

		R_DrawAliasModel (currententity);
	}
	qfglColor3ubv (color_white);
	
	qfglDisable (GL_NORMALIZE);
	qfglDisable (GL_LIGHTING);

	qfglDisable (GL_CULL_FACE);
	if (tess)
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
			qfglTexEnvf (GL_TEXTURE_ENV, GL_RGB_SCALE, rgb_scale);
		} else {
			qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		}
		qfglDisable (GL_TEXTURE_2D);

		qglActiveTexture (gl_mtex_enum + 0);
	}

	qfglEnable (GL_ALPHA_TEST);
	if (gl_va_capable)
		qfglInterleavedArrays (GL_T2F_C4UB_V3F, 0, spriteVertexArray);
	for (i = 0; i < r_numvisedicts; i++) {
		if (r_visedicts[i]->model->type != mod_sprite)
			continue;
		currententity = r_visedicts[i];

		R_DrawSpriteModel (currententity);
	}
	qfglDisable (GL_ALPHA_TEST);
}

static void
R_DrawViewModel (void)
{
	currententity = r_view_model;
	if (r_inhibit_viewmodel
		|| !r_drawviewmodel->int_val
		|| envmap
		|| !r_drawentities->int_val
		|| !currententity->model)
		return;

	// hack the depth range to prevent view model from poking into walls
	qfglDepthRange (gldepthmin, gldepthmin + 0.3 * (gldepthmax - gldepthmin));
	qfglEnable (GL_CULL_FACE);

	if (gl_vector_light->int_val) {
		qfglEnable (GL_LIGHTING);
		qfglEnable (GL_NORMALIZE);
	} else if (tess) {
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

	R_DrawAliasModel (currententity);

	qfglColor3ubv (color_white);
	if (gl_mtex_active_tmus >= 2) { // FIXME: Ugly, but faster than cleaning
									// up in every R_DrawAliasModel()!
		qglActiveTexture (gl_mtex_enum + 1);
		qfglEnable (GL_TEXTURE_2D);
		if (gl_combine_capable && gl_overbright->int_val) {
			qfglTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
			qfglTexEnvf (GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
			qfglTexEnvf (GL_TEXTURE_ENV, GL_RGB_SCALE, rgb_scale);
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

static inline int
SignbitsForPlane (mplane_t *out)
{
	int		bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j = 0; j < 3; j++) {
		if (out->normal[j] < 0)
			bits |= 1 << j;
	}
	return bits;
}

static void
R_SetFrustum (void)
{
	int         i;

	// rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector (frustum[0].normal, vup, vpn,
							 -(90 - r_refdef.fov_x / 2));
	// rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector (frustum[1].normal, vup, vpn,
							 90 - r_refdef.fov_x / 2);
	// rotate VPN up by FOV_Y/2 degrees
	RotatePointAroundVector (frustum[2].normal, vright, vpn,
							 90 - r_refdef.fov_y / 2);
	// rotate VPN down by FOV_Y/2 degrees
	RotatePointAroundVector (frustum[3].normal, vright, vpn,
							 -(90 - r_refdef.fov_y / 2));

	for (i = 0; i < 4; i++) {
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}

void
R_SetupFrame (void)
{
	R_AnimateLight ();

	r_framecount++;

	// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

	// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf (r_origin, r_worldentity.model);

	V_SetContentsColor (r_viewleaf->contents);

	r_cache_thrash = false;

	c_brush_polys = 0;
	c_alias_polys = 0;
}

static void
MYgluPerspective (GLdouble fovy, GLdouble aspect, GLdouble zNear,
				  GLdouble zFar)
{
	GLdouble		xmin, xmax, ymin, ymax;

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

	if (envmap) {
		x = y2 = 0;
		w = h = 256;
	} else {
		x = r_refdef.vrect.x;
		y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height));

		w = r_refdef.vrect.width;
		h = r_refdef.vrect.height;
	}
//	printf ("glViewport(%d, %d, %d, %d)\n", glx + x, gly + y2, w, h);
	qfglViewport (glx + x, gly + y2, w, h);
	screenaspect = (float) r_refdef.vrect.width / r_refdef.vrect.height;
	MYgluPerspective (r_refdef.fov_y, screenaspect, r_nearclip->value,
					  r_farclip->value);
}

static void
R_SetupGL (void)
{
	R_SetFrustum ();

	R_SetupGL_Viewport_and_Perspective ();

	if (mirror) {
		if (mirror_plane->normal[2])
			qfglScalef (1, -1, 1);
		else
			qfglScalef (-1, 1, 1);
		qfglCullFace (GL_BACK);
	} else
		qfglCullFace (GL_FRONT);

	qfglMatrixMode (GL_MODELVIEW);
	qfglLoadIdentity ();

	qfglRotatef (-90, 1, 0, 0);			// put Z going up
	qfglRotatef (90, 0, 0, 1);			// put Z going up
	qfglRotatef (-r_refdef.viewangles[2], 1, 0, 0);
	qfglRotatef (-r_refdef.viewangles[0], 0, 1, 0);
	qfglRotatef (-r_refdef.viewangles[1], 0, 0, 1);
	qfglTranslatef (-r_refdef.vieworg[0], -r_refdef.vieworg[1],
				  -r_refdef.vieworg[2]);

	qfglGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);

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
R_Clear (void)
{
	if (gl_clear->int_val)
		qfglClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	else
		qfglClear (GL_DEPTH_BUFFER_BIT);
}

static void
R_RenderScene (void)
{
	if (r_timegraph->int_val || r_speeds->int_val || r_dspeeds->int_val)
		r_time1 = Sys_DoubleTime ();

	R_SetupFrame ();
	R_SetupGL ();
	R_MarkLeaves ();			// done here so we know if we're in water
	R_PushDlights (vec3_origin);
	R_DrawWorld ();				// adds static entities to the list
	S_ExtraUpdate ();			// don't let sound get messed up if going slow
	R_DrawEntitiesOnList ();
	R_RenderDlights ();

	if (R_TestErrors (0))
		R_DisplayErrors ();
	R_ClearErrors ();
}

static void
R_Mirror (void)
{
	float		d;
	entity_t  **ent;
	msurface_t *s;

//	if (!mirror) // FIXME: Broken
		return;

	memcpy (r_base_world_matrix, r_world_matrix, sizeof (r_base_world_matrix));

	d = 2 * DotProduct (r_refdef.vieworg, mirror_plane->normal) -
		mirror_plane->dist;
	VectorMultSub (r_refdef.vieworg, d, mirror_plane->normal,
				   r_refdef.vieworg);

	d = 2 * DotProduct (vpn, mirror_plane->normal);
	VectorMultSub (vpn, d, mirror_plane->normal, vpn);

	r_refdef.viewangles[0] = -asin (vpn[2]) / M_PI * 180;
	r_refdef.viewangles[1] = atan2 (vpn[1], vpn[0]) / M_PI * 180;
	r_refdef.viewangles[2] = -r_refdef.viewangles[2];

	ent = R_NewEntity();
	if (ent)
		*ent = r_player_entity;

	gldepthmin = 0.5;
	gldepthmax = 1;
	qfglDepthRange (gldepthmin, gldepthmax);

	R_RenderScene ();
	R_DrawWaterSurfaces ();

	gldepthmin = 0;
	gldepthmax = 1;
	qfglDepthRange (gldepthmin, gldepthmax);

	// blend on top
	qfglMatrixMode (GL_PROJECTION);
	if (mirror_plane->normal[2])
		qfglScalef (1, -1, 1);
	else
		qfglScalef (-1, 1, 1);
	qfglCullFace (GL_FRONT);
	qfglMatrixMode (GL_MODELVIEW);

	qfglLoadMatrixf (r_base_world_matrix);

	color_white[3] = r_mirroralpha->value * 255;
	qfglColor4ubv (color_white);
	s = r_worldentity.model->textures[mirrortexturenum]->texturechain;
	for (; s; s = s->texturechain) {
		texture_t  *tex;

		if (!s->texinfo->texture->anim_total)
			tex = s->texinfo->texture;
		else
			tex = R_TextureAnimation (s);

// FIXME: Needs to set the texture, the tmu, and include the header, and then
//	clean up afterwards.
//		if (tex->gl_fb_texturenum && gl_mtex_fullbright
//			&& gl_fb_models->int_val) {
//			s->polys->fb_chain = fullbright_polys[tex->gl_fb_texturenum];
//			fullbright_polys[tex->gl_fb_texturenum] = s->polys;
//		}

		qfglBindTexture (GL_TEXTURE_2D, tex->gl_texturenum);
//		R_RenderBrushPoly (s, tex); // FIXME: Need to move R_Mirror to gl_rsurf.c, and uncommment this line!
	}
	r_worldentity.model->textures[mirrortexturenum]->texturechain = NULL;
	qfglColor3ubv (color_white);
}

/*
	R_RenderView_

	r_refdef must be set before the first call
*/
static void
R_RenderView_ (void)
{
	if (r_norefresh->int_val)
		return;
	if (!r_worldentity.model)
		Sys_Error ("R_RenderView: NULL worldmodel");

	mirror = false;

	R_Clear ();

	// render normal view
	R_RenderScene ();
	R_DrawViewModel ();
	R_DrawWaterSurfaces ();
	R_DrawParticles ();

	// render mirror view
	R_Mirror ();

	if (r_timegraph->int_val)
		R_TimeGraph ();
	if (r_zgraph->int_val)
		R_ZGraph ();
}

// Algorithm:
// Draw up to six views, one in each direction.
// Save the picture to cube map texture, use GL_ARB_texture_cube_map.
// Create FPOLYCNTxFPOLYCNT polygons sized flat grid.
// Baseing on field of view, tie cube map texture to grid using
// translation function to map texture coordinates to fixed/regular
// grid vertices coordinates.
// Render view. Fisheye is done.

static void R_RenderViewFishEye (void);

VISIBLE void
R_RenderView (void)
{
	if(!scr_fisheye->int_val)
		R_RenderView_ ();
	else
		R_RenderViewFishEye ();
	GL_Set2D ();
}

#define BOX_FRONT  0
#define BOX_RIGHT  1
#define BOX_BEHIND 2
#define BOX_LEFT   3
#define BOX_TOP    4
#define BOX_BOTTOM 5

#define FPOLYCNT   16

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

#define NO(x) \
do { \
	if (x < 0) \
		x += 360; \
	else if (x >= 360) \
		x -= 360; \
} while (0)

static void
R_RenderCubeSide (int side)
{
	float pitch, n_pitch;
	float yaw, n_yaw;
	float roll, n_roll;
	float s_roll;

	pitch = n_pitch = r_refdef.viewangles[PITCH];
	yaw  = n_yaw = r_refdef.viewangles[YAW];
	// setting ROLL for now to 0, correct roll handling
	// requre more exhaustive changes in rotation
	// TODO: implement via matrix
//	roll  = n_roll = r_refdef.viewangles[ROLL];
	s_roll = r_refdef.viewangles[ROLL];
	roll = n_roll = 0;
//	roll -= scr_fviews->int_val * 10;
//	n_roll = roll;

	switch (side) {
	case BOX_FRONT:
		break;
	case BOX_RIGHT:
		n_pitch = roll;
		n_yaw -= 90;
		n_roll = -pitch;
		break;
	case BOX_LEFT:
		n_pitch = -roll;
		n_yaw += 90;
		n_roll = pitch;
//		static int f = 0;
//		if (!(f++ % 100))
//				printf ("%4d %4d %4d | %4d %4d %4d\n", (int) pitch, (int) yaw,
//						(int) roll, (int) n_pitch, (int) n_yaw, (int) n_roll);
		break;
	case BOX_TOP:
		n_pitch -= 90;
		break;
	case BOX_BOTTOM:
		n_pitch += 90;
		break;
	case BOX_BEHIND:
		n_pitch = -pitch;
		n_yaw += 180;
		break;
	}
	NO (n_pitch);
	NO (n_yaw);
	NO (n_roll);
	r_refdef.viewangles[PITCH] = n_pitch;
	r_refdef.viewangles[YAW] = n_yaw;
	r_refdef.viewangles[ROLL] = n_roll;

	R_RenderView_ ();
	qfglEnable (GL_TEXTURE_CUBE_MAP_ARB);
	qfglBindTexture (GL_TEXTURE_CUBE_MAP_ARB, cube_map_tex);
	qfglCopyTexSubImage2D (box2cube_map[side], 0, 0, 0, 0, 0,
						   gl_cube_map_size, gl_cube_map_size);
//	CHKGLERR ("qfglCopyTexSubImage2D");
	qfglDisable (GL_TEXTURE_CUBE_MAP_ARB);

	r_refdef.viewangles[PITCH] = pitch;
	r_refdef.viewangles[YAW] = yaw;
	r_refdef.viewangles[ROLL] = s_roll;
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
	Sys_DPrintf ("GL_ARB_texture_cube_map ");
	if (QFGL_ExtensionPresent ("GL_ARB_texture_cube_map")) {
		qfglGetIntegerv (GL_MAX_CUBE_MAP_TEXTURE_SIZE_ARB,
						 &gl_cube_map_maxtex);
		Sys_DPrintf ("present, max texture size %d.\n",
					(int) gl_cube_map_maxtex);
		gl_cube_map_capable = true;
	} else {
		Sys_DPrintf ("not found.\n");
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
		qfglCullFace (GL_BACK);
		qfglClear (GL_COLOR_BUFFER_BIT);

		qfglEnable (GL_TEXTURE_CUBE_MAP_ARB);
		qfglBindTexture (GL_TEXTURE_CUBE_MAP_ARB, cube_map_tex);
		qfglBegin (GL_QUAD_STRIP);

		for (y = 0; y < gl_cube_map_size; y += gl_cube_map_step) {
			for (x = 0; x <= gl_cube_map_size; x += gl_cube_map_step) { // quad_strip, X should be inclusive
				struct xyz *v = &FisheyeLookupTbl[x / gl_cube_map_step]
					[y / gl_cube_map_step + 1];
				qfglTexCoord3f (v->x, v->y, v->z);
				qfglVertex2i (x, y + gl_cube_map_step);
				--v;
				qfglTexCoord3f (v->x, v->y, v->z);
				qfglVertex2i (x, y);
			}
		}
		qfglEnd ();
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
	int s_vid_w, s_vid_h, s_rect_w, s_rect_h, s_gl_w, s_gl_h;

	if (!R_InitFishEye()) return;

	// save values
	s_fov_x = r_refdef.fov_x;
	s_fov_y = r_refdef.fov_y;
	s_vid_w = vid.width;
	s_vid_h = vid.height;
	s_rect_w = r_refdef.vrect.width;
	s_rect_h = r_refdef.vrect.height;
	s_gl_w = glwidth;
	s_gl_h = glheight;
	// the view should be square
	r_refdef.fov_x = r_refdef.fov_y = 90;
	vid.width = vid.height =
		r_refdef.vrect.height = r_refdef.vrect.width =
		glwidth = glheight = gl_cube_map_size;
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
	glwidth = s_gl_w;
	glheight = s_gl_h;
	R_SetupGL_Viewport_and_Perspective ();
	qfglMatrixMode (GL_MODELVIEW);
	qfglCallList (fisheye_grid);
}
