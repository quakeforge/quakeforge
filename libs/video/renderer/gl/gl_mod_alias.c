/*
	gl_mod_alias.c

	Draw Alias Model

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
#include "QF/GL/qf_vid.h"

#include "compat.h"
#include "r_cvar.h"
#include "r_dynamic.h"
#include "r_local.h"
#include "view.h"

typedef struct {
	vec3_t      normal;
	vec3_t      vert;
} blended_vert_t;

typedef struct {
	blended_vert_t *verts;
	int        *order;
	tex_coord_t *tex_coord;
	int         count;
} vert_order_t;

typedef struct {
	short       pose1;
	short       pose2;
	float       blend;
	vec3_t      origin;
	vec3_t      angles;
} lerpdata_t;

float		r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float   r_avertexnormal_dots[SHADEDOT_QUANT][256] = {
#include "anorm_dots.h"
};

vec3_t		shadevector;


static void
GL_DrawAliasFrameTri (vert_order_t *vo)
{
	int         count = vo->count;
	blended_vert_t *verts = vo->verts;
	tex_coord_t *tex_coord = vo->tex_coord;

	qfglBegin (GL_TRIANGLES);
	do {
		// texture coordinates come from the draw list
		qfglTexCoord2fv (tex_coord->st);
		tex_coord++;

		// normals and vertices come from the frame list
		qfglNormal3fv (verts->normal);
		qfglVertex3fv (verts->vert);
		verts++;
	} while (count--);
	qfglEnd ();
}

static inline void
GL_DrawAliasFrameTriMulti (vert_order_t *vo)
{
	int         count = vo->count;
	blended_vert_t *verts = vo->verts;
	tex_coord_t *tex_coord = vo->tex_coord;

	qfglBegin (GL_TRIANGLES);
	do {
		// texture coordinates come from the draw list
		qglMultiTexCoord2fv (gl_mtex_enum + 0, tex_coord->st);
		qglMultiTexCoord2fv (gl_mtex_enum + 1, tex_coord->st);
		tex_coord++;

		// normals and vertices come from the frame list
		qfglNormal3fv (verts->normal);
		qfglVertex3fv (verts->vert);
		verts++;
	} while (--count);
	qfglEnd ();
}

static void
GL_DrawAliasFrame (vert_order_t *vo)
{
	int         count;
	int        *order = vo->order;
	blended_vert_t *verts = vo->verts;

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

			// normals and vertices come from the frame list
			qfglNormal3fv (verts->normal);
			qfglVertex3fv (verts->vert);
			verts++;
		} while (--count);

		qfglEnd ();
	}
}

static inline void
GL_DrawAliasFrameMulti (vert_order_t *vo)
{
	int         count;
	int        *order = vo->order;
	blended_vert_t *verts = vo->verts;

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
			qglMultiTexCoord2fv (gl_mtex_enum + 0, (float *) order);
			qglMultiTexCoord2fv (gl_mtex_enum + 1, (float *) order);
			order += 2;

			// normals and vertices come from the frame list
			qfglNormal3fv (verts->normal);
			qfglVertex3fv (verts->vert);
			verts++;
		} while (--count);

		qfglEnd ();
	}
}

/*
	GL_DrawAliasShadow

	Standard shadow drawing
*/
static void
GL_DrawAliasShadow (aliashdr_t *paliashdr, vert_order_t *vo)
{
	float       height, lheight;
	int         count;
	int        *order = vo->order;
	vec3_t      point;
	blended_vert_t *verts = vo->verts;

	lheight = currententity->origin[2] - lightspot[2];
	height = -lheight + 1.0;

	while ((count = *order++)) {
		// get the vertex count and primitive type
		if (count < 0) {
			count = -count;
			qfglBegin (GL_TRIANGLE_FAN);
		} else
			qfglBegin (GL_TRIANGLE_STRIP);

		do {
			order += 2;		// skip texture coords

			// normals and vertices come from the frame list
			point[0] =
				verts->vert[0] * paliashdr->mdl.scale[0] +
				paliashdr->mdl.scale_origin[0];
			point[1] =
				verts->vert[1] * paliashdr->mdl.scale[1] +
				paliashdr->mdl.scale_origin[1];
			point[2] =
				verts->vert[2] * paliashdr->mdl.scale[2] +
				paliashdr->mdl.scale_origin[2] + lheight;

			point[0] -= shadevector[0] * point[2];
			point[1] -= shadevector[1] * point[2];
			point[2] = height;
			qfglVertex3fv (point);

			verts++;
		} while (--count);

		qfglEnd ();
	}
}

