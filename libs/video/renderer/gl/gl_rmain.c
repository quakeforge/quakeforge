/*
	gl_rmain.c

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

static __attribute__ ((unused)) const char rcsid[] = 
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

#include "QF/console.h"
#include "QF/cvar.h"
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
#include "r_cvar.h"
#include "r_dynamic.h"
#include "r_local.h"
#include "view.h"

entity_t    r_worldentity;

qboolean    r_cache_thrash;				// compatability

vec3_t      modelorg, r_entorigin;
entity_t   *currententity;

int         r_visframecount;			// bumped when going to a new PVS
int         r_framecount;				// used for dlight push checking

int         c_brush_polys, c_alias_polys;

qboolean    envmap;						// true during envmap command capture

int         mirrortexturenum;			// quake texturenum, not gltexturenum
qboolean    mirror;
mplane_t   *mirror_plane;

// view origin
vec3_t      vup;
vec3_t      vpn;
vec3_t      vright;
vec3_t      r_origin;

float       r_world_matrix[16];
float       r_base_world_matrix[16];

// screen size info
refdef_t    r_refdef;

mleaf_t    *r_viewleaf, *r_oldviewleaf;

int         d_lightstylevalue[256];		// 8.8 fraction of base light value

vec3_t		shadecolor;					// Ender (Extend) Colormod
float		modelalpha;					// Ender (Extend) Alpha


void
glrmain_init (void)
{
	gldepthmin = 0;
	gldepthmax = 1;
	qfglDepthFunc (GL_LEQUAL);
	qfglDepthRange (gldepthmin, gldepthmax);
}

inline void
R_RotateForEntity (entity_t *e)
{
	qfglTranslatef (e->origin[0], e->origin[1], e->origin[2]);

	qfglRotatef (e->angles[1], 0, 0, 1);
	qfglRotatef (-e->angles[0], 0, 1, 0);
	// ZOID: fixed z angle
	qfglRotatef (e->angles[2], 1, 0, 0);
}

/*
	R_ShowNearestLoc

	Display the nearest symbolic location (.loc files)
*/
static void
R_ShowNearestLoc (void)
{
	dlight_t   *dl;
	location_t *nearloc;
	vec3_t		trueloc;

	if (r_drawentities->int_val)
		return;

	nearloc = locs_find (r_origin);

	if (nearloc) {
		dl = R_AllocDlight (4096);
		if (dl) {
			VectorCopy (nearloc->loc, dl->origin);
			dl->radius = 200;
			dl->die = r_realtime + 0.1;
			dl->color[0] = 0;
			dl->color[1] = 1;
			dl->color[2] = 0;
		}
		VectorCopy (nearloc->loc, trueloc);
		(*R_WizSpikeEffect) (trueloc);
	}
}

/*
	R_DrawEntitiesOnList

	Draw all the entities we have information on.
*/
static void
R_DrawEntitiesOnList (void)
{
	int		i;

	if (!r_drawentities->int_val) {
		R_ShowNearestLoc();
		return;
	}

	// LordHavoc: split into 3 loops to simplify state changes
	for (i = 0; i < r_numvisedicts; i++) {
		if (r_visedicts[i]->model->type != mod_brush)
			continue;
		currententity = r_visedicts[i];

		R_DrawBrushModel (currententity);
	}

	if (gl_affinemodels->int_val)
		qfglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	for (i = 0; i < r_numvisedicts; i++) {
		if (r_visedicts[i]->model->type != mod_alias)
			continue;
		currententity = r_visedicts[i];

		if (currententity == r_player_entity)
			currententity->angles[PITCH] *= 0.3;

		R_DrawAliasModel (currententity);
	}
	if (gl_affinemodels->int_val)
		qfglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_DONT_CARE);

	qfglColor3ubv (color_white);
	qfglEnable (GL_ALPHA_TEST);
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
	if (gl_affinemodels->int_val)
		qfglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	R_DrawAliasModel (currententity);
	if (gl_affinemodels->int_val)
		qfglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_DONT_CARE);
	qfglDepthRange (gldepthmin, gldepthmax);
	qfglColor3ubv (color_white);
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

	qfglFrustum (xmin, xmax, ymin, ymax, zNear, zFar);
}

static void
R_SetupGL (void)
{
	float		screenaspect;
	int			x, x2, y2, y, w, h;

	R_SetFrustum ();

	// set up viewpoint
	qfglMatrixMode (GL_PROJECTION);
	qfglLoadIdentity ();

	if (envmap) {
		x = y2 = 0;
		w = h = 256;
	} else {
		x = r_refdef.vrect.x * glwidth / vid.width;
		x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth / vid.width;
		y = (vid.height - r_refdef.vrect.y) * glheight / vid.height;
		y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) *
			glheight / vid.height;

		// fudge around because of frac screen scale
		if (x > 0)
			x--;
		if (x2 < glwidth)
			x2++;
		if (y2 < 0)
			y2--;
		if (y < glheight)
			y++;

		w = x2 - x;
		h = y - y2;
	}

	qfglViewport (glx + x, gly + y2, w, h);
	screenaspect = (float) r_refdef.vrect.width / r_refdef.vrect.height;
	MYgluPerspective (r_refdef.fov_y, screenaspect, r_nearclip->value,
					  r_farclip->value);

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
	qfglEnable (GL_CULL_FACE);
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
}

static void
R_Mirror (void)
{
	float		d;
	entity_t  **ent;
	msurface_t *s;

	if (!mirror)
		return;

	memcpy (r_base_world_matrix, r_world_matrix, sizeof (r_base_world_matrix));

	d = DotProduct (r_refdef.vieworg, mirror_plane->normal) -
		mirror_plane->dist;
	VectorMA (r_refdef.vieworg, -2 * d, mirror_plane->normal,
			  r_refdef.vieworg);

	d = DotProduct (vpn, mirror_plane->normal);
	VectorMA (vpn, -2 * d, mirror_plane->normal, vpn);

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

	color_white[2] = r_mirroralpha->value * 255;
	qfglColor4ubv (color_white);
	s = r_worldentity.model->textures[mirrortexturenum]->texturechain;
	for (; s; s = s->texturechain) {
		texture_t  *tex;
		if (!s->texinfo->texture->anim_total)
			tex = s->texinfo->texture;
		else
			tex = R_TextureAnimation (s);

// FIXME: if this is needed, then include header for fullbright_polys
//		if ( tex->gl_fb_texturenum > 0) {
//			s->polys->fb_chain = fullbright_polys[tex->gl_fb_texturenum];
//			fullbright_polys[tex->gl_fb_texturenum] = s->polys;
//		}

		qfglBindTexture (GL_TEXTURE_2D, tex->gl_texturenum);
		R_RenderBrushPoly (s);
	}
	r_worldentity.model->textures[mirrortexturenum]->texturechain = NULL;
	qfglColor3ubv (color_white);
}

/*
	R_RenderView

	r_refdef must be set before the first call
*/
void
R_RenderView (void)
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
