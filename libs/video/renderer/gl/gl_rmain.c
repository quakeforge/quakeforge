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

#include "compat.h"
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
#include "QF/GL/qf_screen.h"
#include "QF/GL/qf_vid.h"
#include "QF/GL/qf_rsurf.h"
#include "QF/GL/qf_rlight.h"
#include "QF/GL/qf_fires.h"

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

void        R_MarkLeaves (void);

//qboolean R_CullBlocked (vec3_t mins, vec3_t maxs, vec3_t org);

// LordHavoc: place for gl_rmain setup code
void
glrmain_init (void)
{
}


void
R_RotateForEntity (entity_t *e)
{
	QFGL_glTranslatef (e->origin[0], e->origin[1], e->origin[2]);

	QFGL_glRotatef (e->angles[1], 0, 0, 1);
	QFGL_glRotatef (-e->angles[0], 0, 1, 0);
	// ZOID: fixed z angle
	QFGL_glRotatef (e->angles[2], 1, 0, 0);
}


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

		time = r_realtime + currententity->syncbase;

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

	QFGL_glBindTexture (GL_TEXTURE_2D, frame->gl_texturenum);

	QFGL_glEnable (GL_ALPHA_TEST);
	QFGL_glBegin (GL_QUADS);

	QFGL_glTexCoord2f (0, 1);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->left, right, point);
	QFGL_glVertex3fv (point);

	QFGL_glTexCoord2f (0, 0);
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->left, right, point);
	QFGL_glVertex3fv (point);

	QFGL_glTexCoord2f (1, 0);
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->right, right, point);
	QFGL_glVertex3fv (point);

	QFGL_glTexCoord2f (1, 1);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->right, right, point);
	QFGL_glVertex3fv (point);

	QFGL_glEnd ();

	QFGL_glDisable (GL_ALPHA_TEST);
}


/*
	ALIAS MODELS
*/

#define NUMVERTEXNORMALS	162

float       r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

vec3_t      shadevector;
float       shadelight;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float   r_avertexnormal_dots[SHADEDOT_QUANT][256] =
	#include "anorm_dots.h"
		;

float      *shadedots = r_avertexnormal_dots[0];

int         lastposenum, lastposenum0;


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

	if (modelalpha != 1.0)
		QFGL_glDepthMask (GL_FALSE);

	if (fb) {
		QFGL_glColor4f (1, 1, 1, modelalpha);
	}

	while ((count = *order++)) {
		// get the vertex count and primitive type
		if (count < 0) {
			count = -count;
			QFGL_glBegin (GL_TRIANGLE_FAN);
		} else {
			QFGL_glBegin (GL_TRIANGLE_STRIP);
		}

		do {
			// texture coordinates come from the draw list
			QFGL_glTexCoord2f (((float *) order)[0], ((float *) order)[1]);
			order += 2;

			if (!fb) {
				// normals and vertexes come from the frame list
				l = shadedots[verts->lightnormalindex] * shadelight;

				// LordHavoc: cleanup after Endy
				QFGL_glColor4f (shadecolor[0] * l, shadecolor[1] * l,
						   shadecolor[2] * l, modelalpha);
			}

			QFGL_glVertex3f (verts->v[0], verts->v[1], verts->v[2]);
			verts++;
		} while (--count);

		QFGL_glEnd ();
	}

	if (modelalpha != 1.0)
		QFGL_glDepthMask (GL_TRUE);

	QFGL_glColor3ubv (lighthalf_v);
}


