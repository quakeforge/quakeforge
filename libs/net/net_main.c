/*
	net_main.c

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/msg.h"
#include "QF/qargs.h"
#include "QF/quakeio.h"
#include "QF/sizebuf.h"
#include "QF/sys.h"

#include "netmain.h"
#include "net_vcr.h"

#include "../nq/include/server.h"

int net_is_dedicated = 0;

qsocket_t  *net_activeSockets = NULL;
qsocket_t  *net_freeSockets = NULL;
int         net_numsockets = 0;

bool        tcpipAvailable = false;

int         net_hostport;
int         DEFAULTnet_hostport = 26000;

char        my_tcpip_address[NET_NAMELEN];

static bool listening = false;

bool        slistInProgress = false;
bool        slistSilent = false;
bool        slistLocal = true;
static double slistStartTime;
static int  slistLastShown;

static void Slist_Send (void *);
static void Slist_Poll (void *);
PollProcedure slistSendProcedure = { NULL, 0.0, Slist_Send };
PollProcedure slistPollProcedure = { NULL, 0.0, Slist_Poll };


static sizebuf_t _net_message_message;
static qmsg_t _net_message = { 0, 0, &_net_message_message };
qmsg_t     *net_message = &_net_message;
unsigned    net_activeconnections = 0;

int         messagesSent = 0;
int         messagesReceived = 0;
int         unreliableMessagesSent = 0;
int         unreliableMessagesReceived = 0;

float net_messagetimeout;
static cvar_t net_messagetimeout_cvar = {
	.name = "net_messagetimeout",
	.description =
		"None",
	.default_value = "300",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &net_messagetimeout },
};
char *hostname;
static cvar_t hostname_cvar = {
	.name = "hostname",
	.description =
		"None",
	.default_value = "UNNAMED",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &hostname },
};

QFile      *vcrFile;
bool        recording = false;

// these two macros are to make the code more readable
#define sfunc	net_drivers[sock->driver]
#define dfunc	net_drivers[net_driverlevel]

int         net_driverlevel;

double      net_time;

static int         hostCacheCount = 0;
static hostcache_t hostcache[HOSTCACHESIZE];

static cbuf_t *net_cbuf;

double
SetNetTime (void)
{
	net_time = Sys_DoubleTime ();
	return net_time;
}


qsocket_t *
NET_NewQSocket (void)
{
	qsocket_t  *sock;

	if (net_freeSockets == NULL)
		return NULL;

	if (net_activeconnections >= svs.maxclients)
		return NULL;

	// get one from free list
	sock = net_freeSockets;
	net_freeSockets = sock->next;

	// add it to active list
	sock->next = net_activeSockets;
	net_activeSockets = sock;

	sock->disconnected = false;
	sock->connecttime = net_time;
	strcpy (sock->address, "UNSET ADDRESS");
	sock->driver = net_driverlevel;
	sock->socket = 0;
	sock->driverdata = NULL;
	sock->canSend = true;
	sock->sendNext = false;
	sock->lastMessageTime = net_time;
	sock->ackSequence = 0;
	sock->sendSequence = 0;
	sock->unreliableSendSequence = 0;
	sock->sendMessageLength = 0;
	sock->receiveSequence = 0;
	sock->unreliableReceiveSequence = 0;
	sock->receiveMessageLength = 0;

	return sock;
}


void
NET_FreeQSocket (qsocket_t *sock)
{
	qsocket_t  *s;

	// remove it from active list
	if (sock == net_activeSockets)
		net_activeSockets = net_activeSockets->next;
	else {
		for (s = net_activeSockets; s; s = s->next)
			if (s->next == sock) {
				s->next = sock->next;
				break;
			}
		if (!s)
			Sys_Error ("NET_FreeQSocket: not active");
	}

	// add it to free list
	sock->next = net_freeSockets;
	net_freeSockets = sock;
	sock->disconnected = true;
}


static void
NET_Listen_f (void)
{
	if (Cmd_Argc () != 2) {
		Sys_Printf ("\"listen\" is \"%u\"\n", listening ? 1 : 0);
		return;
	}

	listening = atoi (Cmd_Argv (1)) ? true : false;

	for (net_driverlevel = 0; net_driverlevel < net_numdrivers;
		 net_driverlevel++) {
		if (net_drivers[net_driverlevel].initialized == false)
			continue;
		dfunc.Listen (listening);
	}
}


static void
MaxPlayers_f (void)
{
	unsigned    n;

	if (Cmd_Argc () != 2) {
		Sys_Printf ("\"maxplayers\" is \"%u\"\n", svs.maxclients);
		return;
	}

	if (sv.active) {
		Sys_Printf
			("maxplayers can not be changed while a server is running.\n");
		return;
	}

	n = atoi (Cmd_Argv (1));
	if (n < 1)
		n = 1;
	if (n > svs.maxclientslimit) {
		n = svs.maxclientslimit;
		Sys_Printf ("\"maxplayers\" set to \"%u\"\n", n);
	}

	if ((n == 1) && listening)
		Cbuf_AddText (net_cbuf, "listen 0\n");

	if ((n > 1) && (!listening))
		Cbuf_AddText (net_cbuf, "listen 1\n");

	svs.maxclients = n;
	if (n == 1)
		Cvar_Set ("deathmatch", "0");
	else
		Cvar_Set ("deathmatch", "1");
}


static void
NET_Port_f (void)
{
	int         n;

	if (Cmd_Argc () != 2) {
		Sys_Printf ("\"port\" is \"%u\"\n", net_hostport);
		return;
	}

	n = atoi (Cmd_Argv (1));
	if (n < 1 || n > 65534) {
		Sys_Printf ("Bad value, must be between 1 and 65534\n");
		return;
	}

	DEFAULTnet_hostport = n;
	net_hostport = n;

	if (listening) {
		// force a change to the new port
		Cbuf_AddText (net_cbuf, "listen 0\n");
		Cbuf_AddText (net_cbuf, "listen 1\n");
	}
}


static void
PrintSlistHeader (void)
{
	Sys_Printf ("Server          Map             Users\n");
	Sys_Printf ("--------------- --------------- -----\n");
	slistLastShown = 0;
}

void
NET_AddCachedHost (const char *name, const char *map, const char *cname,
				   int users, int maxusers, int driver, int ldriver,
				   const netadr_t *addr)
{
	if (hostCacheCount == HOSTCACHESIZE) {
		return;
	}
	for (int i = 0; i < hostCacheCount; i++) {
		// addr will be 0 for loopback, and there can be only one loopback
		// server.
		if (!addr || !memcmp (addr, &hostcache[i].addr, sizeof (netadr_t))) {
			return;
		}
	}

	const int   namesize = sizeof (hostcache[0].name) - 1;
	const int   mapsize = sizeof (hostcache[0].map) - 1;
	const int   cnamesize = sizeof (hostcache[0].cname) - 1;

	hostcache_t *host = &hostcache[hostCacheCount++];
	strncpy (host->name, name, namesize);
	strncpy (host->map, map, mapsize);
	strncpy (host->cname, cname, cnamesize);
	host->name[namesize] = 0;
	host->map[mapsize] = 0;
	host->cname[cnamesize] = 0;

	host->users = users;
	host->maxusers = maxusers;
	host->driver = driver;
	host->ldriver = ldriver;
	if (addr) {
		host->addr = *addr;
	} else {
		memset (&host->addr, 0, sizeof (host->addr));
	}

	// check for and resolve name conflicts
	for (int i = 0; i < hostCacheCount - 1; i++) {
		if (strcasecmp (host->name, hostcache[i].name) == 0) {
			int         len = strlen (host->name);
			if (len < namesize && host->name[len - 1] > '8') {
				host->name[len] = '0';
				host->name[len + 1] = 0;
			} else {
				host->name[len - 1]++;
			}
			i = -1;	// restart loop
		}
	}
}

static void
PrintSlist (void)
{
	int         n;

	for (n = slistLastShown; n < hostCacheCount; n++) {
		if (hostcache[n].maxusers)
			Sys_Printf ("%-15.15s %-15.15s %2u/%2u\n", hostcache[n].name,
						hostcache[n].map, hostcache[n].users,
						hostcache[n].maxusers);
		else
			Sys_Printf ("%-15.15s %-15.15s\n", hostcache[n].name,
						hostcache[n].map);
	}
	slistLastShown = n;
}


static void
PrintSlistTrailer (void)
{
	if (hostCacheCount)
		Sys_Printf ("== end list ==\n\n");
	else
		Sys_Printf ("No Quake servers found.\n\n");
}


static void
NET_Slist_f (void)
{
	if (slistInProgress)
		return;

	if (!slistSilent) {
		Sys_Printf ("Looking for Quake servers...\n");
		PrintSlistHeader ();
	}

	slistInProgress = true;
	slistStartTime = Sys_DoubleTime ();

	SchedulePollProcedure (&slistSendProcedure, 0.0);
	SchedulePollProcedure (&slistPollProcedure, 0.1);

	hostCacheCount = 0;
}


static void
Slist_Send (void *unused)
{
	for (net_driverlevel = 0; net_driverlevel < net_numdrivers;
		 net_driverlevel++) {
		if (!slistLocal && net_driverlevel == 0)
			continue;
		if (net_drivers[net_driverlevel].initialized == false)
			continue;
		dfunc.SearchForHosts (true);
	}

	if ((Sys_DoubleTime () - slistStartTime) < 0.5)
		SchedulePollProcedure (&slistSendProcedure, 0.75);
}


static void
Slist_Poll (void *unused)
{
	for (net_driverlevel = 0; net_driverlevel < net_numdrivers;
		 net_driverlevel++) {
		if (!slistLocal && net_driverlevel == 0)
			continue;
		if (net_drivers[net_driverlevel].initialized == false)
			continue;
		dfunc.SearchForHosts (false);
	}

	if (!slistSilent)
		PrintSlist ();

	if ((Sys_DoubleTime () - slistStartTime) < 1.5) {
		SchedulePollProcedure (&slistPollProcedure, 0.1);
		return;
	}

	if (!slistSilent)
		PrintSlistTrailer ();
	slistInProgress = false;
	slistSilent = false;
	slistLocal = true;
}

qsocket_t *
NET_Connect (const char *host)
{
	qsocket_t  *ret;
	int         n;
	int         numdrivers = net_numdrivers;

	SetNetTime ();

	if (host && *host == 0)
		host = NULL;

	if (host) {
		if (strcasecmp (host, "local") == 0) {
			numdrivers = 1;
			goto JustDoIt;
		}

		if (hostCacheCount) {
			for (n = 0; n < hostCacheCount; n++)
				if (strcasecmp (host, hostcache[n].name) == 0) {
					host = hostcache[n].cname;
					break;
				}
			if (n < hostCacheCount)
				goto JustDoIt;
		}
	}

	slistSilent = host ? true : false;
	NET_Slist_f ();

	while (slistInProgress)
		NET_Poll ();

	if (host == NULL) {
		if (hostCacheCount != 1)
			return NULL;
		host = hostcache[0].cname;
		Sys_Printf ("Connecting to...\n%s @ %s\n\n", hostcache[0].name, host);
	}

	if (hostCacheCount)
		for (n = 0; n < hostCacheCount; n++)
			if (strcasecmp (host, hostcache[n].name) == 0) {
				host = hostcache[n].cname;
				break;
			}

  JustDoIt:
	for (net_driverlevel = 0; net_driverlevel < numdrivers; net_driverlevel++) {
		if (net_drivers[net_driverlevel].initialized == false)
			continue;
		ret = dfunc.Connect (host);
		if (ret)
			return ret;
	}

	if (host) {
		Sys_Printf ("\n");
		PrintSlistHeader ();
		PrintSlist ();
		PrintSlistTrailer ();
	}

	return NULL;
}


struct {
	double      time;
	int         op;
	intptr_t    session;
} vcrConnect;

qsocket_t *
NET_CheckNewConnections (void)
{
	qsocket_t  *ret;

	SetNetTime ();

	for (net_driverlevel = 0; net_driverlevel < net_numdrivers;
		 net_driverlevel++) {
		if (net_drivers[net_driverlevel].initialized == false)
			continue;
		if (net_driverlevel && listening == false)
			continue;
		ret = dfunc.CheckNewConnections ();
		if (ret) {
			if (recording) {
				vcrConnect.time = host_time;
				vcrConnect.op = VCR_OP_CONNECT;
				vcrConnect.session = (intptr_t) ret;
				Qwrite (vcrFile, &vcrConnect, sizeof (vcrConnect));
				Qwrite (vcrFile, ret->address, NET_NAMELEN);
			}
			return ret;
		}
	}

	if (recording) {
		vcrConnect.time = host_time;
		vcrConnect.op = VCR_OP_CONNECT;
		vcrConnect.session = 0;
		Qwrite (vcrFile, &vcrConnect, sizeof (vcrConnect));
	}

	return NULL;
}

void
NET_Close (qsocket_t *sock)
{
	if (!sock)
		return;

	if (sock->disconnected)
		return;

	SetNetTime ();

	// call the driver_Close function
	sfunc.Close (sock);

	Sys_MaskPrintf (SYS_net, "closing socket\n");
	NET_FreeQSocket (sock);
}


struct {
	double      time;
	int         op;
	intptr_t    session;
	int         ret;
	int         len;
} vcrGetMessage;


int
NET_GetMessage (qsocket_t *sock)
{
	int         ret;

	if (!sock)
		return -1;

	if (sock->disconnected) {
		Sys_Printf ("NET_GetMessage: disconnected socket\n");
		return -1;
	}

	SetNetTime ();

	ret = sfunc.QGetMessage (sock);

	// see if this connection has timed out
	if (ret == 0 && sock->driver) {
		if (net_time - sock->lastMessageTime > net_messagetimeout) {
			Sys_MaskPrintf (SYS_net, "socket timed out\n");
			NET_Close (sock);
			return -1;
		}
	}


	if (ret > 0) {
		if (sock->driver) {
			sock->lastMessageTime = net_time;
			if (ret == 1)
				messagesReceived++;
			else if (ret == 2)
				unreliableMessagesReceived++;
		}

		if (recording) {
			vcrGetMessage.time = host_time;
			vcrGetMessage.op = VCR_OP_GETMESSAGE;
			vcrGetMessage.session = (intptr_t) sock;
			vcrGetMessage.ret = ret;
			vcrGetMessage.len = _net_message_message.cursize;
			Qwrite (vcrFile, &vcrGetMessage, 24);
			Qwrite (vcrFile, _net_message_message.data,
						   _net_message_message.cursize);
		}
	} else {
		if (recording) {
			vcrGetMessage.time = host_time;
			vcrGetMessage.op = VCR_OP_GETMESSAGE;
			vcrGetMessage.session = (intptr_t) sock;
			vcrGetMessage.ret = ret;
			Qwrite (vcrFile, &vcrGetMessage, 20);
		}
	}

	return ret;
}


struct {
	double      time;
	int         op;
	intptr_t    session;
	int         r;
} vcrSendMessage;

int
NET_SendMessage (qsocket_t *sock, sizebuf_t *data)
{
	int         r;

	if (!sock)
		return -1;

	if (sock->disconnected) {
		Sys_Printf ("NET_SendMessage: disconnected socket\n");
		return -1;
	}

	SetNetTime ();
	r = sfunc.QSendMessage (sock, data);
	if (r == 1 && sock->driver)
		messagesSent++;

	if (recording) {
		vcrSendMessage.time = host_time;
		vcrSendMessage.op = VCR_OP_SENDMESSAGE;
		vcrSendMessage.session = (intptr_t) sock;
		vcrSendMessage.r = r;
		Qwrite (vcrFile, &vcrSendMessage, 20);
	}

	return r;
}


int
NET_SendUnreliableMessage (qsocket_t *sock, sizebuf_t *data)
{
	int         r;

	if (!sock)
		return -1;

	if (sock->disconnected) {
		Sys_Printf ("NET_SendMessage: disconnected socket\n");
		return -1;
	}

	SetNetTime ();
	r = sfunc.SendUnreliableMessage (sock, data);
	if (r == 1 && sock->driver)
		unreliableMessagesSent++;

	if (recording) {
		vcrSendMessage.time = host_time;
		vcrSendMessage.op = VCR_OP_SENDMESSAGE;
		vcrSendMessage.session = (intptr_t) sock;
		vcrSendMessage.r = r;
		Qwrite (vcrFile, &vcrSendMessage, 20);
	}

	return r;
}


bool
NET_CanSendMessage (qsocket_t *sock)
{
	int         r;

	if (!sock)
		return false;

	if (sock->disconnected)
		return false;

	SetNetTime ();

	r = sfunc.CanSendMessage (sock);

	if (recording) {
		vcrSendMessage.time = host_time;
		vcrSendMessage.op = VCR_OP_CANSENDMESSAGE;
		vcrSendMessage.session = (intptr_t) sock;
		vcrSendMessage.r = r;
		Qwrite (vcrFile, &vcrSendMessage, 20);
	}

	return r;
}


int
NET_SendToAll (sizebuf_t *data, double blocktime)
{
	double      start;
	unsigned    i;
	int         count = 0;
	bool        state1[MAX_SCOREBOARD];	/* can we send */
	bool        state2[MAX_SCOREBOARD];	/* did we send */

	for (i = 0, host_client = svs.clients; i < svs.maxclients;
		 i++, host_client++) {
		if (host_client->netconnection && host_client->active) {
			if (host_client->netconnection->driver == 0) {
				NET_SendMessage (host_client->netconnection, data);
				state1[i] = true;
				state2[i] = true;
				continue;
			}
			count++;
			state1[i] = false;
			state2[i] = false;
		} else {
			state1[i] = true;
			state2[i] = true;
		}
	}

	start = Sys_DoubleTime ();
	while (count) {
		count = 0;
		for (i = 0, host_client = svs.clients; i < svs.maxclients;
			 i++, host_client++) {
			if (!state1[i]) {
				if (NET_CanSendMessage (host_client->netconnection)) {
					state1[i] = true;
					NET_SendMessage (host_client->netconnection, data);
				} else {
					NET_GetMessage (host_client->netconnection);
				}
				count++;
				continue;
			}

			if (!state2[i]) {
				if (NET_CanSendMessage (host_client->netconnection)) {
					state2[i] = true;
				} else {
					NET_GetMessage (host_client->netconnection);
				}
				count++;
				continue;
			}
		}
		if ((Sys_DoubleTime () - start) > blocktime)
			break;
	}
	return count;
}


