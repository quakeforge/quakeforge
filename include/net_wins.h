/*
	net_wins.h

	@description@

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

#ifndef __net_wins_h
#define __net_wind_h

#include "winquake.h"

#include "QF/qtypes.h"

/** \defgroup nq-wins NetQuake Winsock lan driver.
	\ingroup nq-ld
*/
///@{

extern int winsock_initialized;
extern WSADATA winsockdata;

int  WINS_Init (void);
void WINS_Shutdown (void);
void WINS_Listen (bool state);
int  WINS_OpenSocket (int port);
int  WINS_CloseSocket (int socket);
int  WINS_Connect (int socket, netadr_t *addr);
int  WINS_CheckNewConnections (void);
int  WINS_Read (int socket, byte *buf, int len, netadr_t *addr);
int  WINS_Write (int socket, byte *buf, int len, netadr_t *addr);
int  WINS_Broadcast (int socket, byte *buf, int len);
const char *WINS_AddrToString (netadr_t *addr);
int  WINS_GetSocketAddr (int socket, netadr_t *addr);
int  WINS_GetNameFromAddr (netadr_t *addr, char *name);
int  WINS_GetAddrFromName (const char *name, netadr_t *addr);
int  WINS_AddrCompare (netadr_t *addr1, netadr_t *addr2) __attribute__((pure));
int  WINS_GetSocketPort (netadr_t *addr);
int  WINS_SetSocketPort (netadr_t *addr, int port);

///@}

#endif//__net_wins_h
