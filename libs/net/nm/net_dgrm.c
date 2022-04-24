/*
	net_dgrm.c

	Datagram network driver.

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

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/sys.h"
#include "QF/heapsort.h"
#include "QF/keys.h"
#include "QF/qendian.h"
#include "QF/msg.h"
#include "QF/qargs.h"
#include "QF/screen.h"

#include "netmain.h"

#include "../nq/include/server.h"

// This is enables a simple IP banning mechanism
#define BAN_TEST

#ifdef BAN_TEST
#if defined(_WIN32)
#include "winquake.h"
	typedef uint32_t in_addr_t;
#else
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
#endif
#endif // BAN_TEST

#include "net_dgrm.h"

// these two macros are to make the code more readable
#define sfunc	net_landrivers[sock->landriver]
#define dfunc	net_landrivers[net_landriverlevel]

static int  net_landriverlevel;

/* statistic counters */
int         packetsSent = 0;
int         packetsReSent = 0;
int         packetsReceived = 0;
int         receivedDuplicateCount = 0;
int         shortPacketCount = 0;
int         droppedDatagrams;

static int  myDriverLevel;

static byte dgrm_send_packet_data[MAX_DATAGRAM + NET_HEADERSIZE];
static sizebuf_t dgrm_send_packet = {
	.data = dgrm_send_packet_data,
	.maxsize = sizeof (dgrm_send_packet_data),
};

static byte dgrm_receive_packet_data[MAX_DATAGRAM + NET_HEADERSIZE];
static sizebuf_t dgrm_receive_packet = {
	.data = dgrm_receive_packet_data,
	.maxsize = sizeof (dgrm_receive_packet_data),
};
static qmsg_t dgrm_message = {
	.message = &dgrm_receive_packet,
};

// FIXME: MENUCODE
//extern int  m_return_state;
//extern int  m_state;
//extern qboolean m_return_onerror;
//extern char m_return_reason[32];


#ifdef BAN_TEST
in_addr_t banAddr = 0x00000000;
in_addr_t banMask = 0xffffffff;

static void
NET_Ban_f (void)
{
	char        addrStr[32];		//FIXME: overflow
	char        maskStr[32];		//FIXME: overflow
	__attribute__((format(PRINTF, 1, 2))) void (*print) (const char *fmt, ...);

	if (cmd_source == src_command) {
		if (!sv.active) {
			CL_Cmd_ForwardToServer ();
			return;
		}
		print = Sys_Printf;
	} else {
		if (*sv_globals.deathmatch && !host_client->privileged) {
			return;
		}
		print = SV_ClientPrintf;
	}

	switch (Cmd_Argc ()) {
		case 1:
			if (banAddr) {
				struct in_addr t;
				t.s_addr = banAddr;
				strcpy (addrStr, inet_ntoa (t));
				t.s_addr = banMask;
				strcpy (maskStr, inet_ntoa (t));
				print ("Banning %s [%s]\n", addrStr, maskStr);
			} else {
				print ("Banning not active\n");
			}
			break;

		case 2:
			if (strcasecmp (Cmd_Argv (1), "off") == 0) {
				banAddr = 0x00000000;
			} else {
				banAddr = inet_addr (Cmd_Argv (1));
			}
			banMask = 0xffffffff;
			break;

		case 3:
			banAddr = inet_addr (Cmd_Argv (1));
			banMask = inet_addr (Cmd_Argv (2));
			break;

		default:
			print ("BAN ip_address [mask]\n");
			break;
	}
}
#endif


int
Datagram_SendMessage (qsocket_t *sock, sizebuf_t *data)
{
	unsigned    packetLen;
	unsigned    dataLen;
	unsigned    eom;

	memcpy (sock->sendMessage, data->data, data->cursize);
	sock->sendMessageLength = data->cursize;

	if (data->cursize <= MAX_DATAGRAM) {
		dataLen = data->cursize;
		eom = NETFLAG_EOM;
	} else {
		dataLen = MAX_DATAGRAM;
		eom = 0;
	}
	packetLen = NET_HEADERSIZE + dataLen;

	SZ_Clear (&dgrm_send_packet);
	MSG_WriteLongBE (&dgrm_send_packet, packetLen | (NETFLAG_DATA | eom));
	MSG_WriteLongBE (&dgrm_send_packet, sock->sendSequence++);
	MSG_WriteBytes (&dgrm_send_packet, sock->sendMessage, dataLen);

	sock->canSend = false;

	if (sfunc.Write (sock->socket, dgrm_send_packet.data,
					 dgrm_send_packet.cursize, &sock->addr) == -1) {
		return -1;
	}

	sock->lastSendTime = net_time;
	packetsSent++;
	return 1;
}


