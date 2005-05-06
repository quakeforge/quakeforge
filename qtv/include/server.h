/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2004 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

#ifndef __server_h
#define __server_h

#include "netchan.h"
#include "qw/pmove.h"

typedef struct server_s {
	struct server_s *next;
	const char *name;
	const char *address;
	int         qport;
	int         connected;
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

	int         delta;
} server_t;

void Server_Init (void);
void Server_Frame (void);

struct qmsg_s;
void sv_parse (server_t *sv, struct msg_s *msg, int reliable);
void sv_stringcmd (server_t *sv, struct msg_s *msg);

#endif//__server_h