/*
	GL_DrawAliasBlendedFrame

	Interpolated model drawing
*/
void
GL_DrawAliasBlendedFrame (aliashdr_t *paliashdr, int pose1, int pose2, float blend, qboolean fb)
{
	float		light;
	float 		lerp;
	trivertx_t	*verts1;
	trivertx_t	*verts2;
	int 		*order;
	int 		count;

	lastposenum0 = pose1;
	lastposenum = pose2;

	verts1 = (trivertx_t *) ((byte *) paliashdr + paliashdr->posedata);
	verts2 = verts1;

	verts1 += pose1 * paliashdr->poseverts;
	verts2 += pose2 * paliashdr->poseverts;

	order = (int *) ((byte *) paliashdr + paliashdr->commands);

	if (modelalpha != 1.0)
		QFGL_glDepthMask (GL_FALSE);

	if (fb) {	// don't do this in the loop, it doesn't change
		QFGL_glColor4f (1, 1, 1, modelalpha);
	}

	lerp = 1 - blend;
	while ((count = *order++)) {	// get the vertex count and primitive type

		if (count < 0) {
			count = -count;
			QFGL_glBegin (GL_TRIANGLE_FAN);
		} else {
			QFGL_glBegin (GL_TRIANGLE_STRIP);
		}

		do {
			// texture coordinates come from the draw list
			QFGL_glTexCoord2f (((float *) order)[0], ((float *) order)[1]);
			order += 2;

			if (!fb) {
				// normals and vertexes come from the frame list
				// blend the light intensity from the two frames together
				light = shadelight * ((shadedots[verts1->lightnormalindex] *
									   lerp)
									+ (shadedots[verts2->lightnormalindex] *
									   blend));
				QFGL_glColor4f (shadecolor[0] * light, shadecolor[1] * light,
							shadecolor[2] * light, modelalpha);
			}

			// blend the vertex positions from each frame together
			QFGL_glVertex3f ((verts1->v[0] * lerp) + (verts2->v[0] * blend),
						(verts1->v[1] * lerp) + (verts2->v[1] * blend),
						(verts1->v[2] * lerp) + (verts2->v[2] * blend));

			verts1++;
			verts2++;
		} while (--count);
		QFGL_glEnd ();
	}

	if (modelalpha != 1.0)
		QFGL_glDepthMask (GL_TRUE);
	QFGL_glColor3ubv (lighthalf_v);
}


extern vec3_t lightspot;

/*
	GL_DrawAliasShadow

	Standard shadow drawing
*/
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

	while ((count = *order++)) {
		// get the vertex count and primitive type

		if (count < 0) {
			count = -count;
			QFGL_glBegin (GL_TRIANGLE_FAN);
		} else
			QFGL_glBegin (GL_TRIANGLE_STRIP);

		do {
			// texture coordinates come from the draw list
			// (skipped for shadows) QFGL_glTexCoord2fv ((float *)order);
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
			QFGL_glVertex3fv (point);

			verts++;
		} while (--count);

		QFGL_glEnd ();
	}
}


/*
	GL_DrawAliasBlendedShadow
         
	Interpolated shadow drawing
*/
void
GL_DrawAliasBlendedShadow (aliashdr_t *paliashdr, int pose1, int pose2, entity_t *e)
{
	trivertx_t	*verts1, *verts2;
	float 		lerp;
	vec3_t		point1, point2;
	int 		*order, count;
	float       height, lheight, blend;

	blend = (r_realtime - e->frame_start_time) / e->frame_interval;
	blend = min (blend, 1);
	lerp = 1 - blend;

	lheight = e->origin[2] - lightspot[2];
	height = -lheight + 1.0;

	verts2 = verts1 = (trivertx_t *) ((byte *) paliashdr + paliashdr->posedata);

	verts1 += pose1 * paliashdr->poseverts;
	verts2 += pose2 * paliashdr->poseverts;

	order = (int *) ((byte *) paliashdr + paliashdr->commands);

	while ((count = *order++)) {
		// get the vertex count and primitive type

		if (count < 0) {
			count = -count;
			QFGL_glBegin (GL_TRIANGLE_FAN);
		} else {
			QFGL_glBegin (GL_TRIANGLE_STRIP);
		}

		do {
			order += 2;

			point1[0] =	verts1->v[0] * paliashdr->mdl.scale[0] + paliashdr->mdl.scale_origin[0];
			point1[1] =	verts1->v[1] * paliashdr->mdl.scale[1] + paliashdr->mdl.scale_origin[1];
			point1[2] =	verts1->v[2] * paliashdr->mdl.scale[2] + paliashdr->mdl.scale_origin[2];

			point1[0] -= shadevector[0] * (point1[2] + lheight);
			point1[1] -= shadevector[1] * (point1[2] + lheight);

			point2[0] =	verts2->v[0] * paliashdr->mdl.scale[0] + paliashdr->mdl.scale_origin[0];
			point2[1] =	verts2->v[1] * paliashdr->mdl.scale[1] + paliashdr->mdl.scale_origin[1];
			point2[2] =	verts2->v[2] * paliashdr->mdl.scale[2] + paliashdr->mdl.scale_origin[2];

			point2[0] -= shadevector[0] * (point2[2] + lheight);
			point2[1] -= shadevector[1] * (point2[2] + lheight);

			QFGL_glVertex3f ((point1[0] * lerp) + (point2[0] * blend),
						(point1[1] * lerp) + (point2[1] * blend),
						height);

			verts1++;
			verts2++;
		} while (--count);
		QFGL_glEnd ();
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
		pose += (int) (r_realtime / interval) % numposes;
	}

	GL_DrawAliasFrame (paliashdr, pose, fb);
}