static int
SendMessageNext (qsocket_t *sock)
{
	unsigned    packetLen;
	unsigned    dataLen;
	unsigned    eom;

	if (sock->sendMessageLength <= MAX_DATAGRAM) {
		dataLen = sock->sendMessageLength;
		eom = NETFLAG_EOM;
	} else {
		dataLen = MAX_DATAGRAM;
		eom = 0;
	}
	packetLen = NET_HEADERSIZE + dataLen;

	SZ_Clear (&dgrm_send_packet);
	MSG_WriteLongBE (&dgrm_send_packet, packetLen | (NETFLAG_DATA | eom));
	MSG_WriteLongBE (&dgrm_send_packet, sock->sendSequence++);
	MSG_WriteBytes (&dgrm_send_packet, sock->sendMessage, dataLen);

	sock->sendNext = false;

	if (sfunc.Write (sock->socket, dgrm_send_packet.data,
					 dgrm_send_packet.cursize, &sock->addr) == -1) {
		return -1;
	}

	sock->lastSendTime = net_time;
	packetsSent++;
	return 1;
}


static int
ReSendMessage (qsocket_t *sock)
{
	unsigned    packetLen;
	unsigned    dataLen;
	unsigned    eom;

	if (sock->sendMessageLength <= MAX_DATAGRAM) {
		dataLen = sock->sendMessageLength;
		eom = NETFLAG_EOM;
	} else {
		dataLen = MAX_DATAGRAM;
		eom = 0;
	}
	packetLen = NET_HEADERSIZE + dataLen;

	SZ_Clear (&dgrm_send_packet);
	MSG_WriteLongBE (&dgrm_send_packet, packetLen | (NETFLAG_DATA | eom));
	MSG_WriteLongBE (&dgrm_send_packet, sock->sendSequence - 1);
	MSG_WriteBytes (&dgrm_send_packet, sock->sendMessage, dataLen);

	sock->sendNext = false;

	if (sfunc.Write (sock->socket, dgrm_send_packet.data,
					 dgrm_send_packet.cursize, &sock->addr) == -1) {
		return -1;
	}

	sock->lastSendTime = net_time;
	packetsReSent++;
	return 1;
}


qboolean
Datagram_CanSendMessage (qsocket_t *sock)
{
	if (sock->sendNext) {
		SendMessageNext (sock);
	}

	return sock->canSend;
}


__attribute__((const)) qboolean
Datagram_CanSendUnreliableMessage (qsocket_t *sock)
{
	return true;
}


int
Datagram_SendUnreliableMessage (qsocket_t *sock, sizebuf_t *data)
{
	int         packetLen;

	packetLen = NET_HEADERSIZE + data->cursize;

	SZ_Clear (&dgrm_send_packet);
	MSG_WriteLongBE (&dgrm_send_packet, packetLen | NETFLAG_UNRELIABLE);
	MSG_WriteLongBE (&dgrm_send_packet, sock->unreliableSendSequence++);
	MSG_WriteBytes (&dgrm_send_packet, data->data, data->cursize);

	if (sfunc.Write (sock->socket, dgrm_send_packet.data,
					 dgrm_send_packet.cursize, &sock->addr) == -1) {
		return -1;
	}

	packetsSent++;
	return 1;
}


