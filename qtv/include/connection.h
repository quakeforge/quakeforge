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

#ifndef __connection_h
#define __connection_h

#include "netchan.h"

typedef struct connection_s {
	netadr_t    address;
	void       *object;
	void      (*handler) (void *obj);
} connection_t;

void Connection_Init (void);
void Connection_Add (netadr_t *address, void *object, void (*handler)(void*));
void Connection_Del (netadr_t *address);
connection_t *Connection_Find (netadr_t *address);

#endif//__connection_h