static inline void
gl_calc_blend16 (byte *posedata, lerpdata_t *lerpdata, vert_order_t *vo,
				int count)
{
	blended_vert_t *vo_v;
	trivertx16_t *verts;
	trivertx16_t *verts1, *verts2;
	int         i;

	verts = (trivertx16_t *) posedata;
	if (lerpdata->blend == 0.0) {
		verts = verts + lerpdata->pose1 * count;
	} else if (lerpdata->blend == 1.0) {
		verts = verts + lerpdata->pose2 * count;
	} else {
		verts1 = verts + lerpdata->pose1 * count;
		verts2 = verts + lerpdata->pose2 * count;

		for (i = 0, vo_v = vo->verts; i < count;
			 i++, vo_v++, verts1++, verts2++) {
			float      *n1, *n2;

			VectorBlend (verts1->v, verts2->v, lerpdata->blend, vo_v->vert);
			n1 = r_avertexnormals[verts1->lightnormalindex];
			n2 = r_avertexnormals[verts2->lightnormalindex];
			VectorBlend (n1, n2, lerpdata->blend, vo_v->normal);
			if (VectorIsZero (vo_v->normal)) {
				if (lerpdata->blend < 0.5) {
					VectorCopy (n1, vo_v->normal);
				} else {
					VectorCopy (n2, vo_v->normal);
				}
			}
		}
		return;
	}

	for (i = 0, vo_v = vo->verts; i < count; i++, vo_v++, verts++) {
		VectorCopy (verts->v, vo_v->vert);
		VectorCopy (r_avertexnormals[verts->lightnormalindex], vo_v->normal);
	}
}

static inline void
gl_calc_blend8 (byte *posedata, lerpdata_t *lerpdata, vert_order_t *vo,
				int count)
{
	blended_vert_t *vo_v;
	trivertx_t *verts;
	trivertx_t *verts1, *verts2;
	int         i;

	verts = (trivertx_t *) posedata;
	if (lerpdata->blend == 0.0) {
		verts = verts + lerpdata->pose1 * count;
	} else if (lerpdata->blend == 1.0) {
		verts = verts + lerpdata->pose2 * count;
	} else {
		verts1 = verts + lerpdata->pose1 * count;
		verts2 = verts + lerpdata->pose2 * count;

		for (i = 0, vo_v = vo->verts; i < count;
			 i++, vo_v++, verts1++, verts2++) {
			float      *n1, *n2;

			VectorBlend (verts1->v, verts2->v, lerpdata->blend, vo_v->vert);
			n1 = r_avertexnormals[verts1->lightnormalindex];
			n2 = r_avertexnormals[verts2->lightnormalindex];
			VectorBlend (n1, n2, lerpdata->blend, vo_v->normal);
			if (VectorIsZero (vo_v->normal)) {
				if (lerpdata->blend < 0.5) {
					VectorCopy (n1, vo_v->normal);
				} else {
					VectorCopy (n2, vo_v->normal);
				}
			}
		}
		return;
	}

	for (i = 0, vo_v = vo->verts; i < count; i++, vo_v++, verts++) {
		VectorCopy (verts->v, vo_v->vert);
		VectorCopy (r_avertexnormals[verts->lightnormalindex], vo_v->normal);
	}
}

static inline vert_order_t *
GL_GetAliasFrameVerts (aliashdr_t *paliashdr, lerpdata_t *lerpdata)
{
	int         count;
	vert_order_t *vo;
	byte       *posedata;

	posedata = (byte *) paliashdr + paliashdr->posedata;
	count = paliashdr->poseverts;

	vo = Hunk_TempAlloc (sizeof (*vo) + count * sizeof (blended_vert_t));
	vo->order = (int *) ((byte *) paliashdr + paliashdr->commands);
	vo->verts = (blended_vert_t *) &vo[1];
	if (paliashdr->tex_coord) {
		vo->tex_coord = (tex_coord_t *) ((byte *) paliashdr
										 + paliashdr->tex_coord);
	} else {
		vo->tex_coord = NULL;
	}
	vo->count = count;

	if (paliashdr->mdl.ident == HEADER_MDL16)
		gl_calc_blend16 (posedata, lerpdata, vo, count);
	else
		gl_calc_blend8 (posedata, lerpdata, vo, count);

	return vo;
}