int
Datagram_GetMessage (qsocket_t *sock)
{
	unsigned    length;
	unsigned    flags;
	int         ret = 0;
	netadr_t    readaddr;
	unsigned    sequence;
	unsigned    count;

	/// If there is an outstanding reliable packet and more than 1 second has
	/// passed, resend the packet.
	if (!sock->canSend) {
		if ((net_time - sock->lastSendTime) > 1.0) {
			ReSendMessage (sock);
		}
	}

	while (1) {
		SZ_Clear (&dgrm_receive_packet);
		length = sfunc.Read (sock->socket, dgrm_receive_packet.data,
							 dgrm_receive_packet.maxsize, &readaddr);
		dgrm_receive_packet.cursize = length;

//  if ((rand() & 255) > 220)
//      continue;

		if (length == 0) {
			break;
		}

		if ((int) length == -1) {
			Sys_Printf ("Read error\n");
			return -1;
		}

		if (sfunc.AddrCompare (&readaddr, &sock->addr) != 0) {
			continue;
		}

		if (length < NET_HEADERSIZE) {
			shortPacketCount++;
			continue;
		}

		MSG_BeginReading (&dgrm_message);
		length = MSG_ReadLongBE (&dgrm_message);

		flags = length & (~NETFLAG_LENGTH_MASK);
		length &= NETFLAG_LENGTH_MASK;

		if (flags & NETFLAG_CTL) {
			continue;
		}

		sequence = MSG_ReadLongBE (&dgrm_message);
		packetsReceived++;

		if (flags & NETFLAG_UNRELIABLE) {
			if (sequence < sock->unreliableReceiveSequence) {
				Sys_MaskPrintf (SYS_net, "Got a stale datagram\n");
				ret = 0;
				break;
			}
			if (sequence != sock->unreliableReceiveSequence) {
				count = sequence - sock->unreliableReceiveSequence;
				droppedDatagrams += count;
				Sys_MaskPrintf (SYS_net, "Dropped %u datagram(s)\n", count);
			}
			sock->unreliableReceiveSequence = sequence + 1;

			length -= NET_HEADERSIZE;

			/// Copy unreliable data to net_message
			SZ_Clear (net_message->message);
			byte       *buf = SZ_GetSpace (net_message->message, length);
			MSG_ReadBytes (&dgrm_message, buf, length);

			ret = 2;
			break;
		}

		if (flags & NETFLAG_ACK) {
			if (sequence != (sock->sendSequence - 1)) {
				Sys_MaskPrintf (SYS_net, "Stale ACK received\n");
				continue;
			}
			if (sequence == sock->ackSequence) {
				sock->ackSequence++;
				if (sock->ackSequence != sock->sendSequence) {
					Sys_MaskPrintf (SYS_net, "ack sequencing error\n");
				}
			} else {
				Sys_MaskPrintf (SYS_net, "Duplicate ACK received\n");
				continue;
			}
			sock->sendMessageLength -= MAX_DATAGRAM;
			if (sock->sendMessageLength > 0) {
				memcpy (sock->sendMessage, sock->sendMessage + MAX_DATAGRAM,
						sock->sendMessageLength);
				sock->sendNext = true;
			} else {
				sock->sendMessageLength = 0;
				sock->canSend = true;
			}
			continue;
		}

		if (flags & NETFLAG_DATA) {
			SZ_Clear (&dgrm_send_packet);
			MSG_WriteLongBE (&dgrm_send_packet, NET_HEADERSIZE | NETFLAG_ACK);
			MSG_WriteLongBE (&dgrm_send_packet, sequence);
			sfunc.Write (sock->socket, dgrm_send_packet.data,
						 dgrm_send_packet.cursize, &readaddr);

			if (sequence != sock->receiveSequence) {
				receivedDuplicateCount++;
				continue;
			}
			sock->receiveSequence++;

			length -= NET_HEADERSIZE;

			if (flags & NETFLAG_EOM) {
				SZ_Clear (net_message->message);
				SZ_Write (net_message->message, sock->receiveMessage,
						  sock->receiveMessageLength);
				byte       *buf = SZ_GetSpace (net_message->message, length);
				MSG_ReadBytes (&dgrm_message, buf, length);
				sock->receiveMessageLength = 0;

				ret = 1;
				break;
			}

			/// Append reliable data to sock->receiveMessage.
			MSG_ReadBytes (&dgrm_message,
						   sock->receiveMessage + sock->receiveMessageLength,
						   length);
			sock->receiveMessageLength += length;
			continue;
		}
	}

	if (sock->sendNext) {
		SendMessageNext (sock);
	}

	return ret;
}


static void
PrintStats (qsocket_t *s)
{
	Sys_Printf ("canSend = %4u   \n", s->canSend);
	Sys_Printf ("sendSeq = %4u   ", s->sendSequence);
	Sys_Printf ("recvSeq = %4u   \n", s->receiveSequence);
	Sys_Printf ("\n");
}

