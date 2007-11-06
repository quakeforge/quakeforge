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
#include "QF/GL/qf_screen.h"
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

float		r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float   r_avertexnormal_dots[SHADEDOT_QUANT][256] = {
#include "anorm_dots.h"
};

vec3_t		shadevector;


static inline void
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

static inline void
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

static inline vert_order_t *
GL_GetAliasFrameVerts16 (int frame, aliashdr_t *paliashdr, entity_t *e)
{
	float         interval;
	int           count, numposes, pose, i;
	trivertx16_t *verts;
	vert_order_t *vo;
	blended_vert_t *vo_v;

	if ((frame >= paliashdr->mdl.numframes) || (frame < 0)) {
		if (developer->int_val)
			Sys_DPrintf ("R_AliasSetupFrame: no such frame %d %s\n", frame,
						 currententity->model->name);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;
	verts = (trivertx16_t *) ((byte *) paliashdr + paliashdr->posedata);

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
		trivertx16_t *verts1, *verts2;
		float       blend;

		e->frame_interval = interval;

		if (e->pose2 != pose) {
			e->frame_start_time = r_realtime;
			if (e->pose2 == -1) {
				e->pose1 = pose;
			} else {
				e->pose1 = e->pose2;
			}
			e->pose2 = pose;
			blend = 0.0;
		} else if (r_paused) {
			blend = 1.0;
		} else {
			blend = (r_realtime - e->frame_start_time) / e->frame_interval;
			blend = min (blend, 1.0);
		}

		if (blend == 0.0) {
			verts = verts + e->pose1 * count;
		} else if (blend == 1.0) {
			verts = verts + e->pose2 * count;
		} else {
			verts1 = verts + e->pose1 * count;
			verts2 = verts + e->pose2 * count;

			for (i = 0, vo_v = vo->verts; i < count;
				 i++, vo_v++, verts1++, verts2++) {
				float      *n1, *n2;

				VectorBlend (verts1->v, verts2->v, blend, vo_v->vert);
				n1 = r_avertexnormals[verts1->lightnormalindex];
				n2 = r_avertexnormals[verts2->lightnormalindex];
				VectorBlend (n1, n2, blend, vo_v->normal);
				if (VectorIsZero (vo_v->normal)) {
					if (blend < 0.5) {
						VectorCopy (n1, vo_v->normal);
					} else {
						VectorCopy (n2, vo_v->normal);
					}
				}
			}
			return vo;
		}
	} else {
		verts += pose * count;
	}

	for (i = 0, vo_v = vo->verts; i < count; i++, vo_v++, verts++) {
		VectorCopy (verts->v, vo_v->vert);
		VectorCopy (r_avertexnormals[verts->lightnormalindex], vo_v->normal);
	}
	return vo;
}

static inline vert_order_t *
GL_GetAliasFrameVerts (int frame, aliashdr_t *paliashdr, entity_t *e)
{
	float       interval;
	int         count, numposes, pose, i;
	trivertx_t *verts;
	vert_order_t *vo;
	blended_vert_t *vo_v;

	if ((frame >= paliashdr->mdl.numframes) || (frame < 0)) {
		if (developer->int_val)
			Sys_DPrintf ("R_AliasSetupFrame: no such frame %d %s\n", frame,
						 currententity->model->name);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	verts = (trivertx_t *) ((byte *) paliashdr + paliashdr->posedata);

	count = paliashdr->poseverts;
	vo = Hunk_TempAlloc (sizeof (*vo) + count * sizeof (blended_vert_t));
	vo->order = (int *) ((byte *) paliashdr + paliashdr->commands);
	vo->verts = (blended_vert_t *) &vo[1];
	if (paliashdr->tex_coord) {
		vo->tex_coord = (tex_coord_t *) ((byte *) paliashdr + paliashdr->tex_coord);
	} else {
		vo->tex_coord = NULL;
	}
	vo->count = count;

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
		float       blend;

		e->frame_interval = interval;

		if (e->pose2 != pose) {
			e->frame_start_time = r_realtime;
			if (e->pose2 == -1) {
				e->pose1 = pose;
			} else {
				e->pose1 = e->pose2;
			}
			e->pose2 = pose;
			blend = 0.0;
		} else if (r_paused) {
			blend = 1.0;
		} else {
			blend = (r_realtime - e->frame_start_time) / e->frame_interval;
			blend = min (blend, 1.0);
		}

		if (blend == 0.0) {
			verts = verts + e->pose1 * count;
		} else if (blend == 1.0) {
			verts = verts + e->pose2 * count;
		} else {
			verts1 = verts + e->pose1 * count;
			verts2 = verts + e->pose2 * count;

			for (i = 0, vo_v = vo->verts; i < count;
				 i++, vo_v++, verts1++, verts2++) {
				float      *n1, *n2;

				VectorBlend (verts1->v, verts2->v, blend, vo_v->vert);
				n1 = r_avertexnormals[verts1->lightnormalindex];
				n2 = r_avertexnormals[verts2->lightnormalindex];
				VectorBlend (n1, n2, blend, vo_v->normal);
				if (VectorIsZero (vo_v->normal)) {
					if (blend < 0.5) {
						VectorCopy (n1, vo_v->normal);
					} else {
						VectorCopy (n2, vo_v->normal);
					}
				}
			}
			return vo;
		}
	} else {
		verts += pose * count;
	}

	for (i = 0, vo_v = vo->verts; i < count; i++, vo_v++, verts++) {
		VectorCopy (verts->v, vo_v->vert);
		VectorCopy (r_avertexnormals[verts->lightnormalindex], vo_v->normal);
	}
	return vo;
}

static maliasskindesc_t *
R_AliasGetSkindesc (int skinnum, aliashdr_t *ahdr)
{
	maliasskindesc_t *pskindesc;
	maliasskingroup_t *paliasskingroup;

	if ((skinnum >= ahdr->mdl.numskins) || (skinnum < 0)) {
		Sys_DPrintf ("R_AliasSetupSkin: no such skin # %d\n", skinnum);
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

	model = e->model;

	radius = model->radius;
	if (e->scale != 1.0)
		radius *= e->scale;
	if (R_CullSphere (e->origin, radius))
		return;

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
	paliashdr = Cache_Get (&e->model->cache);
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
		vo = GL_GetAliasFrameVerts16 (e->frame, paliashdr, e);
	} else {
		VectorScale (paliashdr->mdl.scale, e->scale, scale);
		vo = GL_GetAliasFrameVerts (e->frame, paliashdr, e);
	}

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