void
R_SetupAliasBlendedFrame (int frame, aliashdr_t *paliashdr, entity_t *e, qboolean fb)
{
	int 	pose, numposes;
	float	blend;

	if ((frame >= paliashdr->mdl.numframes) || (frame < 0)) {
		Con_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1) {
		e->frame_interval = paliashdr->frames[frame].interval;
		pose += (int) (r_realtime / e->frame_interval) % numposes;
	} else {
		/*
			One tenth of a second is good for most Quake animations. If the
			nextthink is longer then the animation is usually meant to pause
			(e.g. check out the shambler magic animation in shambler.qc).  If
			its shorter then things will still be smoothed partly, and the
			jumps will be less noticable because of the shorter time.  So,
			this is probably a good assumption.
		*/
		e->frame_interval = 0.1;
	}

	if (e->pose2 != pose) {
		e->frame_start_time = r_realtime;
		if (e->pose2 == -1) {
			e->pose1 = pose;
		} else {
			e->pose1 = e->pose2;
		}
		e->pose2 = pose;
		blend = 0;
	} else {
		blend = (r_realtime - e->frame_start_time) / e->frame_interval;
	}

	// wierd things start happening if blend passes 1
	if (r_paused || blend > 1)
		blend = 1;

	GL_DrawAliasBlendedFrame (paliashdr, e->pose1, e->pose2, blend, fb);
}