static void
NET_Stats_f (void)
{
	qsocket_t  *s;

	if (Cmd_Argc () == 1) {
		Sys_Printf ("unreliable messages sent   = %i\n",
					unreliableMessagesSent);
		Sys_Printf ("unreliable messages recv   = %i\n",
					unreliableMessagesReceived);
		Sys_Printf ("reliable messages sent     = %i\n", messagesSent);
		Sys_Printf ("reliable messages received = %i\n", messagesReceived);
		Sys_Printf ("packetsSent                = %i\n", packetsSent);
		Sys_Printf ("packetsReSent              = %i\n", packetsReSent);
		Sys_Printf ("packetsReceived            = %i\n", packetsReceived);
		Sys_Printf ("receivedDuplicateCount     = %i\n",
					receivedDuplicateCount);
		Sys_Printf ("shortPacketCount           = %i\n", shortPacketCount);
		Sys_Printf ("droppedDatagrams           = %i\n", droppedDatagrams);
	} else if (strcmp (Cmd_Argv (1), "*") == 0) {
		for (s = net_activeSockets; s; s = s->next) {
			PrintStats (s);
		}
		for (s = net_freeSockets; s; s = s->next) {
			PrintStats (s);
		}
	} else {
		for (s = net_activeSockets; s; s = s->next) {
			if (strcasecmp (Cmd_Argv (1), s->address) == 0) {
				break;
			}
		}
		if (s == NULL) {
			for (s = net_freeSockets; s; s = s->next) {
				if (strcasecmp (Cmd_Argv (1), s->address) == 0) {
					break;
				}
			}
		}
		if (s == NULL) {
			return;
		}
		PrintStats (s);
	}
}

int
Datagram_Init (void)
{
	int         i;
	int         csock;

	myDriverLevel = net_driverlevel;
	Cmd_AddCommand ("net_stats", NET_Stats_f, "No Description");

	if (COM_CheckParm ("-nolan")) {
		return -1;
	}

	for (i = 0; i < net_numlandrivers; i++) {
		csock = net_landrivers[i].Init ();
		if (csock == -1) {
			continue;
		}
		net_landrivers[i].initialized = true;
		net_landrivers[i].controlSock = csock;
	}

#ifdef BAN_TEST
	Cmd_AddCommand ("ban", NET_Ban_f, "No Description");
#endif

	return 0;
}


void
Datagram_Shutdown (void)
{
	int         i;

//
// shutdown the lan drivers
//
	for (i = 0; i < net_numlandrivers; i++) {
		if (net_landrivers[i].initialized) {
			net_landrivers[i].Shutdown ();
			net_landrivers[i].initialized = false;
		}
	}
}


void
Datagram_Close (qsocket_t *sock)
{
	sfunc.CloseSocket (sock->socket);
}


void
Datagram_Listen (qboolean state)
{
	int         i;

	for (i = 0; i < net_numlandrivers; i++) {
		if (net_landrivers[i].initialized) {
			net_landrivers[i].Listen (state);
		}
	}
}

static const cvar_t **cvar_list;
static int num_cvars;

static int
dg_cvar_cmp (const void *_a, const void *_b)
{
	const cvar_t * const *a = _a;
	const cvar_t * const *b = _b;
	return strcmp ((*a)->name, (*b)->name);
}

static int
dg_cvar_select (const cvar_t *cvar, void *data)
{
	if (cvar->flags & CVAR_SERVERINFO) {
		*(int *)data += 1;
		return 1;
	}
	return 0;
}

