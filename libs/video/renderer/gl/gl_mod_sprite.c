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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/model.h"
#include "QF/render.h"
#include "QF/sys.h"

#include "QF/scene/entity.h"

#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"
#include "QF/GL/qf_sprite.h"

#include "compat.h"
#include "r_internal.h"
#include "varrays.h"

static int						 sVAsize;
static int						*sVAindices;
varray_t2f_c4ub_v3f_t	*gl_spriteVertexArray;

void (*gl_R_DrawSpriteModel) (struct entity_s ent);

static void
R_DrawSpriteModel_f (entity_t e)
{
	auto renderer = Entity_GetRenderer (e);
	msprite_t		*sprite = renderer->model->cache.data;
	float			 modelalpha, color[4];
	vec4f_t          cameravec = {};
	vec4f_t          up = {}, right = {}, pn = {};
	vec4f_t          origin, point;

	transform_t transform = Entity_Transform (e);
	origin = Transform_GetWorldPosition (transform);
	cameravec = r_refdef.frame.position - origin;

	// don't bother culling, it's just a single polygon without a surface cache
	auto animation = Entity_GetAnimation (e);
	auto frame = (mspriteframe_t *) ((byte *) sprite + animation->pose2);

	if (!R_BillboardFrame (transform, sprite->type, cameravec,
						   &up, &right, &pn)) {
		// the orientation is undefined so can't draw the sprite
		return;
	}

	VectorCopy (renderer->colormod, color);
	modelalpha = color[3] = renderer->colormod[3];
	if (modelalpha < 1.0)
		qfglDepthMask (GL_FALSE);

	auto texnum = (GLuint *) &frame[1];
	qfglBindTexture (GL_TEXTURE_2D, *texnum);

	qfglBegin (GL_QUADS);

	qfglColor4fv (color);

	point = origin + frame->down * up + frame->left * right;

	qfglTexCoord2f (0, 1);
	qfglVertex3fv ((vec_t*)&point);//FIXME

	point = origin + frame->up * up + frame->left * right;
	qfglTexCoord2f (0, 0);
	qfglVertex3fv ((vec_t*)&point);//FIXME

	point = origin + frame->up * up + frame->right * right;

	qfglTexCoord2f (1, 0);
	qfglVertex3fv ((vec_t*)&point);//FIXME

	point = origin + frame->down * up + frame->right * right;
	qfglTexCoord2f (1, 1);
	qfglVertex3fv ((vec_t*)&point);//FIXME

	qfglEnd ();

	if (modelalpha < 1.0)
		qfglDepthMask (GL_TRUE);
}

static void
R_DrawSpriteModel_VA_f (entity_t e)
{
	auto renderer = Entity_GetRenderer (e);
	msprite_t		*sprite = renderer->model->cache.data;
	unsigned char	 modelalpha, color[4];
	vec4f_t          up = {}, right = {};
	vec4f_t          origin, point;
	int				 i;
//	unsigned int	 vacount;
	varray_t2f_c4ub_v3f_t		*VA;

	VA = gl_spriteVertexArray; // FIXME: Despair

	// don't bother culling, it's just a single polygon without a surface cache
	auto animation = Entity_GetAnimation (e);
	auto frame = (mspriteframe_t *) ((byte *) sprite + animation->pose2);

	auto texnum = (GLuint *) &frame[1];
	qfglBindTexture (GL_TEXTURE_2D, *texnum); // FIXME: DESPAIR

	transform_t transform = Entity_Transform (e);
	if (sprite->type == SPR_ORIENTED) {	// bullet marks on walls
		up = Transform_Up (transform);
		right = Transform_Right (transform);
	} else if (sprite->type == SPR_VP_PARALLEL_UPRIGHT) {
		up = (vec4f_t) { 0, 0, 1, 0 };
		VectorCopy (r_refdef.frame.right, right);
	} else {								// normal sprite
		VectorCopy (r_refdef.frame.up, up);
		VectorCopy (r_refdef.frame.right, right);
	}

	for (i = 0; i < 4; i++) {
		color[i] = renderer->colormod[i] * 255;
	}
	memcpy (VA[0].color, color, 4);
	memcpy (VA[1].color, color, 4);
	memcpy (VA[2].color, color, 4);
	memcpy (VA[3].color, color, 4);

	modelalpha = color[3];
	if (modelalpha < 255)
		qfglDepthMask (GL_FALSE);

	origin = Transform_GetWorldPosition (transform);

	point = origin + frame->down * up + frame->left * right;
	VectorCopy (point, VA[0].vertex);

	point = origin + frame->up * up + frame->left * right;
	VectorCopy (point, VA[1].vertex);

	point = origin + frame->up * up + frame->right * right;
	VectorCopy (point, VA[2].vertex);

	point = origin + frame->down * up + frame->right * right;
	VectorCopy (point, VA[3].vertex);

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
		if (gl_va_capable) {
			gl_R_DrawSpriteModel = R_DrawSpriteModel_VA_f;

#if 0
			if (vaelements > 3)
				sVAsize = min (vaelements - (vaelements % 4), 512);
			else
				sVAsize = 512;
#else
			sVAsize = 4;
#endif
			Sys_MaskPrintf (SYS_dev, "Sprites: %i maximum vertex elements.\n",
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
