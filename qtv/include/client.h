/*
	client.h

	quakeworld client processing

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

#ifndef __client_h
#define __client_h

#include "netchan.h"
#include "qw/msg_backbuf.h"

#include "server.h"

typedef struct client_s {
	// list of clients connected to a server
	struct client_s *next;
	struct client_s **prev;
	// list of clients in general
	struct client_s *clnext;
	struct info_s *userinfo;
	struct connection_s *con;
	const char *name;
	int         drop;
	netchan_t   netchan;
	backbuf_t   backbuf;
	sizebuf_t   datagram;
	byte        datagram_buf[MAX_DATAGRAM];
	bool        send_message;
	frame_t     frames[UPDATE_BACKUP];
	entity_state_t packet_entities[UPDATE_BACKUP][MAX_PACKET_ENTITIES];

	int         delta_sequence;

	struct server_s *server;
	unsigned    spec_track;

	int         connected;

	plent_state_t state;
	usercmd_t   lastcmd;
} client_t;

typedef struct challenge_s {
	int         challenge;
	double      time;
} challenge_t;

void Client_Init (void);
void Client_NewConnection (void);
void Client_SendMessages (client_t *cl);
void Client_New (client_t *cl);
void Client_Frame (void);

extern int client_count;

#endif//__client_h
