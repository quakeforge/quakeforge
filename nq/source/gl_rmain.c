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

	$Id$
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

#include "QF/compat.h"
#include "QF/console.h"
#include "QF/model.h"
#include "QF/qargs.h"
#include "QF/sys.h"

#include "client.h"
#include "chase.h"
#include "game.h"
#include "glquake.h"
#include "render.h"
#include "r_local.h"
#include "view.h"

entity_t    r_worldentity;

qboolean    r_cache_thrash;				// compatability

vec3_t      modelorg, r_entorigin;
entity_t   *currententity;

int         currenttexture = -1;		// to avoid unnecessary texture sets
int         cnttextures[2] = { -1, -1 };	// cached


int         r_visframecount;			// bumped when going to a new PVS
int         r_framecount;				// used for dlight push checking

int         c_brush_polys, c_alias_polys;

qboolean    envmap;						// true during envmap command capture


int         particletexture;			// little dot for particles
int         playertextures;				// up to 16 color translated skins

int         mirrortexturenum;			// quake texturenum, not gltexturenum
qboolean    mirror;
mplane_t   *mirror_plane;

//
// view origin
//
vec3_t      vup;
vec3_t      vpn;
vec3_t      vright;
vec3_t      r_origin;

float       r_world_matrix[16];
float       r_base_world_matrix[16];

//
// screen size info
//
refdef_t    r_refdef;

mleaf_t    *r_viewleaf, *r_oldviewleaf;

int         d_lightstylevalue[256];		// 8.8 fraction of base light value


void        R_MarkLeaves (void);

extern cvar_t *scr_fov;

extern byte gammatable[256];
extern qboolean lighthalf;
static float vid_gamma = 1.0;

// LordHavoc: place for gl_rmain setup code
void
glrmain_init ()
{
};

/*
	GL_CheckGamma

	More or less redesigned by LordHavoc
*/
void
GL_CheckGamma (unsigned char *pal)
{
	float       inf;
	int         i;

	if ((i = COM_CheckParm ("-gamma")) == 0) {
		if ((gl_renderer && strstr (gl_renderer, "Voodoo")) ||
			(gl_vendor && strstr (gl_vendor, "3Dfx")))
			vid_gamma = 1;
		else
			vid_gamma = 0.7;			// default to 0.7 on non-3dfx
		// hardware
	} else
		vid_gamma = atof (com_argv[i + 1]);

	// build the gamma table
	if (vid_gamma == 1) {
		// screw the math
		for (i = 0; i < 256; i++)
			gammatable[i] = i;
	} else {
		for (i = 0; i < 256; i++) {
			inf = pow ((i + 1) / 256.0, vid_gamma) * 255 + 0.5;
			inf = bound (0, inf, 255);
			gammatable[i] = inf;
		}
	}

	// correct the palette
	for (i = 0; i < 768; i++)
		pal[i] = gammatable[pal[i]];
}


void
R_RotateForEntity (entity_t *e)
{
	glTranslatef (e->origin[0], e->origin[1], e->origin[2]);

	glRotatef (e->angles[1], 0, 0, 1);
	glRotatef (-e->angles[0], 0, 1, 0);
	// ZOID: fixed z angle
	glRotatef (e->angles[2], 1, 0, 0);
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/


static mspriteframe_t *
R_GetSpriteFrame (entity_t *currententity)
{
	msprite_t  *psprite;
	mspritegroup_t *pspritegroup;
	mspriteframe_t *pspriteframe;
	int         i, numframes, frame;
	float      *pintervals, fullinterval, targettime, time;

	psprite = currententity->model->cache.data;
	frame = currententity->frame;

	if ((frame >= psprite->numframes) || (frame < 0)) {
		Con_Printf ("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE) {
		pspriteframe = psprite->frames[frame].frameptr;
	} else {
		pspritegroup = (mspritegroup_t *) psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes - 1];

		time = cl.time + currententity->syncbase;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval
		// values
		// are positive, so we don't have to worry about division by 0
		targettime = time - ((int) (time / fullinterval)) * fullinterval;

		for (i = 0; i < (numframes - 1); i++) {
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}


static void
R_DrawSpriteModel (entity_t *e)
{
	vec3_t      point;
	mspriteframe_t *frame;
	float      *up, *right;
	vec3_t      v_forward, v_right, v_up;
	msprite_t  *psprite;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	frame = R_GetSpriteFrame (e);
	psprite = currententity->model->cache.data;

	if (psprite->type == SPR_ORIENTED) {	// bullet marks on walls
		AngleVectors (currententity->angles, v_forward, v_right, v_up);
		up = v_up;
		right = v_right;
	} else {							// normal sprite
		up = vup;
		right = vright;
	}

	if (lighthalf)
		glColor4f (0.5, 0.5, 0.5, 1);
	else
		glColor4f (1, 1, 1, 1);

	glBindTexture (GL_TEXTURE_2D, frame->gl_texturenum);

	glEnable (GL_ALPHA_TEST);
	glBegin (GL_QUADS);

	glTexCoord2f (0, 1);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->left, right, point);
	glVertex3fv (point);

	glTexCoord2f (0, 0);
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->left, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 0);
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->right, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 1);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->right, right, point);
	glVertex3fv (point);

	glEnd ();

	glDisable (GL_ALPHA_TEST);
}