static qsocket_t *
_Datagram_CheckNewConnections (void)
{
	netadr_t    clientaddr;
	netadr_t    newaddr;
	int         newsock;
	int         acceptsock;
	qsocket_t  *sock;
	qsocket_t  *s;
	int         len;
	int         command;
	int         control;
	int         ret;

	acceptsock = dfunc.CheckNewConnections ();
	if (acceptsock == -1) {
		return NULL;
	}

	SZ_Clear (net_message->message);

	len = dfunc.Read (acceptsock, net_message->message->data,
					  net_message->message->maxsize, &clientaddr);
	if (len < (int) sizeof (int)) {
		return NULL;
	}

	net_message->message->cursize = len;

	MSG_BeginReading (net_message);
	control = MSG_ReadLongBE (net_message);
	if (control == -1) {
		return NULL;
	}
	if ((control & (~NETFLAG_LENGTH_MASK)) != (int) NETFLAG_CTL) {
		return NULL;
	}
	if ((control & NETFLAG_LENGTH_MASK) != len) {
		return NULL;
	}

	command = MSG_ReadByte (net_message);
	if (command == CCREQ_SERVER_INFO) {
		if (strcmp (MSG_ReadString (net_message), "QUAKE") != 0) {
			return NULL;
		}

		SZ_Clear (net_message->message);
		// save space for the header, filled in later
		MSG_WriteLong (net_message->message, 0);
		MSG_WriteByte (net_message->message, CCREP_SERVER_INFO);
		dfunc.GetSocketAddr (acceptsock, &newaddr);
		MSG_WriteString (net_message->message, dfunc.AddrToString (&newaddr));
		MSG_WriteString (net_message->message, hostname);
		MSG_WriteString (net_message->message, sv.name);
		MSG_WriteByte (net_message->message, net_activeconnections);
		MSG_WriteByte (net_message->message, svs.maxclients);
		MSG_WriteByte (net_message->message, NET_PROTOCOL_VERSION);
		MSG_PokeLongBE (net_message->message, 0,
						NETFLAG_CTL | net_message->message->cursize);
		dfunc.Write (acceptsock, net_message->message->data,
					 net_message->message->cursize, &clientaddr);
		SZ_Clear (net_message->message);
		return NULL;
	}

	if (command == CCREQ_PLAYER_INFO) {
		int         playerNumber;
		int         activeNumber;
		unsigned    clientNumber;
		client_t   *client;

		playerNumber = MSG_ReadByte (net_message);
		activeNumber = -1;
		for (clientNumber = 0, client = svs.clients;
			 clientNumber < svs.maxclients; clientNumber++, client++) {
			if (client->active) {
				activeNumber++;
				if (activeNumber == playerNumber) {
					break;
				}
			}
		}
		if (clientNumber == svs.maxclients) {
			return NULL;
		}

		SZ_Clear (net_message->message);
		// save space for the header, filled in later
		MSG_WriteLong (net_message->message, 0);
		MSG_WriteByte (net_message->message, CCREP_PLAYER_INFO);
		MSG_WriteByte (net_message->message, playerNumber);
		MSG_WriteString (net_message->message, client->name);
		MSG_WriteLong (net_message->message, client->colors);
		MSG_WriteLong (net_message->message, SVfloat (client->edict, frags));
		MSG_WriteLong (net_message->message,
					   (int) (net_time - client->netconnection->connecttime));
		MSG_WriteString (net_message->message, client->netconnection->address);
		MSG_PokeLongBE (net_message->message, 0,
						NETFLAG_CTL | net_message->message->cursize);
		dfunc.Write (acceptsock, net_message->message->data,
					 net_message->message->cursize, &clientaddr);
		SZ_Clear (net_message->message);

		return NULL;
	}

	if (command == CCREQ_RULE_INFO) {
		const char *prevCvarName;
		const cvar_t *var;

		// find the search start location
		prevCvarName = MSG_ReadString (net_message);
		if (*prevCvarName) {
			cvar_t      key = { .name = prevCvarName };
			var = bsearch (&key, cvar_list, num_cvars, sizeof (cvar_t *),
						   dg_cvar_cmp);
			if (!var)
				return 0;
			var++;
		} else {
			if (cvar_list) {
				free (cvar_list);
			}
			num_cvars = 0;
			cvar_list = Cvar_Select (dg_cvar_select, &num_cvars);
			heapsort (cvar_list, num_cvars, sizeof (cvar_t *), dg_cvar_cmp);
			var = cvar_list[0];
		}

		// send the response

		SZ_Clear (net_message->message);
		// save space for the header, filled in later
		MSG_WriteLong (net_message->message, 0);
		MSG_WriteByte (net_message->message, CCREP_RULE_INFO);
		if (var) {
			MSG_WriteString (net_message->message, var->name);
			MSG_WriteString (net_message->message, Cvar_VarString (var));
		}
		MSG_PokeLongBE (net_message->message, 0,
						NETFLAG_CTL | net_message->message->cursize);
		dfunc.Write (acceptsock, net_message->message->data,
					 net_message->message->cursize, &clientaddr);
		SZ_Clear (net_message->message);

		return NULL;
	}

	if (command != CCREQ_CONNECT) {
		return NULL;
	}

	if (strcmp (MSG_ReadString (net_message), "QUAKE") != 0) {
		return NULL;
	}

	if (MSG_ReadByte (net_message) != NET_PROTOCOL_VERSION) {
		SZ_Clear (net_message->message);
		// save space for the header, filled in later
		MSG_WriteLong (net_message->message, 0);
		MSG_WriteByte (net_message->message, CCREP_REJECT);
		MSG_WriteString (net_message->message, "Incompatible version.\n");
		MSG_PokeLongBE (net_message->message, 0,
						NETFLAG_CTL | net_message->message->cursize);
		dfunc.Write (acceptsock, net_message->message->data,
					 net_message->message->cursize, &clientaddr);
		SZ_Clear (net_message->message);
		return NULL;
	}
#ifdef BAN_TEST
	// check for a ban
	if (clientaddr.family == AF_INET) {
		unsigned testAddr;

		memcpy (&testAddr, clientaddr.ip, 4);
		if ((testAddr & banMask) == banAddr) {
			SZ_Clear (net_message->message);
			// save space for the header, filled in later
			MSG_WriteLong (net_message->message, 0);
			MSG_WriteByte (net_message->message, CCREP_REJECT);
			MSG_WriteString (net_message->message, "You have been banned.\n");
			MSG_PokeLongBE (net_message->message, 0,
							NETFLAG_CTL | net_message->message->cursize);
			dfunc.Write (acceptsock, net_message->message->data,
						 net_message->message->cursize, &clientaddr);
			SZ_Clear (net_message->message);
			return NULL;
		}
	}
#endif

	// see if this guy is already connected
	for (s = net_activeSockets; s; s = s->next) {
		if (s->driver != net_driverlevel) {
			continue;
		}
		ret = dfunc.AddrCompare (&clientaddr, &s->addr);
		if (ret >= 0) {
			// is this a duplicate connection reqeust?
			if (ret == 0 && net_time - s->connecttime < 2.0) {
				// yes, so send a duplicate reply
				SZ_Clear (net_message->message);
				// save space for the header, filled in later
				MSG_WriteLong (net_message->message, 0);
				MSG_WriteByte (net_message->message, CCREP_ACCEPT);
				dfunc.GetSocketAddr (s->socket, &newaddr);
				MSG_WriteLong (net_message->message,
							   dfunc.GetSocketPort (&newaddr));
				MSG_PokeLongBE (net_message->message, 0,
								NETFLAG_CTL | net_message->message->cursize);
				dfunc.Write (acceptsock, net_message->message->data,
							 net_message->message->cursize, &clientaddr);
				SZ_Clear (net_message->message);
				return NULL;
			}
			// it's somebody coming back in from a crash/disconnect
			// so close the old qsocket and let their retry get them back in
			Sys_MaskPrintf (SYS_net, "closing stale socket %d %g\n", ret,
							net_time - s->connecttime);
			NET_Close (s);
			return NULL;
		}
	}

	// allocate a QSocket
	sock = NET_NewQSocket ();
	if (sock == NULL) {
		// no room; try to let him know
		SZ_Clear (net_message->message);
		// save space for the header, filled in later
		MSG_WriteLong (net_message->message, 0);
		MSG_WriteByte (net_message->message, CCREP_REJECT);
		MSG_WriteString (net_message->message, "Server is full.\n");
		MSG_PokeLongBE (net_message->message, 0,
						NETFLAG_CTL | net_message->message->cursize);
		dfunc.Write (acceptsock, net_message->message->data,
					 net_message->message->cursize, &clientaddr);
		SZ_Clear (net_message->message);
		return NULL;
	}
	// allocate a network socket
	newsock = dfunc.OpenSocket (0);
	if (newsock == -1) {
		Sys_MaskPrintf (SYS_net, "failed to open socket");
		NET_FreeQSocket (sock);
		return NULL;
	}
	// connect to the client
	if (dfunc.Connect (newsock, &clientaddr) == -1) {
		Sys_MaskPrintf (SYS_net, "failed to connect client");
		dfunc.CloseSocket (newsock);
		NET_FreeQSocket (sock);
		return NULL;
	}
	// everything is allocated, just fill in the details
	sock->socket = newsock;
	sock->landriver = net_landriverlevel;
	sock->addr = clientaddr;
	strcpy (sock->address, dfunc.AddrToString (&clientaddr));

	// send him back the info about the server connection he has been
	// allocated
	SZ_Clear (net_message->message);
	// save space for the header, filled in later
	MSG_WriteLong (net_message->message, 0);
	MSG_WriteByte (net_message->message, CCREP_ACCEPT);
	dfunc.GetSocketAddr (newsock, &newaddr);
	MSG_WriteLong (net_message->message, dfunc.GetSocketPort (&newaddr));
//  MSG_WriteString(net_message->message, dfunc.AddrToString(&newaddr));
	MSG_PokeLongBE (net_message->message, 0,
					NETFLAG_CTL | net_message->message->cursize);
	dfunc.Write (acceptsock, net_message->message->data,
				 net_message->message->cursize, &clientaddr);
	SZ_Clear (net_message->message);

	return sock;
}

