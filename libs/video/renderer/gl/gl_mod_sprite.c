/*
	gl_mod_sprite.c

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

#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"

#include "QF/console.h"
#include "QF/model.h"
#include "QF/render.h"

#include "r_shared.h"

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

// FIXME: add modelalpha support?
void
R_DrawSpriteModel (entity_t *e)
{
	float			*up, *right;
	msprite_t		*psprite;
	mspriteframe_t	*frame;
	vec3_t			 point, v_forward, v_right, v_up;

	// don't bother culling, it's just a single polygon without a surface cache
	frame = R_GetSpriteFrame (e);
	psprite = e->model->cache.data;

	if (psprite->type == SPR_ORIENTED) {	// bullet marks on walls
		AngleVectors (e->angles, v_forward, v_right, v_up);
		up = v_up;
		right = v_right;
	} else {								// normal sprite
		up = vup;
		right = vright;
	}

	qfglBindTexture (GL_TEXTURE_2D, frame->gl_texturenum);

	qfglBegin (GL_QUADS);

	qfglTexCoord2f (0, 1);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->left, right, point);
	qfglVertex3fv (point);

	qfglTexCoord2f (0, 0);
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->left, right, point);
	qfglVertex3fv (point);

	qfglTexCoord2f (1, 0);
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->right, right, point);
	qfglVertex3fv (point);

	qfglTexCoord2f (1, 1);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->right, right, point);
	qfglVertex3fv (point);

	qfglEnd ();
}