static maliasskindesc_t *
R_AliasGetSkindesc (int skinnum, aliashdr_t *ahdr)
{
	maliasskindesc_t *pskindesc;
	maliasskingroup_t *paliasskingroup;

	if ((skinnum >= ahdr->mdl.numskins) || (skinnum < 0)) {
		Sys_MaskPrintf (SYS_DEV, "R_AliasGetSkindesc: no such skin # %d\n",
						skinnum);
		skinnum = 0;
	}

	pskindesc = ((maliasskindesc_t *)
				 ((byte *) ahdr + ahdr->skindesc)) + skinnum;

	if (pskindesc->type == ALIAS_SKIN_GROUP) {
		int         numskins, i;
		float       fullskininterval, skintargettime, skintime;
		float      *pskinintervals;

		paliasskingroup = (maliasskingroup_t *) ((byte *) ahdr +
												 pskindesc->skin);
		pskinintervals = (float *)
			((byte *) ahdr + paliasskingroup->intervals);
		numskins = paliasskingroup->numskins;
		fullskininterval = pskinintervals[numskins - 1];

		skintime = r_realtime + currententity->syncbase;

		skintargettime = skintime -
			((int) (skintime / fullskininterval)) * fullskininterval;
		for (i = 0; i < (numskins - 1); i++) {
			if (pskinintervals[i] > skintargettime)
				break;
		}
		pskindesc = &paliasskingroup->skindescs[i];
	}

	return pskindesc;
}

static void
r_alais_setup_lerp (aliashdr_t *paliashdr, entity_t *e, lerpdata_t *lerpdata)
{
	int         frame = e->frame;
	int         posenum, numposes;

	if ((frame >= paliashdr->mdl.numframes) || (frame < 0)) {
		Sys_MaskPrintf (SYS_DEV, "r_alais_setup_lerp: no such frame %d %s\n",
						frame, currententity->model->name);
		frame = 0;
	}

	posenum = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1) {
		e->lerptime = paliashdr->frames[frame].interval;
		posenum += (int) (r_realtime / e->lerptime) % numposes;
	} else {
		e->lerptime = 0.1;
	}

	if (e->lerpflags & LERP_RESETANIM) {
		//kill any lerp in progress
		e->lerpstart = 0;
		e->previouspose = posenum;
		e->currentpose = posenum;
		e->lerpflags &= ~LERP_RESETANIM;
	} else if (e->currentpose != posenum) {
		// pose changed, start new lerp
		if (e->lerpflags & LERP_RESETANIM2) {
			//defer lerping one more time
			e->lerpstart = 0;
			e->previouspose = posenum;
			e->currentpose = posenum;
			e->lerpflags &= ~LERP_RESETANIM2;
		} else {
			e->lerpstart = r_realtime;
			e->previouspose = e->currentpose;
			e->currentpose = posenum;
		}
	}
	if (gl_lerp_anim->int_val
		/*&& !(e->model->flags & MOD_NOLERP && gl_lerp_anim->int_val != 2)*/) {
		float       interval = e->lerpfinish - e->lerpstart;
		float       time = r_realtime - e->lerpstart;
		if (e->lerpflags & LERP_FINISH && numposes == 1)
			lerpdata->blend = bound (0, (time) / (interval), 1);
		else
			lerpdata->blend = bound (0, (time) / e->lerptime, 1);
		lerpdata->pose1 = e->previouspose;
		lerpdata->pose2 = e->currentpose;
	} else {
		lerpdata->blend = 1;
		lerpdata->pose1 = posenum;
		lerpdata->pose2 = posenum;
	}
}

