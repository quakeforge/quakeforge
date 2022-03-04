/*
	world.h

	Client world scene management

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/3/4

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

#ifndef __client_world_h
#define __client_world_h

#include "QF/darray.h"
#include "QF/msg.h"
#include "QF/qtypes.h"

#include "QF/simd/types.h"

typedef struct modelset_s DARRAY_TYPE (struct model_s *) modelset_t;

typedef struct worldscene_s {
	struct scene_s *scene;
	struct plitem_s *edicts;
	struct plitem_s *worldspawn;
	struct model_s *worldmodel;
	modelset_t  models;
} worldscene_t;

extern worldscene_t cl_world;

struct msg_s;
struct entity_state_s;

void CL_World_Init (void);

// PROTOCOL_FITZQUAKE -- flags for entity baseline messages
#define B_LARGEMODEL	(1<<0)	// modelindex is short instead of byte
#define B_LARGEFRAME	(1<<1)	// frame is short instead of byte
#define B_ALPHA			(1<<2)	// 1 byte, uses ENTALPHA_ENCODE, not sent if ENTALPHA_DEFAULT
void CL_ParseBaseline (struct msg_s *msg, struct entity_state_s *baseline,
					   int version);
/*
	Static entities are non-interactive world objects like torches
*/
void CL_ParseStatic (struct msg_s *msg, int version);
void CL_MapCfg (const char *mapname);
void CL_World_NewMap (const char *mapname, const char *skyname);

#endif//__client_world_h
