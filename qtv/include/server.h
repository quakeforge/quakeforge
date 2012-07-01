/*
	server.h

	quakeworld server processing

	Copyright (C) 2004 Bill Currie <bill@taniwha.org>

	Author: Bill Currie
	Date: 2004/02/20

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

#ifndef __server_h
#define __server_h

#include "netchan.h"
#include "qw/bothdefs.h"
#include "qw/pmove.h"

struct client_s;

typedef struct player_s {
	struct player_s *next;				// for multicast messages
	struct info_s *info;
	int         stats[MAX_CL_STATS];
	int         uid;
	int         ping;
	int         pl;
	int         frags;
	int         time;

	plent_state_t ent;
} player_t;

typedef struct frame_s {
	int         delta_sequence;
	qboolean    invalid;
	packet_players_t players;
	packet_entities_t entities;
} frame_t;

typedef struct qtv_leaf_s {
	struct qtv_leaf_s *next;
	int         num;
} qtv_leaf_t;

typedef struct {
	entity_state_t e;
	qtv_leaf_t *leafs;
} qtv_entity_t;

#define MAX_SV_PLAYERS 32
#define MAX_SV_ENTITIES 512
#define MAX_SIGNON_BUFFERS  8

typedef struct server_s {
	struct server_s *next;
	const char *name;
	const char *address;
	int         qport;
	int         connected;
	int         signon;
	struct connection_s *con;
	netadr_t    adr;
	netchan_t   netchan;
	double      next_run;

	int         ver;
	int         spawncount;
	const char *gamedir;
	const char *message;
	movevars_t  movevars;
	int         cdtrack;
	int         sounds;
	struct info_s *info;
	char       *soundlist[MAX_SOUNDS + 1];
	char       *modellist[MAX_MODELS + 1];
	char       *lightstyles[MAX_LIGHTSTYLES];
	struct model_s *worldmodel;
	int         playermodel;
	int         num_signon_buffers;
	int         signon_buffer_size[MAX_SIGNON_BUFFERS];
	byte        signon_buffers[MAX_SIGNON_BUFFERS][MAX_DATAGRAM];

	struct client_s *clients;

	int         delta;
	int         validsequence;

	frame_t     frames[UPDATE_BACKUP];
	qtv_entity_t  entities[MAX_SV_ENTITIES];
	byte        ent_valid[MAX_SV_ENTITIES];
	entity_state_t baselines[MAX_SV_ENTITIES];
	player_t    players[MAX_SV_PLAYERS];
	player_t   *player_list;			// list of players for multicast
	unsigned    player_mask;			// which players are in the list
	plent_state_t player_states[UPDATE_BACKUP][MAX_SV_PLAYERS];
	entity_state_t entity_states[UPDATE_BACKUP][MAX_DEMO_PACKET_ENTITIES];
} server_t;

void Server_Init (void);
void Server_Frame (void);
void Server_List (void);
void Server_Connect (const char *name, struct client_s *client);
void Server_Disconnect (struct client_s *client);
void Server_Broadcast (server_t *sv, int reliable, int all, const byte *msg,
					   int len);
void Server_BroadcastCommand (server_t *sv, const char *cmd);

struct qmsg_s;
void sv_parse (server_t *sv, struct msg_s *msg, int reliable);
void sv_stringcmd (server_t *sv, struct msg_s *msg);

extern int server_count;

#endif//__server_h