static void
r_alias_lerp_transform (entity_t *e, lerpdata_t *lerpdata)
{
	float       blend;
	vec3_t      d;
	int         i;

	if (e->lerpflags & LERP_RESETMOVE) {
		// kill any lerps in progress
		e->movelerpstart = 0;
		VectorCopy (e->origin, e->previousorigin);
		VectorCopy (e->origin, e->currentorigin);
		VectorCopy (e->angles, e->previousangles);
		VectorCopy (e->angles, e->currentangles);
		e->lerpflags &= ~LERP_RESETMOVE;
	} else if (!VectorCompare (e->origin, e->currentorigin)
			   || !VectorCompare (e->angles, e->currentangles)) {
		// origin/angles changed, start new lerp
		e->movelerpstart = r_realtime;
		VectorCopy (e->currentorigin, e->previousorigin);
		VectorCopy (e->origin,  e->currentorigin);
		VectorCopy (e->currentangles, e->previousangles);
		VectorCopy (e->angles,  e->currentangles);
	}

	if (gl_lerp_anim->int_val /* && e != &cl.viewent */
		&& e->lerpflags & LERP_MOVESTEP) {
		float       interval = e->lerpfinish - e->lerpstart;
		float       time = r_realtime - e->movelerpstart;
		if (e->lerpflags & LERP_FINISH)
			blend = bound (0, (time) / (interval), 1);
		else
			blend = bound (0, (time) / 0.1, 1);

		VectorBlend (e->previousorigin, e->currentorigin, blend,
					 lerpdata->origin);

		//FIXME use quaternions?
		VectorSubtract (e->currentangles, e->previousangles, d);
		for (i = 0; i < 3; i++) {
			if (d[i] > 180)
				d[i] -= 360;
			if (d[i] < -180)
				d[i] += 360;
		}
		VectorMultAdd (e->previousangles, blend, d, lerpdata->angles);
	} else {
		//don't lerp
		VectorCopy (e->origin, lerpdata->origin);
		VectorCopy (e->angles, lerpdata->angles);
	}
}