qsocket_t *
Datagram_CheckNewConnections (void)
{
	qsocket_t  *ret = NULL;

	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers;
		 net_landriverlevel++)
		if (net_landrivers[net_landriverlevel].initialized)
			if ((ret = _Datagram_CheckNewConnections ()) != NULL)
				break;
	return ret;
}


static void
_Datagram_SearchForHosts (qboolean xmit)
{
	int         ret;
	netadr_t    readaddr;
	netadr_t    myaddr;
	int         control;

	dfunc.GetSocketAddr (dfunc.controlSock, &myaddr);
	if (xmit) {
		SZ_Clear (net_message->message);
		// save space for the header, filled in later
		MSG_WriteLong (net_message->message, 0);
		MSG_WriteByte (net_message->message, CCREQ_SERVER_INFO);
		MSG_WriteString (net_message->message, "QUAKE");
		MSG_WriteByte (net_message->message, NET_PROTOCOL_VERSION);
		MSG_PokeLongBE (net_message->message, 0,
						NETFLAG_CTL | net_message->message->cursize);
		dfunc.Broadcast (dfunc.controlSock, net_message->message->data,
						 net_message->message->cursize);
		SZ_Clear (net_message->message);
	}

	while ((ret = dfunc.Read (dfunc.controlSock, net_message->message->data,
							  net_message->message->maxsize, &readaddr)) > 0) {
		if (ret < (int) sizeof (int)) {
			continue;
		}

		net_message->message->cursize = ret;

		// don't answer our own query
		if (dfunc.AddrCompare (&readaddr, &myaddr) >= 0) {
			continue;
		}

		MSG_BeginReading (net_message);
		control = MSG_ReadLongBE (net_message);
		if (control == -1) {
			continue;
		}
		if ((control & (~NETFLAG_LENGTH_MASK)) != (int) NETFLAG_CTL) {
			continue;
		}
		if ((control & NETFLAG_LENGTH_MASK) != ret) {
			continue;
		}

		if (MSG_ReadByte (net_message) != CCREP_SERVER_INFO) {
			continue;
		}

		const char *addrstr = MSG_ReadString (net_message);
		dfunc.GetAddrFromName (addrstr, &readaddr);

		const char *name = MSG_ReadString (net_message);
		const char *map = MSG_ReadString (net_message);
		int         users = MSG_ReadByte (net_message);
		int         maxusers = MSG_ReadByte (net_message);
		int         protocol = MSG_ReadByte (net_message);
		const char *cname = dfunc.AddrToString (&readaddr);

		if (protocol != NET_PROTOCOL_VERSION) {
			char       *new_name = alloca (strlen (name) + 2);
			new_name[0] = '*';
			strcpy (new_name + 1, name);
			name = new_name;
		}

		NET_AddCachedHost (name, map, cname, users, maxusers, net_driverlevel,
						   net_landriverlevel, &readaddr);
	}
}

