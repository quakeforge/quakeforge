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

#ifndef __client_h
#define __client_h

#include "netchan.h"
#include "qw/msg_backbuf.h"

typedef struct client_s {
	struct info_s *userinfo;
	struct connection_s *con;
	int         drop;
	netchan_t   netchan;
	backbuf_t   backbuf;
} client_t;

typedef struct challenge_s {
	int         challenge;
	double      time;
} challenge_t;

void Client_Init (void);
void Client_NewConnection (void);

#endif//__client_h
