/*
	net_vcr.c

	VCR network driver.

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

#include "QF/msg.h"
#include "QF/sys.h"

#include "netmain.h"
#include "net_vcr.h"

#include "../nq/include/server.h"


// This is the playback portion of the VCR.  It reads the file produced
// by the recorder and plays it back to the host.  The recording contains
// everything necessary (events, timestamps, and data) to duplicate the game
// from the viewpoint of everything above the network layer.

static struct {
	double      time;
	int         op;
	long        session;
} next;

int
VCR_Init (void)
{
	net_drivers[0].Init = VCR_Init;

	net_drivers[0].SearchForHosts = VCR_SearchForHosts;
	net_drivers[0].Connect = VCR_Connect;
	net_drivers[0].CheckNewConnections = VCR_CheckNewConnections;
	net_drivers[0].QGetMessage = VCR_GetMessage;
	net_drivers[0].QSendMessage = VCR_SendMessage;
	net_drivers[0].CanSendMessage = VCR_CanSendMessage;
	net_drivers[0].Close = VCR_Close;
	net_drivers[0].Shutdown = VCR_Shutdown;

	Qread (vcrFile, &next, sizeof (next));
	return 0;
}

static void
VCR_ReadNext (void)
{
	if (Qread (vcrFile, &next, sizeof (next)) == 0) {
		next.op = 255;
		Sys_Error ("=== END OF PLAYBACK===");
	}
	if (next.op < 1 || next.op > VCR_MAX_MESSAGE)
		Sys_Error ("VCR_ReadNext: bad op");
}


void
VCR_Listen (bool state)
{
}


void
VCR_Shutdown (void)
{
}


int
VCR_GetMessage (qsocket_t * sock)
{
	int         ret;
	long       *driverdata = (long *) (char *) &sock->driverdata;

	if (host_time != next.time || next.op != VCR_OP_GETMESSAGE
		|| next.session != *driverdata)
		Sys_Error ("VCR missmatch");

	Qread (vcrFile, &ret, sizeof (int));

	if (ret != 1) {
		VCR_ReadNext ();
		return ret;
	}

	Qread (vcrFile, &net_message->message->cursize, sizeof (int));

	Qread (vcrFile, net_message->message->data,
				  net_message->message->cursize);

	VCR_ReadNext ();

	return 1;
}


int
VCR_SendMessage (qsocket_t * sock, sizebuf_t *data)
{
	int         ret;
	long       *driverdata = (long *) (char *) &sock->driverdata;

	if (host_time != next.time || next.op != VCR_OP_SENDMESSAGE
		|| next.session != *driverdata)
		Sys_Error ("VCR missmatch");

	Qread (vcrFile, &ret, sizeof (int));

	VCR_ReadNext ();

	return ret;
}


bool
VCR_CanSendMessage (qsocket_t * sock)
{
	bool        ret;
	long       *driverdata = (long *) (char *) &sock->driverdata;

	if (host_time != next.time || next.op != VCR_OP_CANSENDMESSAGE
		|| next.session != *driverdata)
		Sys_Error ("VCR missmatch");

	Qread (vcrFile, &ret, sizeof (int));

	VCR_ReadNext ();

	return ret;
}


void
VCR_Close (qsocket_t * sock)
{
}


void
VCR_SearchForHosts (bool xmit)
{
}


__attribute__((const)) qsocket_t *
VCR_Connect (const char *host)
{
	return NULL;
}


qsocket_t *
VCR_CheckNewConnections (void)
{
	qsocket_t  *sock;
	long       *driverdata;

	if (host_time != next.time || next.op != VCR_OP_CONNECT)
		Sys_Error ("VCR missmatch");

	if (!next.session) {
		VCR_ReadNext ();
		return NULL;
	}

	sock = NET_NewQSocket ();
	driverdata = (long *) (char *) &sock->driverdata;
	*driverdata = next.session;

	Qread (vcrFile, sock->address, NET_NAMELEN);
	VCR_ReadNext ();

	return sock;
}
