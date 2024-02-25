/*
	r_sprite.c

	Draw Sprite Model

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

#include "QF/sys.h"
#include "QF/scene/entity.h"

#include "r_internal.h"

mspriteframe_t *
R_GetSpriteFrame (const msprite_t *sprite, const animation_t *animation)
{
	int         framenum = animation->frame;
	float       syncbase = animation->syncbase;
	uint32_t data = R_GetFrameData (&sprite->skin, framenum, sprite, syncbase);
	return (mspriteframe_t *) ((byte *) sprite + data);
}
