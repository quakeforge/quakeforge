/*
	net_udp.c

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2000       Marcus Sundberg [mackan@stacken.kth.se]

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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
# define model_t sun_model_t
# include <netinet/in.h>
# undef model_t
#endif
#ifdef HAVE_NETDB_H
# include <netdb.h>
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif
#ifdef NeXT
# include <libc.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/msg.h"
#include "QF/sys.h"
#include "QF/qargs.h"

#include "compat.h"
#include "netchan.h"

#ifdef _WIN32
# include <windows.h>
# undef EWOULDBLOCK
# define EWOULDBLOCK    WSAEWOULDBLOCK
#endif

#ifndef MAXHOSTNAMELEN
# define MAXHOSTNAMELEN	512
#endif

#ifndef HAVE_SOCKLEN_T
# ifdef HAVE_SIZE
   typedef size_t socklen_t;
# else
   typedef unsigned int socklen_t;
# endif
#endif

int         net_socket;
netadr_t    net_local_adr;
netadr_t    net_loopback_adr;
netadr_t    net_from;

static sizebuf_t _net_message_message;
static qmsg_t _net_message = {0, 0, &_net_message_message};
qmsg_t     *net_message = &_net_message;

#define	MAX_UDP_PACKET	(MAX_MSGLEN*2)
byte        net_message_buffer[MAX_UDP_PACKET];

#ifdef _WIN32
 WSADATA     winsockdata;
#endif



static void
NetadrToSockadr (netadr_t *a, struct sockaddr_in *s)
{
	memset (s, 0, sizeof (*s));
	s->sin_family = AF_INET;

	memcpy (&s->sin_addr, &a->ip, 4);
	s->sin_port = a->port;
}

static void
SockadrToNetadr (struct sockaddr_in *s, netadr_t *a)
{
	memcpy (&a->ip, &s->sin_addr, 4);
	a->port = s->sin_port;
}

qboolean
NET_CompareBaseAdr (netadr_t a, netadr_t b)
{
	if (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2]
		&& a.ip[3] == b.ip[3])
		return true;
	return false;
}

qboolean
NET_CompareAdr (netadr_t a, netadr_t b)
{
	if (a.ip[0] == b.ip[0] && a.ip[1] == b.ip[1] && a.ip[2] == b.ip[2]
		&& a.ip[3] == b.ip[3] && a.port == b.port)
		return true;
	return false;
}

const char       *
NET_AdrToString (netadr_t a)
{
	static char s[64];

	snprintf (s, sizeof (s), "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2],
			  a.ip[3], ntohs (a.port));

	return s;
}

const char       *
NET_BaseAdrToString (netadr_t a)
{
	static char s[64];

	snprintf (s, sizeof (s), "%i.%i.%i.%i", a.ip[0], a.ip[1], a.ip[2], a.ip[3]);

	return s;
}

/*
	NET_StringToAdr

	idnewt
	idnewt:28000
	192.246.40.70
	192.246.40.70:28000
*/
qboolean
NET_StringToAdr (const char *s, netadr_t *a)
{
	static dstring_t *copy;
	char       *colon;
	struct hostent *h;
	struct sockaddr_in sadr;

	if (!copy)
		copy = dstring_new ();

	memset (&sadr, 0, sizeof (sadr));
	sadr.sin_family = AF_INET;

	sadr.sin_port = 0;

	dstring_copystr (copy, s);
	// strip off a trailing :port if present
	for (colon = copy->str; *colon; colon++)
		if (*colon == ':') {
			*colon = 0;
			sadr.sin_port = htons ((unsigned short) atoi (colon + 1));
		}

	if (copy->str[0] >= '0' && copy->str[0] <= '9') {
		int         addr = inet_addr (copy->str);
		memcpy (&sadr.sin_addr, &addr, 4);
	} else {
		if (!(h = gethostbyname (copy->str)))
			return 0;
		memcpy (&sadr.sin_addr, h->h_addr_list[0], 4);
	}

	SockadrToNetadr (&sadr, a);

	return true;
}

