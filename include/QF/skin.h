/*
	skin.h

	Client skin definitions

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

#ifndef _SKIN_H
#define _SKIN_H

#include "QF/qtypes.h"
#include "QF/zone.h"

#define MAX_CACHED_SKINS 128
#define MAX_SKIN_LENGTH	32

#define RSSHOT_WIDTH 320
#define RSSHOT_HEIGHT 200

typedef struct skin_s
{
	char		name[MAX_SKIN_LENGTH];
	qboolean	failedload;		// the name isn't a valid skin
	union {
		cache_user_t	cache;
		struct tex_s	*texels;
	} data;
	int         texture;
	int         fb_texture;
	int         numfb;
} skin_t;

extern byte player_8bit_texels[640 * 400];
extern skin_t   skin_cache[MAX_CACHED_SKINS];
extern int skin_textures;
extern int skin_fb_textures;

struct tex_s;
struct tex_s *Skin_Cache (skin_t *skin);
struct player_info_s;
struct model_s;

void	Skin_Find (struct player_info_s *sc);
void	Skin_Flush (void);
int		Skin_Init_Textures (int base);
void	Skin_Init (void);
void	Skin_Init_Cvars (void);
void	Skin_Init_Translation (void);
void	Skin_Set_Translate (int top, int bottom, void *_dest);
void	Skin_Do_Translation (skin_t *player_skin, int slot, skin_t *skin);
void	Skin_Do_Translation_Model (struct model_s *model, int skinnum,
								   int slot, skin_t *skin);
void	Skin_Player_Model (struct model_s *model);
void	Skin_Process (skin_t *skin, struct tex_s *);

skin_t	*Skin_NewTempSkin (void);
void	Skin_ClearTempSkins (void);

int		Skin_FbPercent (const char *skin_name); // * 10

extern char allskins[128];

#endif
