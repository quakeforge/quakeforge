/*
	net_loop.c

	Loopback network driver.

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

#include "QF/cvar.h"
#include "QF/msg.h"
#include "QF/sys.h"

#include "netmain.h"
#include "net_loop.h"

#include "../nq/include/server.h"

bool        localconnectpending = false;
qsocket_t  *loop_client = NULL;
qsocket_t  *loop_server = NULL;

__attribute__((pure)) int
Loop_Init (void)
{
	qfZoneScoped (true);
	if (net_is_dedicated)
		return -1;
	return 0;
}


void
Loop_Shutdown (void)
{
}


void
Loop_Listen (bool state)
{
}


void
Loop_SearchForHosts (bool xmit)
{
	if (!sv.active)
		return;

	const char *name = "local";
	if (strcmp (hostname, "UNNAMED") != 0) {
		name = hostname;
	}
	const char *map = sv.name;
	int         users = net_activeconnections;
	int         maxusers = svs.maxclients;
	const char *cname = "local";
	NET_AddCachedHost (name, map, cname, users, maxusers, net_driverlevel,
					   0, 0);
}


qsocket_t  *
Loop_Connect (const char *host)
{
	if (strcmp (host, "local") != 0)
		return NULL;

	localconnectpending = true;

	if (!loop_client) {
		if ((loop_client = NET_NewQSocket ()) == NULL) {
			Sys_Printf ("Loop_Connect: no qsocket available\n");
			return NULL;
		}
		strcpy (loop_client->address, "localhost");
	}
	loop_client->receiveMessageLength = 0;
	loop_client->sendMessageLength = 0;
	loop_client->canSend = true;

	if (!loop_server) {
		if ((loop_server = NET_NewQSocket ()) == NULL) {
			Sys_Printf ("Loop_Connect: no qsocket available\n");
			return NULL;
		}
		strcpy (loop_server->address, "LOCAL");
	}
	loop_server->receiveMessageLength = 0;
	loop_server->sendMessageLength = 0;
	loop_server->canSend = true;

	loop_client->driverdata = (void *) loop_server;
	loop_server->driverdata = (void *) loop_client;

	return loop_client;
}


qsocket_t  *
Loop_CheckNewConnections (void)
{
	if (!localconnectpending)
		return NULL;

	localconnectpending = false;
	loop_server->sendMessageLength = 0;
	loop_server->receiveMessageLength = 0;
	loop_server->canSend = true;
	loop_client->sendMessageLength = 0;
	loop_client->receiveMessageLength = 0;
	loop_client->canSend = true;
	return loop_server;
}


static int
IntAlign (int value)
{
	return (value + (sizeof (int) - 1)) & (~(sizeof (int) - 1));
}


int
Loop_GetMessage (qsocket_t * sock)
{
	int         ret;
	int         length;

	if (sock->receiveMessageLength == 0)
		return 0;

	ret = sock->receiveMessage[0];
	length = sock->receiveMessage[1] + (sock->receiveMessage[2] << 8);
	// alignment byte skipped here
	SZ_Clear (net_message->message);
	SZ_Write (net_message->message, &sock->receiveMessage[4], length);

	length = IntAlign (length + 4);
	sock->receiveMessageLength -= length;

	if (sock->receiveMessageLength)
		memmove (sock->receiveMessage, &sock->receiveMessage[length],
				 sock->receiveMessageLength);

	if (sock->driverdata && ret == 1)
		((qsocket_t *) sock->driverdata)->canSend = true;

	return ret;
}


int
Loop_SendMessage (qsocket_t * sock, sizebuf_t *data)
{
	byte       *buffer;
	int        *bufferLength;

	if (!sock->driverdata)
		return -1;

	bufferLength = &((qsocket_t *) sock->driverdata)->receiveMessageLength;

	if ((*bufferLength + data->cursize + 4) > NET_MAXMESSAGE)
		Sys_Error ("Loop_SendMessage: overflow");

	buffer = ((qsocket_t *) sock->driverdata)->receiveMessage + *bufferLength;

	// message type
	*buffer++ = 1;

	// length
	*buffer++ = data->cursize & 0xff;
	*buffer++ = data->cursize >> 8;

	// align
	buffer++;

	// message
	memcpy (buffer, data->data, data->cursize);
	*bufferLength = IntAlign (*bufferLength + data->cursize + 4);

	sock->canSend = false;
	return 1;
}


int
Loop_SendUnreliableMessage (qsocket_t * sock, sizebuf_t *data)
{
	byte       *buffer;
	int        *bufferLength;

	if (!sock->driverdata)
		return -1;

	bufferLength = &((qsocket_t *) sock->driverdata)->receiveMessageLength;

	if ((*bufferLength + data->cursize + sizeof (byte) + sizeof (short)) >
		NET_MAXMESSAGE) return 0;

	buffer = ((qsocket_t *) sock->driverdata)->receiveMessage + *bufferLength;

	// message type
	*buffer++ = 2;

	// length
	*buffer++ = data->cursize & 0xff;
	*buffer++ = data->cursize >> 8;

	// align
	buffer++;

	// message
	memcpy (buffer, data->data, data->cursize);
	*bufferLength = IntAlign (*bufferLength + data->cursize + 4);
	return 1;
}


__attribute__((pure)) bool
Loop_CanSendMessage (qsocket_t * sock)
{
	if (!sock->driverdata)
		return false;
	return sock->canSend;
}


__attribute__((const)) bool
Loop_CanSendUnreliableMessage (qsocket_t * sock)
{
	return true;
}


void
Loop_Close (qsocket_t * sock)
{
	if (sock->driverdata)
		((qsocket_t *) sock->driverdata)->driverdata = NULL;
	sock->receiveMessageLength = 0;
	sock->sendMessageLength = 0;
	sock->canSend = true;
	if (sock == loop_client)
		loop_client = NULL;
	else
		loop_server = NULL;
}