void
Datagram_SearchForHosts (qboolean xmit)
{
	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers;
		 net_landriverlevel++) {
		if (net_landrivers[net_landriverlevel].initialized) {
			_Datagram_SearchForHosts (xmit);
		}
	}
}


static qsocket_t *
_Datagram_Connect (const char *host)
{
	netadr_t    sendaddr;
	netadr_t    readaddr;
	qsocket_t  *sock;
	int         newsock;
	int         ret;
	int         reps;
	double      start_time;
	int         control;
	const char *reason;

	// see if we can resolve the host name
	if (dfunc.GetAddrFromName (host, &sendaddr) == -1) {
		return NULL;
	}

	newsock = dfunc.OpenSocket (0);
	if (newsock == -1) {
		return NULL;
	}

	sock = NET_NewQSocket ();
	if (sock == NULL) {
		goto ErrorReturn2;
	}
	sock->socket = newsock;
	sock->landriver = net_landriverlevel;

	// connect to the host
	if (dfunc.Connect (newsock, &sendaddr) == -1) {
		goto ErrorReturn;
	}

	// send the connection request
	Sys_Printf ("trying...\n");
	CL_UpdateScreen (cl.time);
	start_time = net_time;

	for (reps = 0; reps < 3; reps++) {
		SZ_Clear (net_message->message);
		// save space for the header, filled in later
		MSG_WriteLong (net_message->message, 0);
		MSG_WriteByte (net_message->message, CCREQ_CONNECT);
		MSG_WriteString (net_message->message, "QUAKE");
		MSG_WriteByte (net_message->message, NET_PROTOCOL_VERSION);
		MSG_PokeLongBE (net_message->message, 0,
						NETFLAG_CTL | net_message->message->cursize);
		dfunc.Write (newsock, net_message->message->data,
					 net_message->message->cursize, &sendaddr);
		SZ_Clear (net_message->message);
		do {
			ret =
				dfunc.Read (newsock, net_message->message->data,
							net_message->message->maxsize, &readaddr);
			// if we got something, validate it
			if (ret > 0) {
				// is it from the right place?
				if (sfunc.AddrCompare (&readaddr, &sendaddr) != 0) {
					Sys_MaskPrintf (SYS_net, "%2d ",
									sfunc.AddrCompare (&readaddr, &sendaddr));
					Sys_MaskPrintf (SYS_net, "%d %s ", readaddr.family,
									sfunc.AddrToString (&readaddr));
					Sys_MaskPrintf (SYS_net, "%d %s\n", sendaddr.family,
									sfunc.AddrToString (&sendaddr));
					ret = 0;
					continue;
				}

				if (ret < (int) sizeof (int)) {
					ret = 0;
					continue;
				}

				net_message->message->cursize = ret;
				MSG_BeginReading (net_message);

				control = MSG_ReadLongBE (net_message);
				if (control == -1) {
					ret = 0;
					continue;
				}
				if ((control & (~NETFLAG_LENGTH_MASK)) != (int) NETFLAG_CTL) {
					ret = 0;
					continue;
				}
				if ((control & NETFLAG_LENGTH_MASK) != ret) {
					ret = 0;
					continue;
				}
			}
		} while (ret == 0 && (SetNetTime () - start_time) < 2.5);

		if (ret) {
			break;
		}
		Sys_Printf ("still trying...\n");
		CL_UpdateScreen (cl.time);
		start_time = SetNetTime ();
	}

	if (ret == 0) {
		reason = "No Response";
		Sys_Printf ("%s\n", reason);
//		strcpy (m_return_reason, reason);
		goto ErrorReturn;
	}

	if (ret == -1) {
		reason = "Network Error";
		Sys_Printf ("%s\n", reason);
//		strcpy (m_return_reason, reason);
		goto ErrorReturn;
	}

	ret = MSG_ReadByte (net_message);
	if (ret == CCREP_REJECT) {
		reason = MSG_ReadString (net_message);
		Sys_Printf ("%s\n", reason);
//		strncpy (m_return_reason, reason, 31);
		goto ErrorReturn;
	}

	if (ret == CCREP_ACCEPT) {
		memcpy (&sock->addr, &sendaddr, sizeof (netadr_t));
		dfunc.SetSocketPort (&sock->addr, MSG_ReadLong (net_message));
	} else {
		reason = "Bad Response";
		Sys_Printf ("%s\n", reason);
//		strcpy (m_return_reason, reason);
		goto ErrorReturn;
	}

	dfunc.GetNameFromAddr (&sendaddr, sock->address);

	Sys_Printf ("Connection accepted\n");
	sock->lastMessageTime = SetNetTime ();

	// switch the connection to the specified address
	if (dfunc.Connect (newsock, &sock->addr) == -1) {
		reason = "Connect to Game failed";
		Sys_Printf ("%s\n", reason);
//		strcpy (m_return_reason, reason);
		goto ErrorReturn;
	}

	// FIXME: MENUCODE
//	m_return_onerror = false;
	return sock;

  ErrorReturn:
	// FIXME: MENUCODE - do something with reason
	Sys_MaskPrintf (SYS_net, "FIXME: MENUCODE - do something with reason\n");
	NET_FreeQSocket (sock);
  ErrorReturn2:
	dfunc.CloseSocket (newsock);
	// FIXME: MENUCODE
//	if (m_return_onerror) {
//		key_dest = key_menu;
//		m_state = m_return_state;
//		m_return_onerror = false;
//	}
	return NULL;
}

qsocket_t  *
Datagram_Connect (const char *host)
{
	qsocket_t  *ret = NULL;

	for (net_landriverlevel = 0; net_landriverlevel < net_numlandrivers;
		 net_landriverlevel++) {
		if (net_landrivers[net_landriverlevel].initialized) {
			if ((ret = _Datagram_Connect (host)) != NULL) {
				break;
			}
		}
	}
	return ret;
}
