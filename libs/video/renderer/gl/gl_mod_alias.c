/*
	gl_mod_alias.c

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
static const char rcsid[] = 
	"$Id$";

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

typedef struct {
	vec3_t      vert;
	float       lightdot;
} blended_vert_t;

typedef struct {
	blended_vert_t *verts;
	int        *order;
} vert_order_t;

float		r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float   r_avertexnormal_dots[SHADEDOT_QUANT][256] =
	#include "anorm_dots.h"
		;

float		shadelight;
float	   *shadedots = r_avertexnormal_dots[0];
int			lastposenum, lastposenum0;
vec3_t		shadevector;


static void
GL_DrawAliasFrame (vert_order_t *vo, qboolean fb)
{
	float			l;
	float			color[4];
	int				count;
	int			   *order;
	blended_vert_t *verts;

	verts = vo->verts;
	order = vo->order;

	color[3] = modelalpha;

	if (modelalpha != 1.0)
		qfglDepthMask (GL_FALSE);

	if (fb) {
		color_white[3] = modelalpha * 255;
		qfglColor4ubv (color_white);
	}

	while ((count = *order++)) {
		// get the vertex count and primitive type
		if (count < 0) {
			count = -count;
			qfglBegin (GL_TRIANGLE_FAN);
		} else {
			qfglBegin (GL_TRIANGLE_STRIP);
		}

		do {
			// texture coordinates come from the draw list
			qfglTexCoord2fv ((float *) order);
			order += 2;

			if (!fb) {
				// normals and vertexes come from the frame list
				l = shadelight * verts->lightdot;
				VectorScale (shadecolor, l, color);

				qfglColor4fv (color);
			}

			qfglVertex3fv (verts->vert);
			verts++;
		} while (--count);

		qfglEnd ();
	}

	if (modelalpha != 1.0)
		qfglDepthMask (GL_TRUE);
}

/*
	GL_DrawAliasShadow

	Standard shadow drawing
*/
static void
GL_DrawAliasShadow (aliashdr_t *paliashdr, int posenum)
{
	float		height, lheight;
	int			count;
	int		   *order;
	vec3_t		point;
	trivertx_t *verts;

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
			qfglBegin (GL_TRIANGLE_FAN);
		} else
			qfglBegin (GL_TRIANGLE_STRIP);

		do {
			// texture coordinates come from the draw list
			// (skipped for shadows) qfglTexCoord2fv ((float *)order);
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
//			height -= 0.001;
			qfglVertex3fv (point);

			verts++;
		} while (--count);

		qfglEnd ();
	}
}

/*
	GL_DrawAliasBlendedShadow
         
	Interpolated shadow drawing
*/
void
GL_DrawAliasBlendedShadow (aliashdr_t *paliashdr, int pose1, int pose2,
						   entity_t *e)
{
	float		blend, height, lheight, lerp;
	int			count;
	int		   *order;
	trivertx_t *verts1, *verts2;
	vec3_t		point1, point2;

	blend = (r_realtime - e->frame_start_time) / e->frame_interval;
	blend = min (blend, 1);
	lerp = 1 - blend;

	lheight = e->origin[2] - lightspot[2];
	height = -lheight + 1.0;

	verts2 = verts1 = (trivertx_t *) ((byte *) paliashdr +
									  paliashdr->posedata);

	verts1 += pose1 * paliashdr->poseverts;
	verts2 += pose2 * paliashdr->poseverts;

	order = (int *) ((byte *) paliashdr + paliashdr->commands);

	while ((count = *order++)) {
		// get the vertex count and primitive type
		if (count < 0) {
			count = -count;
			qfglBegin (GL_TRIANGLE_FAN);
		} else {
			qfglBegin (GL_TRIANGLE_STRIP);
		}

		do {
			order += 2;

			point1[0] =	verts1->v[0] * paliashdr->mdl.scale[0] +
				paliashdr->mdl.scale_origin[0];
			point1[1] =	verts1->v[1] * paliashdr->mdl.scale[1] +
				paliashdr->mdl.scale_origin[1];
			point1[2] =	verts1->v[2] * paliashdr->mdl.scale[2] +
				paliashdr->mdl.scale_origin[2];

			point1[0] -= shadevector[0] * (point1[2] + lheight);
			point1[1] -= shadevector[1] * (point1[2] + lheight);

			point2[0] =	verts2->v[0] * paliashdr->mdl.scale[0] +
				paliashdr->mdl.scale_origin[0];
			point2[1] =	verts2->v[1] * paliashdr->mdl.scale[1] +
				paliashdr->mdl.scale_origin[1];
			point2[2] =	verts2->v[2] * paliashdr->mdl.scale[2] +
				paliashdr->mdl.scale_origin[2];

			point2[0] -= shadevector[0] * (point2[2] + lheight);
			point2[1] -= shadevector[1] * (point2[2] + lheight);

			qfglVertex3f ((point1[0] * lerp) + (point2[0] * blend),
						(point1[1] * lerp) + (point2[1] * blend),
						height);

			verts1++;
			verts2++;
		} while (--count);
		qfglEnd ();
	}
}