/*
=============================================================

  ALIAS MODELS

=============================================================
*/

#define NUMVERTEXNORMALS	162

float       r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

vec3_t      shadevector;
float       shadelight;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float       r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "anorm_dots.h"
           ;

float      *shadedots = r_avertexnormal_dots[0];

int         lastposenum;


static void
GL_DrawAliasFrame (aliashdr_t *paliashdr, int posenum, qboolean fb)
{
	float       l;
	trivertx_t *verts;
	int        *order;
	int         count;

	lastposenum = posenum;

	verts = (trivertx_t *) ((byte *) paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *) ((byte *) paliashdr + paliashdr->commands);

	if (fb)
		glColor3f (1, 1, 1);
	else if (lighthalf)
		shadelight *= 2;
	while (1) {
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;						// done
		if (count < 0) {
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		} else
			glBegin (GL_TRIANGLE_STRIP);

		do {
			// texture coordinates come from the draw list
			glTexCoord2f (((float *) order)[0], ((float *) order)[1]);
			order += 2;

			if (!fb) {
				// normals and vertexes come from the frame list
				l = shadedots[verts->lightnormalindex] * shadelight;
				glColor3f (l, l, l);
			}

			glVertex3f (verts->v[0], verts->v[1], verts->v[2]);
			verts++;
		} while (--count);

		glEnd ();
	}
}



extern vec3_t lightspot;

static void
GL_DrawAliasShadow (aliashdr_t *paliashdr, int posenum)
{
	trivertx_t *verts;
	int        *order;
	vec3_t      point;
	float       height, lheight;
	int         count;

	lheight = currententity->origin[2] - lightspot[2];

	height = 0;
	verts = (trivertx_t *) ((byte *) paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *) ((byte *) paliashdr + paliashdr->commands);

	height = -lheight + 1.0;

	while (1) {
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;						// done
		if (count < 0) {
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		} else
			glBegin (GL_TRIANGLE_STRIP);

		do {
			// texture coordinates come from the draw list
			// (skipped for shadows) glTexCoord2fv ((float *)order);
			order += 2;

			// normals and vertexes come from the frame list
			point[0] =
				verts->v[0] * paliashdr->mdl.scale[0] +
				paliashdr->mdl.scale_origin[0];
			point[1] =
				verts->v[1] * paliashdr->mdl.scale[1] +
				paliashdr->mdl.scale_origin[1];
			point[2] =
				verts->v[2] * paliashdr->mdl.scale[2] +
				paliashdr->mdl.scale_origin[2];

			point[0] -= shadevector[0] * (point[2] + lheight);
			point[1] -= shadevector[1] * (point[2] + lheight);
			point[2] = height;
//          height -= 0.001;
			glVertex3fv (point);

			verts++;
		} while (--count);

		glEnd ();
	}
}


static void
R_SetupAliasFrame (int frame, aliashdr_t *paliashdr, qboolean fb)
{
	int         pose, numposes;
	float       interval;

	if ((frame >= paliashdr->mdl.numframes) || (frame < 0)) {
		Con_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1) {
		interval = paliashdr->frames[frame].interval;
		pose += (int) (cl.time / interval) % numposes;
	}

	GL_DrawAliasFrame (paliashdr, pose, fb);
}


