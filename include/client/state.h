/*
	state.h

	client state

	Copyright (C) 2013 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2013/01/31

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

#ifndef __client_state_h
#define __client_state_h

typedef struct player_info_s {
	int         userid;
	struct info_s *userinfo;

	// scoreboard information
	struct info_key_s *name;
	struct info_key_s *team;
	struct info_key_s *chat;
	float       entertime;
	int         frags;
	int         ping;
	byte        pl;

	// skin information
	int         topcolor;
	int         bottomcolor;
	struct info_key_s *skinname;
	struct skin_s *skin;

	struct entity_s *flag_ent;

	int         spectator;
	int         stats[MAX_CL_STATS];	// health, etc
	int         prevcount;
} player_info_t;

#endif//__client_state_h