void
R_DrawAliasModel (entity_t *e)
{
	float       radius, minlight, d;
	float       position[4] = {0.0, 0.0, 0.0, 1.0},
				color[4] = {0.0, 0.0, 0.0, 1.0},
				dark[4] = {0.0, 0.0, 0.0, 1.0},
				emission[4] = {0.0, 0.0, 0.0, 1.0};
	int         gl_light, texture;
	int         fb_texture = 0, used_lights = 0;
	qboolean    is_fullbright = false;
	unsigned    lnum;
	aliashdr_t *paliashdr;
	dlight_t   *l;
	model_t    *model;
	vec3_t      dist, scale;
	vert_order_t *vo;
	lerpdata_t  lerpdata;

	paliashdr = Cache_Get (&e->model->cache);
	r_alais_setup_lerp (paliashdr, e, &lerpdata);
	r_alias_lerp_transform (e, &lerpdata);

	model = e->model;

	radius = model->radius;
	if (e->scale != 1.0)
		radius *= e->scale;
	if (R_CullSphere (e->origin, radius)) {
		Cache_Release (&e->model->cache);
		return;
	}

	VectorSubtract (r_origin, e->origin, modelorg);

	modelalpha = e->colormod[3];

	is_fullbright = (model->fullbright || e->fullbright);
	minlight = max (model->min_light, e->min_light);

	qfglColor4fv (e->colormod);
	
	if (!is_fullbright) {
		float lightadj;

		// get lighting information
		R_LightPoint (e->origin);

		lightadj = (ambientcolor[0] + ambientcolor[1] + ambientcolor[2]) / 765.0;

		// Do minlight stuff here since that's how software does it :)

		if (lightadj > 0) {
			if (lightadj < minlight)
				lightadj = minlight / lightadj;
			else
				lightadj = 1.0;
		
			// 255 is fullbright
			VectorScale (ambientcolor, lightadj / 255.0, ambientcolor);
		} else {
			ambientcolor[0] = ambientcolor[1] = ambientcolor[2] = minlight;
		}

		if (gl_vector_light->int_val) {
			for (l = r_dlights, lnum = 0; lnum < r_maxdlights; lnum++, l++) {
				if (l->die >= r_realtime) {
					VectorSubtract (l->origin, e->origin, dist);
					if ((d = DotProduct (dist, dist)) >	// Out of range
						((l->radius + radius) * (l->radius + radius))) {
						continue;
					}
					
					
					if (used_lights >= gl_max_lights) {
						// For solid lighting, multiply by 0.5 since it's cos
						// 60 and 60 is a good guesstimate at the average
						// incident angle. Seems to match vector lighting
						// best, too.
						VectorMultAdd (emission,
							0.5 / ((d * 0.01 / l->radius) + 0.5),
							l->color, emission);
						continue;
					}

					VectorCopy (l->origin, position);

					VectorCopy (l->color, color);
					color[3] = 1.0;

					gl_light = GL_LIGHT0 + used_lights;
					qfglEnable (gl_light);
					qfglLightfv (gl_light, GL_POSITION, position);
					qfglLightfv (gl_light, GL_AMBIENT, color);
					qfglLightfv (gl_light, GL_DIFFUSE, color);
					qfglLightfv (gl_light, GL_SPECULAR, color);
					// 0.01 is used here because it just seemed to match
					// the bmodel lighting best. it's over r instead of r*r
					// so that larger-radiused lights will be brighter
					qfglLightf (gl_light, GL_QUADRATIC_ATTENUATION,
								0.01 / (l->radius));
					used_lights++;
				}
			}

			VectorAdd (ambientcolor, emission, emission);
			d = max (emission[0], max (emission[1], emission[2]));
			// 1.5 to allow some pastelization (curb darkness from dlight)
			if (d > 1.5) {
				VectorScale (emission, 1.5 / d, emission);
			}

			qfglMaterialfv (GL_FRONT, GL_EMISSION, emission);
		} else {
			VectorCopy (ambientcolor, emission);

			for (l = r_dlights, lnum = 0; lnum < r_maxdlights; lnum++, l++) {
				if (l->die >= r_realtime) {
					VectorSubtract (l->origin, e->origin, dist);
					
					if ((d = DotProduct (dist, dist)) > (l->radius + radius) *
						(l->radius + radius)) {
							continue;
					}
					
					// For solid lighting, multiply by 0.5 since it's cos 60
					// and 60 is a good guesstimate at the average incident
					// angle. Seems to match vector lighting best, too.
					VectorMultAdd (emission,
						(0.5 / ((d * 0.01 / l->radius) + 0.5)),
						l->color, emission);
				}
			}
			
			d = max (emission[0], max (emission[1], emission[2]));			
			// 1.5 to allow some fading (curb emission making stuff dark)
			if (d > 1.5) {
				VectorScale (emission, 1.5 / d, emission);
			}

			emission[0] *= e->colormod[0];
			emission[1] *= e->colormod[1];
			emission[2] *= e->colormod[2];
			emission[3] *= e->colormod[3];

			qfglColor4fv (emission);
		}
	}

	// locate the proper data
	c_alias_polys += paliashdr->mdl.numtris;

	// if the model has a colorised/external skin, use it, otherwise use
	// the skin embedded in the model data
	if (e->skin && !gl_nocolors->int_val) {
		skin_t *skin = e->skin;

		texture = skin->texture;
		if (gl_fb_models->int_val) {
			fb_texture = skin->fb_texture;
		}
	} else {
		maliasskindesc_t *skindesc;

		skindesc = R_AliasGetSkindesc (e->skinnum, paliashdr);
		texture = skindesc->texnum;
		if (gl_fb_models->int_val && !is_fullbright)
			fb_texture = skindesc->fb_texnum;
	}

	if (paliashdr->mdl.ident == HEADER_MDL16) {
		VectorScale (paliashdr->mdl.scale, e->scale / 256.0, scale);
	} else {
		VectorScale (paliashdr->mdl.scale, e->scale, scale);
	}
	vo = GL_GetAliasFrameVerts (paliashdr, &lerpdata);

	// setup the transform
	qfglPushMatrix ();
	R_RotateForEntity (e);

	qfglTranslatef (paliashdr->mdl.scale_origin[0],
					paliashdr->mdl.scale_origin[1],
					paliashdr->mdl.scale_origin[2]);
	qfglScalef (scale[0], scale[1], scale[2]);

	if (modelalpha < 1.0)
		qfglDepthMask (GL_FALSE);

	// draw all the triangles
	if (is_fullbright) {
		qfglBindTexture (GL_TEXTURE_2D, texture);
		
		if (gl_vector_light->int_val) {
			qfglDisable (GL_LIGHTING);
			if (!tess)
				qfglDisable (GL_NORMALIZE);
		}
		
		if (vo->tex_coord)
			GL_DrawAliasFrameTri (vo);
		else 
			GL_DrawAliasFrame (vo);
		
		if (gl_vector_light->int_val) {
			if (!tess)
				qfglEnable (GL_NORMALIZE);
			qfglEnable (GL_LIGHTING);
		}
	} else if (!fb_texture) {
		// Model has no fullbrights, don't bother with multi
		qfglBindTexture (GL_TEXTURE_2D, texture);
		if (vo->tex_coord)
			GL_DrawAliasFrameTri (vo);
		else
			GL_DrawAliasFrame (vo);
	} else {	// try multitexture
		if (gl_mtex_active_tmus >= 2) {	// set up the textures
			qglActiveTexture (gl_mtex_enum + 0);
			qfglBindTexture (GL_TEXTURE_2D, texture);

			qglActiveTexture (gl_mtex_enum + 1);
			qfglEnable (GL_TEXTURE_2D);
			qfglBindTexture (GL_TEXTURE_2D, fb_texture);

			// do the heavy lifting
			if (vo->tex_coord)
				GL_DrawAliasFrameTriMulti (vo);
			else
				GL_DrawAliasFrameMulti (vo);

			// restore the settings
			qfglDisable (GL_TEXTURE_2D);
			qglActiveTexture (gl_mtex_enum + 0);
		} else {
			if (vo->tex_coord) {
				qfglBindTexture (GL_TEXTURE_2D, texture);
				GL_DrawAliasFrameTri (vo);

				if (gl_vector_light->int_val) {
					qfglDisable (GL_LIGHTING);
					if (!tess)
						qfglDisable (GL_NORMALIZE);
				}

				qfglColor4fv (e->colormod);

				qfglBindTexture (GL_TEXTURE_2D, fb_texture);								
				GL_DrawAliasFrameTri (vo);
				
				if (gl_vector_light->int_val) {
					qfglEnable (GL_LIGHTING);
					if (!tess)
						qfglEnable (GL_NORMALIZE);
				}
			} else {
				qfglBindTexture (GL_TEXTURE_2D, texture);
				GL_DrawAliasFrame (vo);

				if (gl_vector_light->int_val) {
					qfglDisable (GL_LIGHTING);
					if (!tess)
						qfglDisable (GL_NORMALIZE);
				}
				
				qfglColor4fv (e->colormod);
				
				qfglBindTexture (GL_TEXTURE_2D, fb_texture);
				GL_DrawAliasFrame (vo);
				
				if (gl_vector_light->int_val) {
					qfglEnable (GL_LIGHTING);
					if (!tess)
						qfglEnable (GL_NORMALIZE);
				}
			}
		}
	}

	qfglPopMatrix ();

	// torches, grenades, and lightning bolts do not have shadows
	if (r_shadows->int_val && model->shadow_alpha) {
		qfglPushMatrix ();
		R_RotateForEntity (e);

		if (!tess)
			qfglDisable (GL_NORMALIZE);
		qfglDisable (GL_LIGHTING);
		qfglDisable (GL_TEXTURE_2D);
		qfglDepthMask (GL_FALSE);

		if (modelalpha < 1.0) {
			VectorBlend (e->colormod, dark, 0.5, color);
			color[3] = modelalpha * (model->shadow_alpha / 255.0);
			qfglColor4fv (color);
		} else {
			color_black[3] = model->shadow_alpha;
			qfglColor4ubv (color_black);
		}
		GL_DrawAliasShadow (paliashdr, vo);

		qfglDepthMask (GL_TRUE);
		qfglEnable (GL_TEXTURE_2D);
		qfglEnable (GL_LIGHTING);
		if (!tess)
			qfglEnable (GL_NORMALIZE);
		qfglPopMatrix ();
	} else if (modelalpha < 1.0) {
		qfglDepthMask (GL_TRUE);
	}

	while (used_lights--) {
		qfglDisable (GL_LIGHT0 + used_lights);
	}

	Cache_Release (&e->model->cache);
}
