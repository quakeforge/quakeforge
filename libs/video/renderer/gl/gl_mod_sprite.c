/*
	gl_mod_sprite.c

	sprite model rendering

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

static __attribute__ ((used)) const char rcsid[] = "$Id$";

#define NH_DEFINE
#include "namehack.h"

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"

#include "QF/model.h"
#include "QF/render.h"
#include "QF/sys.h"

#include "compat.h"
#include "r_internal.h"
#include "varrays.h"

static int						 sVAsize;
static int						*sVAindices;
varray_t2f_c4ub_v3f_t	*gl_spriteVertexArray;

void (*gl_R_DrawSpriteModel) (struct entity_s *ent);


static mspriteframe_t *
R_GetSpriteFrame (entity_t *currententity)
{
	float			fullinterval, targettime, time;
	float		   *pintervals;
	int				frame, numframes, i;
	msprite_t      *psprite;
	mspriteframe_t *pspriteframe;
	mspritegroup_t *pspritegroup;

	psprite = currententity->model->cache.data;
	frame = currententity->frame;

	if ((frame >= psprite->numframes) || (frame < 0)) {
		Sys_MaskPrintf (SYS_DEV, "R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE) {
		pspriteframe = psprite->frames[frame].frameptr;
	} else {
		pspritegroup = (mspritegroup_t *) psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes - 1];

		time = vr_data.realtime + currententity->syncbase;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval
		// values are positive, so we don't have to worry about division by 0
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
R_DrawSpriteModel_f (entity_t *e)
{
	float			 modelalpha, color[4];
	float			*up, *right;
	msprite_t		*psprite;
	mspriteframe_t	*frame;
	vec3_t			 point, point1, point2, v_up;

	// don't bother culling, it's just a single polygon without a surface cache
	frame = R_GetSpriteFrame (e);
	psprite = e->model->cache.data;

	if (psprite->type == SPR_ORIENTED) {	// bullet marks on walls
		up = e->transform + 2 * 4;
		right = e->transform + 1 * 4;
	} else if (psprite->type == SPR_VP_PARALLEL_UPRIGHT) {
		v_up[0] = 0;
		v_up[1] = 0;
		v_up[2] = 1;
		up = v_up;
		right = vright;
	} else {								// normal sprite
		up = vup;
		right = vright;
	}
	if (e->scale != 1.0) {
		VectorScale (up, e->scale, up);
		VectorScale (right, e->scale, right);
	}

	VectorCopy (e->colormod, color);
	modelalpha = color[3] = e->colormod[3];
	if (modelalpha < 1.0)
		qfglDepthMask (GL_FALSE);

	qfglBindTexture (GL_TEXTURE_2D, frame->gl_texturenum);

	qfglBegin (GL_QUADS);

	qfglColor4fv (color);

	qfglTexCoord2f (0, 1);
	VectorMultAdd (e->origin, frame->down, up, point1);
	VectorMultAdd (point1, frame->left, right, point);
	qfglVertex3fv (point);

	qfglTexCoord2f (0, 0);
	VectorMultAdd (e->origin, frame->up, up, point2);
	VectorMultAdd (point2, frame->left, right, point);
	qfglVertex3fv (point);

	qfglTexCoord2f (1, 0);
	VectorMultAdd (point2, frame->right, right, point);
	qfglVertex3fv (point);

	qfglTexCoord2f (1, 1);
	VectorMultAdd (point1, frame->right, right, point);
	qfglVertex3fv (point);

	qfglEnd ();

	if (modelalpha < 1.0)
		qfglDepthMask (GL_TRUE);
}

static void
R_DrawSpriteModel_VA_f (entity_t *e)
{
	unsigned char	 modelalpha, color[4];
	float			*up, *right;
	int				 i;
//	unsigned int	 vacount;
	msprite_t		*psprite;
	mspriteframe_t	*frame;
	vec3_t			 point1, point2, v_up;
	varray_t2f_c4ub_v3f_t		*VA;

	VA = gl_spriteVertexArray; // FIXME: Despair

	// don't bother culling, it's just a single polygon without a surface cache
	frame = R_GetSpriteFrame (e);
	psprite = e->model->cache.data;

	qfglBindTexture (GL_TEXTURE_2D, frame->gl_texturenum); // FIXME: DESPAIR

	if (psprite->type == SPR_ORIENTED) {	// bullet marks on walls
		up = e->transform + 2 * 4;
		right = e->transform + 1 * 4;
	} else if (psprite->type == SPR_VP_PARALLEL_UPRIGHT) {
		v_up[0] = 0;
		v_up[1] = 0;
		v_up[2] = 1;
		up = v_up;
		right = vright;
	} else {								// normal sprite
		up = vup;
		right = vright;
	}
	if (e->scale != 1.0) {
		VectorScale (up, e->scale, up);
		VectorScale (right, e->scale, right);
	}

	for (i = 0; i < 4; i++)
		color[i] = e->colormod[i] * 255;
	memcpy (VA[0].color, color, 4);

	modelalpha = color[3];
	if (modelalpha < 255)
		qfglDepthMask (GL_FALSE);

	VectorMultAdd (e->origin, frame->down, up, point1);
	VectorMultAdd (point1, frame->left, right, VA[0].vertex);

	memcpy (VA[1].color, color, 4);
	VectorMultAdd (e->origin, frame->up, up, point2);
	VectorMultAdd (point2, frame->left, right, VA[1].vertex);

	memcpy (VA[2].color, color, 4);
	VectorMultAdd (point2, frame->right, right, VA[2].vertex);

	memcpy (VA[3].color, color, 4);
	VectorMultAdd (point1, frame->right, right, VA[3].vertex);

//	VA += 4;
//	vacount += 4;
//	if (vacount + 4 > sVAsize) {
//		qfglDrawElements (GL_QUADS, vacount, GL_UNSIGNED_INT, sVAindices);
		qfglDrawElements (GL_QUADS, 4, GL_UNSIGNED_INT, sVAindices);
//		vacount = 0;
//		VA = gl_spriteVertexArray;
//	}

	if (modelalpha < 255)
		qfglDepthMask (GL_TRUE);
}

void
gl_R_InitSprites (void)
{
	int		i;

	if (r_init) {
		if (gl_va_capable) {			// 0 == gl_va_capable
			gl_R_DrawSpriteModel = R_DrawSpriteModel_VA_f;

#if 0
			if (vaelements > 3)
				sVAsize = min (vaelements - (vaelements % 4), 512);
			else
				sVAsize = 512;
#else
			sVAsize = 4;
#endif
			Sys_MaskPrintf (SYS_DEV, "Sprites: %i maximum vertex elements.\n",
							sVAsize);

			if (gl_spriteVertexArray)
				free (gl_spriteVertexArray);
			gl_spriteVertexArray = (varray_t2f_c4ub_v3f_t *)
				calloc (sVAsize, sizeof (varray_t2f_c4ub_v3f_t));
			qfglInterleavedArrays (GL_T2F_C4UB_V3F, 0, gl_spriteVertexArray);

			if (sVAindices)
				free (sVAindices);
			sVAindices = (int *) calloc (sVAsize, sizeof (int));
			for (i = 0; i < sVAsize; i++)
				sVAindices[i] = i;
			for (i = 0; i < sVAsize / 4; i++) {
				gl_spriteVertexArray[i * 4].texcoord[0] = 0.0;
				gl_spriteVertexArray[i * 4].texcoord[1] = 1.0;
				gl_spriteVertexArray[i * 4 + 1].texcoord[0] = 0.0;
				gl_spriteVertexArray[i * 4 + 1].texcoord[1] = 0.0;
				gl_spriteVertexArray[i * 4 + 2].texcoord[0] = 1.0;
				gl_spriteVertexArray[i * 4 + 2].texcoord[1] = 0.0;
				gl_spriteVertexArray[i * 4 + 3].texcoord[0] = 1.0;
				gl_spriteVertexArray[i * 4 + 3].texcoord[1] = 1.0;
			}
		} else {
			gl_R_DrawSpriteModel = R_DrawSpriteModel_f;

			if (gl_spriteVertexArray) {
				free (gl_spriteVertexArray);
				gl_spriteVertexArray = 0;
			}
			if (sVAindices) {
				free (sVAindices);
				sVAindices = 0;
			}
		}
	}
}