static void
R_DrawAliasModel (entity_t *e)
{
	int         i;
	int         lnum;
	vec3_t      dist;
	float       add;
	model_t    *clmodel;
	vec3_t      mins, maxs;
	aliashdr_t *paliashdr;
	float       an;
	int         anim;

	clmodel = currententity->model;

	VectorAdd (currententity->origin, clmodel->mins, mins);
	VectorAdd (currententity->origin, clmodel->maxs, maxs);

	if (R_CullBox (mins, maxs))
		return;

	VectorCopy (currententity->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	// 
	// get lighting information
	// 

	shadelight = R_LightPoint (currententity->origin);

	// always give the gun some light
	if (e == &cl.viewent && shadelight < 24)
		shadelight = 24;

	for (lnum = 0; lnum < MAX_DLIGHTS; lnum++) {
		if (cl_dlights[lnum].die >= cl.time) {
			VectorSubtract (currententity->origin, cl_dlights[lnum].origin,
							dist);
			add =
				(cl_dlights[lnum].radius * cl_dlights[lnum].radius * 8) /
				(DotProduct (dist, dist));

			if (add > 0)
				shadelight += add;
		}
	}

	// clamp lighting
	if (shadelight > 200)
		shadelight = 200;

	// ZOID: never allow players to go totally black
	if (!strcmp (clmodel->name, "progs/player.mdl")) {
		if (shadelight < 8)
			shadelight = 8;
	}
		else if (!gl_fb_models->int_val
				 && (!strcmp (clmodel->name, "progs/flame.mdl")
					 || !strcmp (clmodel->name, "progs/flame2.mdl"))) {
		// HACK HACK HACK -- no fullbright colors, so make torches full light
		shadelight = 200;
	}

	shadedots =
		r_avertexnormal_dots[((int) (e->angles[1] * (SHADEDOT_QUANT / 360.0))) &
							 (SHADEDOT_QUANT - 1)];
	shadelight = shadelight / 200.0;

	an = e->angles[1] / 180 * M_PI;
	shadevector[0] = cos (-an);
	shadevector[1] = sin (-an);
	shadevector[2] = 1;
	VectorNormalize (shadevector);

	// 
	// locate the proper data
	// 
	paliashdr = (aliashdr_t *) Mod_Extradata (currententity->model);

	c_alias_polys += paliashdr->mdl.numtris;

	// 
	// draw all the triangles
	// 

	glPushMatrix ();
	R_RotateForEntity (e);

	// LordHavoc: must be in modulate mode for reasons of lighting as well as 
	// 
	// 
	// fullbright support
	glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (!strcmp (clmodel->name, "progs/eyes.mdl")) {
		glTranslatef (paliashdr->mdl.scale_origin[0],
					  paliashdr->mdl.scale_origin[1],
					  paliashdr->mdl.scale_origin[2] - (22 + 8));
		// double size of eyes, since they are really hard to see in gl
		glScalef (paliashdr->mdl.scale[0] * 2, paliashdr->mdl.scale[1] * 2,
				  paliashdr->mdl.scale[2] * 2);
	} else {
		glTranslatef (paliashdr->mdl.scale_origin[0],
					  paliashdr->mdl.scale_origin[1],
					  paliashdr->mdl.scale_origin[2]);
		glScalef (paliashdr->mdl.scale[0], paliashdr->mdl.scale[1],
				  paliashdr->mdl.scale[2]);
	}

	anim = (int) (cl.time * 10) & 3;
	glBindTexture (GL_TEXTURE_2D,
				   paliashdr->gl_texturenum[currententity->skinnum][anim]);

	// we can't dynamically colormap textures, so they are cached
	// seperately for the players.  Heads are just uncolored.
	if (currententity->colormap != vid.colormap && !gl_nocolors->int_val) {
		i = currententity - cl_entities;
		if (i >= 1 && i <= cl.maxclients	/* && !strcmp
											   (currententity->model->name,
											   "progs/player.mdl") */ )
			glBindTexture (GL_TEXTURE_2D, playertextures - 1 + i);
	}

	if (gl_smoothmodels->int_val)
		glShadeModel (GL_SMOOTH);

	if (gl_affinemodels->int_val)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	R_SetupAliasFrame (currententity->frame, paliashdr, false);

	// This block is GL fullbright support for objects...
	if (clmodel->hasfullbrights && gl_fb_models->int_val
		&& paliashdr->gl_fb_texturenum[currententity->skinnum][anim]) {
		glBlendFunc (GL_ONE, GL_ONE);
		glEnable (GL_BLEND);

		glBindTexture (GL_TEXTURE_2D,
					   paliashdr->gl_fb_texturenum[currententity->
												   skinnum][anim]);
		R_SetupAliasFrame (currententity->frame, paliashdr, true);

		glDisable (GL_BLEND);
		glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	glShadeModel (GL_FLAT);
	if (gl_affinemodels->int_val)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glPopMatrix ();

	if (r_shadows->int_val) {
		glPushMatrix ();
		R_RotateForEntity (e);
		glDisable (GL_TEXTURE_2D);
		glColor4f (0, 0, 0, 0.5);
		GL_DrawAliasShadow (paliashdr, lastposenum);
		glEnable (GL_TEXTURE_2D);
		glColor4f (0.5, 0.5, 0.5, 1);
		glPopMatrix ();
	}

}

//==================================================================================


static void
R_DrawEntitiesOnList (void)
{
	int         i;

	if (!r_drawentities->int_val)
		return;

	// LordHavoc: split into 3 loops to simplify state changes
	for (i = 0; i < cl_numvisedicts; i++) {
		if (cl_visedicts[i]->model->type != mod_brush)
			continue;
		currententity = cl_visedicts[i];

		R_DrawBrushModel (currententity);
	}

	for (i = 0; i < cl_numvisedicts; i++) {
		if (cl_visedicts[i]->model->type != mod_alias)
			continue;
		currententity = cl_visedicts[i];

		if (currententity == &cl_entities[cl.viewentity])
			currententity->angles[PITCH] *= 0.3;

		R_DrawAliasModel (currententity);
	}

	for (i = 0; i < cl_numvisedicts; i++) {
		if (cl_visedicts[i]->model->type != mod_sprite)
			continue;
		currententity = cl_visedicts[i];

		R_DrawSpriteModel (currententity);
	}
}


static void
R_DrawViewModel (void)
{
	currententity = &cl.viewent;
	if (!r_drawviewmodel->int_val
		|| chase_active->int_val
		|| envmap || !r_drawentities->int_val || (cl.items & IT_INVISIBILITY)
		|| cl.stats[STAT_HEALTH] <= 0 || !currententity->model)
		return;

	// hack the depth range to prevent view model from poking into walls
	glDepthRange (gldepthmin, gldepthmin + 0.3 * (gldepthmax - gldepthmin));
	R_DrawAliasModel (currententity);
	glDepthRange (gldepthmin, gldepthmax);
}


static int
SignbitsForPlane (mplane_t *out)
{
	int         bits, j;

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

	if (r_refdef.fov_x == 90) {
		// front side is visible

		VectorAdd (vpn, vright, frustum[0].normal);
		VectorSubtract (vpn, vright, frustum[1].normal);

		VectorAdd (vpn, vup, frustum[2].normal);
		VectorSubtract (vpn, vup, frustum[3].normal);
	} else {

		// rotate VPN right by FOV_X/2 degrees
		RotatePointAroundVector (frustum[0].normal, vup, vpn,
								 -(90 - r_refdef.fov_x / 2));
		// rotate VPN left by FOV_X/2 degrees
		RotatePointAroundVector (frustum[1].normal, vup, vpn,
								 90 - r_refdef.fov_x / 2);
		// rotate VPN up by FOV_X/2 degrees
		RotatePointAroundVector (frustum[2].normal, vright, vpn,
								 90 - r_refdef.fov_y / 2);
		// rotate VPN down by FOV_X/2 degrees
		RotatePointAroundVector (frustum[3].normal, vright, vpn,
								 -(90 - r_refdef.fov_y / 2));
	}

	for (i = 0; i < 4; i++) {
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}


void
R_SetupFrame (void)
{
// don't allow cheats in multiplayer
	Cvar_SetValue (r_fullbright, 0);
	Cvar_SetValue (r_lightmap, 0);

	R_AnimateLight ();

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);

	V_SetContentsColor (r_viewleaf->contents);
	V_CalcBlend ();

	r_cache_thrash = false;

	c_brush_polys = 0;
	c_alias_polys = 0;

}


static void
MYgluPerspective (GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar)
{
	GLdouble    xmin, xmax, ymin, ymax;

	ymax = zNear * tan (fovy * M_PI / 360.0);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	glFrustum (xmin, xmax, ymin, ymax, zNear, zFar);
}


static void
R_SetupGL (void)
{
	float       screenaspect;
	extern int  glwidth, glheight;
	int         x, x2, y2, y, w, h;

	// 
	// set up viewpoint
	// 
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();
	x = r_refdef.vrect.x * glwidth / vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth / vid.width;
	y = (vid.height - r_refdef.vrect.y) * glheight / vid.height;
	y2 =
		(vid.height -
		 (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight / vid.height;

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

	if (envmap) {
		x = y2 = 0;
		w = h = 256;
	}

	glViewport (glx + x, gly + y2, w, h);
	screenaspect = (float) r_refdef.vrect.width / r_refdef.vrect.height;
//  yfov = 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*180/M_PI;
//  yfov = (2.0 * tan (scr_fov->value/360*M_PI)) / screenaspect;
//  yfov = 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*(scr_fov->value*2)/M_PI;
//  MYgluPerspective (yfov,  screenaspect,  4,  4096);
	MYgluPerspective (r_refdef.fov_y, screenaspect, 4, 4096);

	if (mirror) {
		if (mirror_plane->normal[2])
			glScalef (1, -1, 1);
		else
			glScalef (-1, 1, 1);
		glCullFace (GL_BACK);
	} else
		glCullFace (GL_FRONT);

	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();

	glRotatef (-90, 1, 0, 0);			// put Z going up
	glRotatef (90, 0, 0, 1);			// put Z going up
	glRotatef (-r_refdef.viewangles[2], 1, 0, 0);
	glRotatef (-r_refdef.viewangles[0], 0, 1, 0);
	glRotatef (-r_refdef.viewangles[1], 0, 0, 1);
	glTranslatef (-r_refdef.vieworg[0], -r_refdef.vieworg[1],
				  -r_refdef.vieworg[2]);

	glGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);

	// 
	// set drawing parms
	// 
	if (gl_cull->int_val)
		glEnable (GL_CULL_FACE);
	else
		glDisable (GL_CULL_FACE);

	glEnable (GL_BLEND);
	glDisable (GL_ALPHA_TEST);
	glAlphaFunc (GL_GREATER, 0.5);
	glEnable (GL_DEPTH_TEST);
	glShadeModel (GL_SMOOTH);
}


/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
static void
R_RenderScene (void)
{
	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_MarkLeaves ();					// done here so we know if we're in
	// water

	R_DrawWorld ();						// adds static entities to the list

	S_ExtraUpdate ();					// don't let sound get messed up if
	// going slow

	R_DrawEntitiesOnList ();

	R_RenderDlights ();
	R_UpdateFires ();
	R_DrawParticles ();
}


static void
R_Clear (void)
{
	if (gl_clear->int_val)
		glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	else
		glClear (GL_DEPTH_BUFFER_BIT);
	gldepthmin = 0;
	gldepthmax = 1;
	glDepthFunc (GL_LEQUAL);

	glDepthRange (gldepthmin, gldepthmax);
}

#if 0 // !!! FIXME, Zoid, mirror is disabled for now
void
R_Mirror (void)
{
	float       d;
	msurface_t *s;
	entity_t   *ent;

	if (!mirror)
		return;

	memcpy (r_base_world_matrix, r_world_matrix, sizeof (r_base_world_matrix));

	d =
		DotProduct (r_refdef.vieworg,
					mirror_plane->normal) - mirror_plane->dist;
	VectorMA (r_refdef.vieworg, -2 * d, mirror_plane->normal, r_refdef.vieworg);

	d = DotProduct (vpn, mirror_plane->normal);
	VectorMA (vpn, -2 * d, mirror_plane->normal, vpn);

	r_refdef.viewangles[0] = -asin (vpn[2]) / M_PI * 180;
	r_refdef.viewangles[1] = atan2 (vpn[1], vpn[0]) / M_PI * 180;
	r_refdef.viewangles[2] = -r_refdef.viewangles[2];

	ent = &cl_entities[cl.viewentity];
	if (cl_numvisedicts < MAX_VISEDICTS) {
		cl_visedicts[cl_numvisedicts] = ent;
		cl_numvisedicts++;
	}

	gldepthmin = 0.5;
	gldepthmax = 1;
	glDepthRange (gldepthmin, gldepthmax);
	glDepthFunc (GL_LEQUAL);

	R_RenderScene ();
	R_DrawWaterSurfaces ();


	gldepthmin = 0;
	gldepthmax = 0.5;
	glDepthRange (gldepthmin, gldepthmax);
	glDepthFunc (GL_LEQUAL);

	// blend on top
	glMatrixMode (GL_PROJECTION);
	if (mirror_plane->normal[2])
		glScalef (1, -1, 1);
	else
		glScalef (-1, 1, 1);
	glCullFace (GL_FRONT);
	glMatrixMode (GL_MODELVIEW);

	glLoadMatrixf (r_base_world_matrix);

	glColor4f (1, 1, 1, r_mirroralpha->value);
	s = cl.worldmodel->textures[mirrortexturenum]->texturechain;
	for (; s; s = s->texturechain)
		R_RenderBrushPoly (s);
	cl.worldmodel->textures[mirrortexturenum]->texturechain = NULL;
	glColor4f (1, 1, 1, 1);
}
#endif


/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void
R_RenderView (void)
{
	if (r_norefresh->int_val)
		return;

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

//  glFinish ();

	mirror = false;

	R_Clear ();

	// render normal view
	R_RenderScene ();
	R_DrawViewModel ();
	R_DrawWaterSurfaces ();

	// render mirror view
//  R_Mirror ();
}