static void
R_DrawAliasModel (entity_t *e, qboolean cull)
{
	int         lnum;
	vec3_t      dist;
	float       add;
	model_t    *clmodel;
	vec3_t      mins, maxs;
	aliashdr_t *paliashdr;
	float       an;
	int         anim;
	int         texture;
	int         fb_texture = 0;
	int         skinnum;
	qboolean	modelIsFullbright = false;

	clmodel = currententity->model;

	VectorAdd (currententity->origin, clmodel->mins, mins);
	VectorAdd (currententity->origin, clmodel->maxs, maxs);

	if (cull && R_CullBox (mins, maxs))
		return;

	/*
	if (cull && R_CullBlocked(mins, maxs, currententity->origin))
		return;
		*/

	// FIXME: shadecolor is supposed to be the lighting for the model, not
	// just colormod
	shadecolor[0] = currententity->colormod[0] * 2.0;
	shadecolor[1] = currententity->colormod[1] * 2.0;
	shadecolor[2] = currententity->colormod[2] * 2.0;

	VectorCopy (currententity->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	// get lighting information
	shadelight = R_LightPoint (currententity->origin);

	// always give the gun some light
	if (e == r_view_model)
		shadelight = max (shadelight, 24);

	for (lnum = 0; lnum < MAX_DLIGHTS; lnum++) {
		if (r_dlights[lnum].die >= r_realtime) {
			VectorSubtract (currententity->origin, r_dlights[lnum].origin,
							dist);
			add = (r_dlights[lnum].radius * r_dlights[lnum].radius * 8) /
				(DotProduct (dist, dist));	// FIXME Deek

			if (add > 0)
				shadelight += add;
		}
	}

	// clamp lighting so it doesn't overbright as much
	shadelight = min (shadelight, 100); // was 200

	// never allow players to go totally black
	if (strequal (clmodel->name, "progs/player.mdl")) {
		shadelight = max (shadelight, 8);
	}
	
	if (strnequal (clmodel->name, "progs/flame", 11)
			|| strnequal (clmodel->name, "progs/bolt", 10)) {
		modelIsFullbright = true;
		shadelight = 255;	// make certain models full brightness always
	}

	shadedots = r_avertexnormal_dots[((int) (e->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
	shadelight /= 200.0;

	an = e->angles[1] / 180 * M_PI;
	shadevector[0] = cos (-an);
	shadevector[1] = sin (-an);
	shadevector[2] = 1;
	VectorNormalize (shadevector);

	// locate the proper data
	paliashdr = (aliashdr_t *) Mod_Extradata (currententity->model);
	c_alias_polys += paliashdr->mdl.numtris;

	// draw all the triangles
	QFGL_glPushMatrix ();
	R_RotateForEntity (e);

	if (strequal (clmodel->name, "progs/eyes.mdl")) {
		QFGL_glTranslatef (paliashdr->mdl.scale_origin[0],
					  paliashdr->mdl.scale_origin[1],
					  paliashdr->mdl.scale_origin[2] - (22 + 8));
		// double size of eyes, since they are really hard to see in GL
		QFGL_glScalef (paliashdr->mdl.scale[0] * 2, paliashdr->mdl.scale[1] * 2,
				  paliashdr->mdl.scale[2] * 2);
	} else {
		QFGL_glTranslatef (paliashdr->mdl.scale_origin[0],
					  paliashdr->mdl.scale_origin[1],
					  paliashdr->mdl.scale_origin[2]);
		QFGL_glScalef (paliashdr->mdl.scale[0], paliashdr->mdl.scale[1],
				  paliashdr->mdl.scale[2]);
	}

	anim = (int) (r_realtime * 10) & 3;

	skinnum = currententity->skinnum;
	if ((skinnum >= paliashdr->mdl.numskins) || (skinnum < 0)) {
		Con_DPrintf ("R_AliasSetupSkin: no such skin # %d\n", skinnum);
		skinnum = 0;
	}

	texture = paliashdr->gl_texturenum[skinnum][anim];
	if (gl_fb_models->int_val && !modelIsFullbright)
		fb_texture = paliashdr->gl_fb_texturenum[skinnum][anim];

	// we can't dynamically colormap textures, so they are cached
	// seperately for the players.  Heads are just uncolored.
	if (currententity->skin && !gl_nocolors->int_val) {
		skin_t *skin = currententity->skin;

		texture = skin->texture;
		if (gl_fb_models->int_val) {
			fb_texture = skin->fb_texture;
		}
	}

	QFGL_glBindTexture (GL_TEXTURE_2D, texture);

	if (gl_affinemodels->int_val)
		QFGL_glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	if (gl_lerp_anim->int_val) {
		R_SetupAliasBlendedFrame (currententity->frame, paliashdr, currententity, false);
	} else {
		R_SetupAliasFrame (currententity->frame, paliashdr, false);
	}

	// This block is GL fullbright support for objects...
	if (fb_texture) {
		QFGL_glBindTexture (GL_TEXTURE_2D, fb_texture);
		if (gl_lerp_anim->int_val) {
			R_SetupAliasBlendedFrame (currententity->frame, paliashdr,
			                          currententity, true);
		} else {
			R_SetupAliasFrame (currententity->frame, paliashdr, true);
		}
	}

	if (gl_affinemodels->int_val)
		QFGL_glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_DONT_CARE);

	QFGL_glPopMatrix ();

	if (r_shadows->int_val) {
		// torches, grenades, and lightning bolts do not have shadows
		if (modelIsFullbright)
			return;
		if (strequal (clmodel->name, "progs/grenade.mdl"))
			return;

		QFGL_glPushMatrix ();
		R_RotateForEntity (e);

		QFGL_glDisable (GL_TEXTURE_2D);
		QFGL_glColor4f (0, 0, 0, 0.5);

		if (gl_lerp_anim->int_val) {
			GL_DrawAliasBlendedShadow (paliashdr, lastposenum0, lastposenum, currententity);
		} else {
			GL_DrawAliasShadow (paliashdr, lastposenum);
		}

		QFGL_glEnable (GL_TEXTURE_2D);
		QFGL_glColor3ubv (lighthalf_v);
		QFGL_glPopMatrix ();
	}
}


/*
	R_ShowNearestLoc

	Display the nearest symbolic location (.loc files)
*/
static void
R_ShowNearestLoc (void)
{
	location_t	*nearloc;
	vec3_t		trueloc;
	dlight_t	*dl;

	if (r_drawentities->int_val)
		return;

	nearloc = locs_find (r_origin);

	if (nearloc) {
		dl = R_AllocDlight (4096);
		VectorCopy (nearloc->loc, dl->origin);
		dl->radius = 200;
		dl->die = r_realtime + 0.1;
		dl->color[0] = 0;
		dl->color[1] = 1;
		dl->color[2] = 0;

		VectorCopy (nearloc->loc, trueloc);
		R_RunSpikeEffect (trueloc, 7);
	}
}


/*
	R_DrawEntitiesOnList

	Draw all the entities we have information on.
*/
static void
R_DrawEntitiesOnList (void)
{
	int         i;

	if (!r_drawentities->int_val) {
		R_ShowNearestLoc();
		return;
	}

	// LordHavoc: split into 3 loops to simplify state changes
	for (i = 0; i < r_numvisedicts; i++) {
		if (r_visedicts[i]->model->type != mod_brush)
			continue;
		currententity = r_visedicts[i];
		modelalpha = currententity->alpha;

		R_DrawBrushModel (currententity);
	}

	for (i = 0; i < r_numvisedicts; i++) {
		if (r_visedicts[i]->model->type != mod_alias)
			continue;
		currententity = r_visedicts[i];
		modelalpha = currententity->alpha;

		if (currententity == r_player_entity)
			currententity->angles[PITCH] *= 0.3;

		R_DrawAliasModel (currententity, true);
	}

	for (i = 0; i < r_numvisedicts; i++) {
		if (r_visedicts[i]->model->type != mod_sprite)
			continue;
		currententity = r_visedicts[i];
		modelalpha = currententity->alpha;

		R_DrawSpriteModel (currententity);
	}
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

	// this is a HACK!  --KB
	modelalpha = currententity->alpha;

	// hack the depth range to prevent view model from poking into walls
	QFGL_glDepthRange (gldepthmin, gldepthmin + 0.3 * (gldepthmax - gldepthmin));
	R_DrawAliasModel (currententity, false);
	QFGL_glDepthRange (gldepthmin, gldepthmax);
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
	R_AnimateLight ();

	r_framecount++;

	// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

	// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf (r_origin, r_worldentity.model);

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

	QFGL_glFrustum (xmin, xmax, ymin, ymax, zNear, zFar);
}


static void
R_SetupGL (void)
{
	float		screenaspect;
	extern int	glwidth, glheight;
	int			x, x2, y2, y, w, h;

	// set up viewpoint
	QFGL_glMatrixMode (GL_PROJECTION);
	QFGL_glLoadIdentity ();
	x = r_refdef.vrect.x * glwidth / vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth / vid.width;
	y = (vid.height - r_refdef.vrect.y) * glheight / vid.height;
	y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight
		/ vid.height;

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

	QFGL_glViewport (glx + x, gly + y2, w, h);
	screenaspect = (float) r_refdef.vrect.width / r_refdef.vrect.height;
	MYgluPerspective (r_refdef.fov_y, screenaspect, 4, 4096);

	if (mirror) {
		if (mirror_plane->normal[2])
			QFGL_glScalef (1, -1, 1);
		else
			QFGL_glScalef (-1, 1, 1);
		QFGL_glCullFace (GL_BACK);
	} else
		QFGL_glCullFace (GL_FRONT);

	QFGL_glMatrixMode (GL_MODELVIEW);
	QFGL_glLoadIdentity ();

	QFGL_glRotatef (-90, 1, 0, 0);			// put Z going up
	QFGL_glRotatef (90, 0, 0, 1);			// put Z going up
	QFGL_glRotatef (-r_refdef.viewangles[2], 1, 0, 0);
	QFGL_glRotatef (-r_refdef.viewangles[0], 0, 1, 0);
	QFGL_glRotatef (-r_refdef.viewangles[1], 0, 0, 1);
	QFGL_glTranslatef (-r_refdef.vieworg[0], -r_refdef.vieworg[1],
				  -r_refdef.vieworg[2]);

	QFGL_glGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);

	// set drawing parms
	QFGL_glEnable (GL_CULL_FACE);
	QFGL_glDisable (GL_ALPHA_TEST);
	QFGL_glAlphaFunc (GL_GREATER, 0.5);
	QFGL_glEnable (GL_DEPTH_TEST);
	if (gl_dlight_smooth->int_val)
		QFGL_glShadeModel (GL_SMOOTH);
	else
		QFGL_glShadeModel (GL_FLAT);
}


static void
R_Clear (void)
{
	if (gl_clear->int_val)
		QFGL_glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	else
		QFGL_glClear (GL_DEPTH_BUFFER_BIT);
	gldepthmin = 0;
	gldepthmax = 1;
	QFGL_glDepthFunc (GL_LEQUAL);

	QFGL_glDepthRange (gldepthmin, gldepthmax);
}


void
R_RenderScene (void)
{
	if (r_timegraph->int_val || r_speeds->int_val || r_dspeeds->int_val)
		r_time1 = Sys_DoubleTime ();

	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_PushDlights (vec3_origin);

	R_MarkLeaves ();			// done here so we know if we're in water

	R_DrawWorld ();				// adds static entities to the list

	S_ExtraUpdate ();			// don't let sound get messed up if going slow

	R_DrawEntitiesOnList ();

	R_RenderDlights ();

	if (r_timegraph->int_val)
		R_TimeGraph ();

	if (r_zgraph->int_val)
		R_ZGraph ();
}


void R_RenderBrushPoly (msurface_t *fa);


void
R_Mirror (void)
{
	float       d;
	msurface_t *s;
	entity_t   **ent;

	if (!mirror)
		return;

	memcpy (r_base_world_matrix, r_world_matrix, sizeof (r_base_world_matrix));

	d = DotProduct (r_refdef.vieworg, mirror_plane->normal) -
		mirror_plane->dist;
	VectorMA (r_refdef.vieworg, -2 * d, mirror_plane->normal, r_refdef.vieworg);

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
	QFGL_glDepthRange (gldepthmin, gldepthmax);
	QFGL_glDepthFunc (GL_LEQUAL);

	R_RenderScene ();
	R_DrawWaterSurfaces ();

	gldepthmin = 0;
	gldepthmax = 1;//XXX 0.5;
	QFGL_glDepthRange (gldepthmin, gldepthmax);
	QFGL_glDepthFunc (GL_LEQUAL);

	// blend on top
	QFGL_glMatrixMode (GL_PROJECTION);
	if (mirror_plane->normal[2])
		QFGL_glScalef (1, -1, 1);
	else
		QFGL_glScalef (-1, 1, 1);
	QFGL_glCullFace (GL_FRONT);
	QFGL_glMatrixMode (GL_MODELVIEW);

	QFGL_glLoadMatrixf (r_base_world_matrix);

	QFGL_glColor4f (1, 1, 1, r_mirroralpha->value);
	s = r_worldentity.model->textures[mirrortexturenum]->texturechain;
	for (; s; s = s->texturechain)
		R_RenderBrushPoly (s);
	r_worldentity.model->textures[mirrortexturenum]->texturechain = NULL;
	QFGL_glColor4f (1, 1, 1, 1);
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

//	QFGL_glFinish ();

	mirror = false;

	R_Clear ();

	// render normal view
	R_RenderScene ();

	R_DrawWaterSurfaces ();

	R_UpdateFires ();

	R_DrawParticles ();

	R_DrawViewModel ();

	// render mirror view
	R_Mirror ();
}

#if 0
qboolean R_CullBlocked (vec3_t mins, vec3_t maxs, vec3_t org)
{
	static struct trace_t  trace;
	vec3_t  point;
	float   rad;

	if (!gl_occlusion->int_val)
		return false;

	// Check the origin first
	if ( Mod_PointInLeaf(org, cl.worldmodel)->contents != CONTENTS_SOLID)
	{
		if (SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, r_refdef.vieworg, org, &trace))
			return false;
	}

	rad = sqrt((maxs[0]-org[0])*(maxs[0]-org[0]) +
			(maxs[1]-org[1])*(maxs[1]-org[1]) +
			(maxs[2]-org[2])*(maxs[2]-org[2]) );

	// Check a few points on the bounding sphere to catch rotating objects
	// Raise the origin a bit to catch droptofloor models
	point[0] = org[0]; point[1] = org[1]+rad; point[2] = org[2]+4;
	if ( Mod_PointInLeaf(point, cl.worldmodel)->contents != CONTENTS_SOLID)
	{
		if (SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, r_refdef.vieworg, point, &trace))
			return false;
	}
	point[0] = org[0]+rad; point[1] = org[1]; point[2] = org[2]+4;
	if ( Mod_PointInLeaf(point, cl.worldmodel)->contents != CONTENTS_SOLID)
	{
		if (SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, r_refdef.vieworg, point, &trace))
			return false;
	}
	point[0] = org[0]; point[1] = org[1]-rad; point[2] = org[2]+4;
	if ( Mod_PointInLeaf(point, cl.worldmodel)->contents != CONTENTS_SOLID)
	{
		if (SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, r_refdef.vieworg, point, &trace))
			return false;
	}
	point[0] = org[0]-rad; point[1] = org[1]; point[2] = org[2]+4;
	if ( Mod_PointInLeaf(point, cl.worldmodel)->contents != CONTENTS_SOLID)
	{
		if (SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, r_refdef.vieworg, point, &trace))
			return false;
	}

	// Check the poles of the sphere (can catch ents on ledges that would otherwise be missed)
	point[0] = org[0]; point[1] = org[1]; point[2] = org[2]+rad;
	if ( Mod_PointInLeaf(point, cl.worldmodel)->contents != CONTENTS_SOLID)
	{
		if (SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, r_refdef.vieworg, point, &trace))
			return false;
	}
	point[0] = org[0]; point[1] = org[1]; point[2] = org[2]-rad;
	if ( Mod_PointInLeaf(point, cl.worldmodel)->contents != CONTENTS_SOLID)
	{
		if (SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, r_refdef.vieworg, point, &trace))
			return false;
	}


	// Check the corners...
	if ( Mod_PointInLeaf(maxs, cl.worldmodel)->contents != CONTENTS_SOLID)
	{
		if (SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, r_refdef.vieworg, maxs, &trace))
			return false;
	}
	point[0] = mins[0]; point[1] = maxs[1]; point[2] = maxs[2];
	if ( Mod_PointInLeaf(point, cl.worldmodel)->contents != CONTENTS_SOLID)
	{
		if (SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, r_refdef.vieworg, point, &trace))
			return false;
	}
	point[0] = mins[0]; point[1] = mins[1]; point[2] = maxs[2];
	if ( Mod_PointInLeaf(point, cl.worldmodel)->contents != CONTENTS_SOLID)
	{
		if (SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, r_refdef.vieworg, point, &trace))
			return false;
	}
	point[0] = maxs[0]; point[1] = mins[1]; point[2] = maxs[2];
	if ( Mod_PointInLeaf(point, cl.worldmodel)->contents != CONTENTS_SOLID)
	{
		if (SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, r_refdef.vieworg, point, &trace))
			return false;
	}
	if ( Mod_PointInLeaf(mins, cl.worldmodel)->contents != CONTENTS_SOLID)
	{
		if (SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, r_refdef.vieworg, mins, &trace))
			return false;
	}
	point[0] = mins[0]; point[1] = maxs[1]; point[2] = mins[2]+4;
	if ( Mod_PointInLeaf(point, cl.worldmodel)->contents != CONTENTS_SOLID)
	{
		if (SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, r_refdef.vieworg, point, &trace))
			return false;
	}
	point[0] = maxs[0]; point[1] = mins[1]; point[2] = mins[2]+4;
	if ( Mod_PointInLeaf(point, cl.worldmodel)->contents != CONTENTS_SOLID)
	{
		if (SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, r_refdef.vieworg, point, &trace))
			return false;
	}
	point[0] = maxs[0]; point[1] = maxs[1]; point[2] = mins[2]+4;
	if ( Mod_PointInLeaf(point, cl.worldmodel)->contents != CONTENTS_SOLID)
	{
		if (SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, r_refdef.vieworg, point, &trace))
			return false;
	}

	// Model is blocked (probably)
	return true;
}
#endif