// Returns true if we can't bind the address locally--in other words, 
// the IP is NOT one of our interfaces.
qboolean
NET_IsClientLegal (netadr_t *adr)
{
#if 0
	int         newsocket;
	struct sockaddr_in sadr;

	if (adr->ip[0] == 127)
		return false;					// no local connections period

	NetadrToSockadr (adr, &sadr);

	if ((newsocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		Sys_Error ("NET_IsClientLegal: socket:", strerror (errno));

	sadr.sin_port = 0;

	if (bind (newsocket, (void *) &sadr, sizeof (sadr)) == -1) {
		// It is not a local address
		close (newsocket);
		return true;
	}
	close (newsocket);
	return false;
#else
	return true;
#endif
}

qboolean
NET_GetPacket (void)
{
	int         ret;
	socklen_t   fromlen;
	struct sockaddr_in from;

	fromlen = sizeof (from);
	ret =
		recvfrom (net_socket, (void *) net_message_buffer,
				  sizeof (net_message_buffer), 0, (struct sockaddr *) &from,
				  &fromlen);

	SockadrToNetadr (&from, &net_from);


	if (ret == -1) {
#ifdef _WIN32
		int         err = WSAGetLastError ();

		if (err == WSAEMSGSIZE) {
			Con_Printf ("Warning:  Oversize packet from %s\n",
						NET_AdrToString (net_from));
			return false;
		}
		if (err == 10054) {
			Con_DPrintf ("error 10054 from %s\n", NET_AdrToString (net_from));
			return false;
		}
#else // _WIN32
		int         err = errno;

		if (err == ECONNREFUSED)
			return false;
#endif // _WIN32
		if (err == EWOULDBLOCK)
			return false;
		Con_Printf ("NET_GetPacket: %d: %s\n", err, strerror (err));
		return false;
	}

	// Check for malformed packets
	if (ntohs(net_from.port)<1024) {
		Con_Printf ("Warning: Packet from %s dropped: Bad port\n",
					NET_AdrToString (net_from));
		return false;
	}

	if (from.sin_addr.s_addr==INADDR_ANY || from.sin_addr.s_addr == 
		INADDR_BROADCAST) {
		Con_Printf ("Warning: Packet dropped - bad address\n");
		return false;
	}

	_net_message_message.cursize = ret;
	if (ret == sizeof (net_message_buffer)) {
		Con_Printf ("Oversize packet from %s\n", NET_AdrToString (net_from));
		return false;
	}

	return ret;
}

void
NET_SendPacket (int length, void *data, netadr_t to)
{
	int         ret;
	struct sockaddr_in addr;

	NetadrToSockadr (&to, &addr);

	ret = sendto (net_socket, data, length, 0, (struct sockaddr *) &addr,
				  sizeof (addr));
	if (ret == -1) {
#ifdef _WIN32
		int         err = WSAGetLastError ();

		if (err == WSAEADDRNOTAVAIL)
			Con_DPrintf ("NET_SendPacket Warning: %i\n", err);
#else // _WIN32
		int         err = errno;

		if (err == ECONNREFUSED)
			return;
#endif // _WIN32
		if (err == EWOULDBLOCK)
			return;

		Con_Printf ("NET_SendPacket: %s\n", strerror (errno));
	}
}

static int
UDP_OpenSocket (int port)
{
	int         newsocket, i;
	struct sockaddr_in address;

#ifdef _WIN32
#define ioctl ioctlsocket
	unsigned long _true = true;
#else
	int         _true = 1;
#endif

	memset (&address, 0, sizeof(address));

	if ((newsocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		Sys_Error ("UDP_OpenSocket: socket:%s", strerror (errno));
	if (ioctl (newsocket, FIONBIO, &_true) == -1)
		Sys_Error ("UDP_OpenSocket: ioctl FIONBIO:%s", strerror (errno));
	address.sin_family = AF_INET;
// ZOID -- check for interface binding option
	if ((i = COM_CheckParm ("-ip")) != 0 && i < com_argc) {
		address.sin_addr.s_addr = inet_addr (com_argv[i + 1]);
		Con_Printf ("Binding to IP Interface Address of %s\n",
					inet_ntoa (address.sin_addr));
	} else
		address.sin_addr.s_addr = INADDR_ANY;
	if (port == PORT_ANY)
		address.sin_port = 0;
	else
		address.sin_port = htons ((short) port);
	if (bind (newsocket, (void *) &address, sizeof (address)) == -1)
		Sys_Error ("UDP_OpenSocket: bind: %s", strerror (errno));

	return newsocket;
}

static void
NET_GetLocalAddress (void)
{
	char        buff[MAXHOSTNAMELEN];
	socklen_t   namelen;
	struct sockaddr_in address;

	gethostname (buff, MAXHOSTNAMELEN);
	buff[MAXHOSTNAMELEN - 1] = 0;

	NET_StringToAdr (buff, &net_local_adr);

	namelen = sizeof (address);
	if (getsockname (net_socket, (struct sockaddr *) &address, &namelen) == -1)
		Sys_Error ("NET_Init: getsockname: %s", strerror (errno));
	net_local_adr.port = address.sin_port;

	Con_Printf ("IP address %s\n", NET_AdrToString (net_local_adr));
}

void
NET_Init (int port)
{
#ifdef _WIN32
	int         r;
	WORD        wVersionRequested;

	wVersionRequested = MAKEWORD (1, 1);

	r = WSAStartup (MAKEWORD (1, 1), &winsockdata);
	if (r)
		Sys_Error ("Winsock initialization failed.");
#endif /* _WIN32 */
	// open the single socket to be used for all communications
	net_socket = UDP_OpenSocket (port);

	// init the message buffer
	_net_message_message.maxsize = sizeof (net_message_buffer);
	_net_message_message.data = net_message_buffer;

	// determine my name & address
	NET_GetLocalAddress ();

	net_loopback_adr.ip[0] = 127;
	net_loopback_adr.ip[3] = 1;

	Con_Printf ("UDP (IPv4) Initialized\n");
}

void
NET_Shutdown (void)
{
#ifdef _WIN32
	closesocket (net_socket);
	WSACleanup ();
#else
	close (net_socket);
#endif
}