//=============================================================================

static void
NET_shutdown (void *data)
{
	qsocket_t  *sock;

	SetNetTime ();

	for (sock = net_activeSockets; sock; sock = sock->next)
		NET_Close (sock);

//
// shutdown the drivers
//
	for (net_driverlevel = 0; net_driverlevel < net_numdrivers;
		 net_driverlevel++) {
		if (net_drivers[net_driverlevel].initialized == true) {
			net_drivers[net_driverlevel].Shutdown ();
			net_drivers[net_driverlevel].initialized = false;
		}
	}

	if (vcrFile) {
		Sys_Printf ("Closing vcrfile.\n");
		Qclose (vcrFile);
	}
}

void
NET_Init (cbuf_t *cbuf)
{
	qfZoneScoped (true);
	int         i;
	int         controlSocket;
	qsocket_t  *s;

	net_cbuf = cbuf;

	Sys_RegisterShutdown (NET_shutdown, 0);

	if (COM_CheckParm ("-playback")) {
		net_numdrivers = 1;
		net_drivers[0].Init = VCR_Init;
	}

	if (COM_CheckParm ("-record"))
		recording = true;

	i = COM_CheckParm ("-port");
	if (!i)
		i = COM_CheckParm ("-udpport");
	if (!i)
		i = COM_CheckParm ("-ipxport");

	if (i) {
		if (i < com_argc - 1)
			DEFAULTnet_hostport = atoi (com_argv[i + 1]);
		else
			Sys_Error ("NET_Init: you must specify a number after -port");
	}
	net_hostport = DEFAULTnet_hostport;

	if (COM_CheckParm ("-listen") || net_is_dedicated)
		listening = true;
	net_numsockets = svs.maxclientslimit;
	if (!net_is_dedicated)
		net_numsockets++;

	SetNetTime ();

	for (i = 0; i < net_numsockets; i++) {
		s = (qsocket_t *) Hunk_AllocName (0, sizeof (qsocket_t), "qsocket");
		s->next = net_freeSockets;
		net_freeSockets = s;
		s->disconnected = true;
	}

	// allocate space for network message buffer
	SZ_Alloc (&_net_message_message, NET_MAXMESSAGE);

	Cvar_Register (&net_messagetimeout_cvar, 0, 0);
	Cvar_Register (&hostname_cvar, 0, 0);

	Cmd_AddCommand ("slist", NET_Slist_f, "No Description");
	Cmd_AddCommand ("listen", NET_Listen_f, "No Description");
	Cmd_AddCommand ("maxplayers", MaxPlayers_f, "No Description");
	Cmd_AddCommand ("port", NET_Port_f, "No Description");

	// initialize all the drivers
	for (net_driverlevel = 0; net_driverlevel < net_numdrivers;
		 net_driverlevel++) {
		controlSocket = net_drivers[net_driverlevel].Init ();
		if (controlSocket == -1)
			continue;
		net_drivers[net_driverlevel].initialized = true;
		net_drivers[net_driverlevel].controlSock = controlSocket;
		if (listening)
			net_drivers[net_driverlevel].Listen (true);
	}

	if (*my_tcpip_address)
		Sys_MaskPrintf (SYS_net, "TCP/IP address %s\n", my_tcpip_address);
}


static PollProcedure *pollProcedureList = NULL;

void
NET_Poll (void)
{
	PollProcedure *pp;

	SetNetTime ();

	for (pp = pollProcedureList; pp; pp = pp->next) {
		if (pp->nextTime > net_time)
			break;
		pollProcedureList = pp->next;
		pp->procedure (pp->arg);
	}
}


void
SchedulePollProcedure (PollProcedure *proc, double timeOffset)
{
	PollProcedure *pp, *prev;

	proc->nextTime = Sys_DoubleTime () + timeOffset;
	for (pp = pollProcedureList, prev = NULL; pp; pp = pp->next) {
		if (pp->nextTime >= proc->nextTime)
			break;
		prev = pp;
	}

	if (prev == NULL) {
		proc->next = pollProcedureList;
		pollProcedureList = proc;
		return;
	}

	proc->next = pp;
	prev->next = proc;
}