vert_order_t *
GL_GetAliasFrameVerts (int frame, aliashdr_t *paliashdr, entity_t *e)
{
	float		  interval;
	int			  count, numposes, pose, i;
	trivertx_t	 *verts;
	vert_order_t *vo;

	if ((frame >= paliashdr->mdl.numframes) || (frame < 0)) {
		Con_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	verts = (trivertx_t *) ((byte *) paliashdr + paliashdr->posedata);

	count = paliashdr->poseverts;
	vo = Hunk_TempAlloc (sizeof (*vo) + count * sizeof (blended_vert_t));
	vo->order = (int *) ((byte *) paliashdr + paliashdr->commands);
	vo->verts = (blended_vert_t*)&vo[1];

	if (numposes > 1) {
		interval = paliashdr->frames[frame].interval;
		pose += (int) (r_realtime / interval) % numposes;
	} else {
		/*
			One tenth of a second is good for most Quake animations. If
			the nextthink is longer then the animation is usually meant
			to pause (e.g. check out the shambler magic animation in
			shambler.qc).  If its shorter then things will still be
			smoothed partly, and the jumps will be less noticable
			because of the shorter time.  So, this is probably a good
			assumption.
		*/
		interval = 0.1;
	}

	if (gl_lerp_anim->int_val) {
		trivertx_t *verts1, *verts2;
		float       blend, lerp;

		e->frame_interval = interval;

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
		lerp = 1 - blend;

		verts1 = verts + e->pose1 * count;
		verts2 = verts + e->pose2 * count;

		if (!blend) {
			verts = verts1;
		} else if (blend == 1) {
			verts = verts2;
		} else {
			for (i = 0; i < count; i++) {
				vo->verts[i].vert[0] = verts1[i].v[0] * lerp
										+ verts2[i].v[0] * blend;
				vo->verts[i].vert[1] = verts1[i].v[1] * lerp
										+ verts2[i].v[1] * blend;
				vo->verts[i].vert[2] = verts1[i].v[2] * lerp
										+ verts2[i].v[2] * blend;
				vo->verts[i].lightdot =
					shadedots[verts1[i].lightnormalindex] * lerp
					+ shadedots[verts2[i].lightnormalindex] * blend;
			}
			lastposenum0 = e->pose1;
			lastposenum = e->pose2;
			return vo;
		}
	} else {
		verts += pose * count;
	}
	for (i = 0; i < count; i++) {
		vo->verts[i].vert[0] = verts[i].v[0];
		vo->verts[i].vert[1] = verts[i].v[1];
		vo->verts[i].vert[2] = verts[i].v[2];
		vo->verts[i].lightdot = shadedots[verts[i].lightnormalindex];
	}
	lastposenum = pose;
	return vo;
}

void
R_DrawAliasModel (entity_t *e, qboolean cull)
{
	float		  add, an;
	int			  anim, lnum, skinnum, texture;
	int			  fb_texture = 0;
	aliashdr_t	 *paliashdr;
	model_t		 *clmodel;
	vec3_t		  dist, mins, maxs;
	vert_order_t *vo;

	clmodel = e->model;

	VectorAdd (e->origin, clmodel->mins, mins);
	VectorAdd (e->origin, clmodel->maxs, maxs);

	if (cull && R_CullBox (mins, maxs))
			return;

	// FIXME: shadecolor is supposed to be the lighting for the model, not
	// just colormod
	shadecolor[0] = e->colormod[0] * 2.0;
	shadecolor[1] = e->colormod[1] * 2.0;
	shadecolor[2] = e->colormod[2] * 2.0;
	modelalpha = e->alpha;

	VectorCopy (e->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	if (clmodel->fullbright) {
		shadelight = 1.0;	// make certain models full brightness always
	} else {
		// get lighting information
		shadelight = R_LightPoint (e->origin);

		// always give the gun some light
		if (e == r_view_model)
			shadelight = max (shadelight, 24);

		for (lnum = 0; lnum < r_maxdlights; lnum++) {
			if (r_dlights[lnum].die >= r_realtime) {
				VectorSubtract (e->origin, r_dlights[lnum].origin, dist);
				add = (r_dlights[lnum].radius * r_dlights[lnum].radius * 8) /
					(DotProduct (dist, dist));	// FIXME Deek

				if (add > 0)
					shadelight += add;
			}
		}

		// clamp lighting so it doesn't overbright as much
		shadelight = min (shadelight, 100); // was 200

		// never allow players to go totally black
		shadelight = max (shadelight, clmodel->min_light);

		shadelight *= 0.005;

		shadedots = r_avertexnormal_dots[(int) (e->angles[1] *
												(SHADEDOT_QUANT / 360.0)) &
										 (SHADEDOT_QUANT - 1)];
		an = e->angles[1] * (M_PI / 180);
		shadevector[0] = cos (-an);
		shadevector[1] = sin (-an);
		shadevector[2] = 1;
		VectorNormalize (shadevector);
	}

	// locate the proper data
	paliashdr = Cache_Get (&e->model->cache);
	c_alias_polys += paliashdr->mdl.numtris;

	// draw all the triangles
	qfglPushMatrix ();
	R_RotateForEntity (e);

	qfglTranslatef (paliashdr->mdl.scale_origin[0],
					paliashdr->mdl.scale_origin[1],
					paliashdr->mdl.scale_origin[2]);
	qfglScalef (paliashdr->mdl.scale[0], paliashdr->mdl.scale[1],
				paliashdr->mdl.scale[2]);

	anim = (int) (r_realtime * 10) & 3;

	skinnum = e->skinnum;
	if ((skinnum >= paliashdr->mdl.numskins) || (skinnum < 0)) {
		Con_DPrintf ("R_AliasSetupSkin: no such skin # %d\n", skinnum);
		skinnum = 0;
	}

	texture = paliashdr->gl_texturenum[skinnum][anim];
	if (gl_fb_models->int_val && !clmodel->fullbright)
		fb_texture = paliashdr->gl_fb_texturenum[skinnum][anim];

	// we can't dynamically colormap textures, so they are cached
	// seperately for the players.  Heads are just uncolored.
	if (e->skin && !gl_nocolors->int_val) {
		skin_t *skin = e->skin;

		texture = skin->texture;
		if (gl_fb_models->int_val) {
			fb_texture = skin->fb_texture;
		}
	}

	qfglBindTexture (GL_TEXTURE_2D, texture);

	if (gl_affinemodels->int_val)
		qfglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	vo = GL_GetAliasFrameVerts (e->frame, paliashdr, e);

	GL_DrawAliasFrame (vo, false);

	// This block is GL fullbright support for objects...
	if (fb_texture) {
		qfglBindTexture (GL_TEXTURE_2D, fb_texture);
		GL_DrawAliasFrame (vo, true);
	}

	if (gl_affinemodels->int_val)
		qfglHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_DONT_CARE);

	qfglPopMatrix ();

	if (r_shadows->int_val) {
		// torches, grenades, and lightning bolts do not have shadows
		if (!clmodel->shadow_alpha)
			return;

		qfglPushMatrix ();
		R_RotateForEntity (e);

		qfglDisable (GL_TEXTURE_2D);
		color_black[3] = (clmodel->shadow_alpha + 1) / 2;
		qfglColor4ubv (color_black);

		if (gl_lerp_anim->int_val) {
			GL_DrawAliasBlendedShadow (paliashdr, lastposenum0, lastposenum,
									   e);
		} else {
			GL_DrawAliasShadow (paliashdr, lastposenum);
		}

		qfglEnable (GL_TEXTURE_2D);
		qfglPopMatrix ();
	}

	Cache_Release (&e->model->cache);
}
