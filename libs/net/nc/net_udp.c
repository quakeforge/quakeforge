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
/* Sun's model_t in sys/model.h conflicts w/ Quake's model_t */
#define model_t quakeforgemodel_t

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
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
# include <netinet/in.h>
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

#ifdef _WIN32
# include <windows.h>
# undef EWOULDBLOCK
# define EWOULDBLOCK	WSAEWOULDBLOCK
#endif

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>

#undef model_t

#include "QF/dstring.h"
#include "QF/msg.h"
#include "QF/qargs.h"
#include "QF/sys.h"

#include "netchan.h"

#ifndef MAXHOSTNAMELEN
# define MAXHOSTNAMELEN	512
#endif

#ifndef HAVE_SOCKLEN_T
# ifdef _WIN32
	typedef int socklen_t;
# else
#  ifdef HAVE_SIZE
   typedef size_t socklen_t;
#  else
   typedef unsigned int socklen_t;
#  endif
# endif
#endif

netadr_t    net_from;
netadr_t    net_local_adr;
netadr_t    net_loopback_adr;
int         net_socket;

static sizebuf_t _net_message_message;
static qmsg_t _net_message = {0, 0, &_net_message_message};
qmsg_t     *net_message = &_net_message;

#define	MAX_UDP_PACKET	(MAX_MSGLEN*2)
byte        net_message_buffer[MAX_UDP_PACKET];

#ifdef _WIN32
 WSADATA     winsockdata;
#endif

#define ADDR_SIZE 4

typedef union address {
#ifdef HAV_SS_LEN	// FIXME check properly, but should be ok
	struct sockaddr_storage ss;
#endif
	struct sockaddr         sa;
	struct sockaddr_in      s4;
} AF_address_t;

#undef SA_LEN
#undef SS_LEN

#ifdef HAVE_SA_LEN
#define SA_LEN(sa) (sa)->sa_len
#else
#define SA_LEN(sa) (sizeof(struct sockaddr_in))
#endif

#ifdef HAVE_SS_LEN
#define SS_LEN(ss) (ss)->ss_len
#else
#define SS_LEN(ss) (sizeof(struct sockaddr_in))
#endif


static void
NetadrToSockadr (netadr_t *a, AF_address_t *s)
{
	memset (s, 0, sizeof (*s));
	s->s4.sin_family = AF_INET;

	memcpy (&s->s4.sin_addr, &a->ip, ADDR_SIZE);
	s->s4.sin_port = a->port;
}

static void
SockadrToNetadr (AF_address_t *s, netadr_t *a)
{
	memcpy (&a->ip, &s->s4.sin_addr, ADDR_SIZE);
	a->port = s->s4.sin_port;
}

qboolean
NET_CompareBaseAdr (netadr_t a, netadr_t b)
{
	if (memcmp (a.ip, b.ip, ADDR_SIZE) == 0)
		return true;
	return false;
}

qboolean
NET_CompareAdr (netadr_t a, netadr_t b)
{
	if (memcmp (a.ip, b.ip, ADDR_SIZE) == 0 && a.port == b.port)
		return true;
	return false;
}

const char *
NET_AdrToString (netadr_t a)
{
	static char s[64];

	snprintf (s, sizeof (s), "%i.%i.%i.%i:%i", a.ip[0], a.ip[1], a.ip[2],
			  a.ip[3], ntohs (a.port));

	return s;
}

const char *
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
	AF_address_t sadr;

	if (!copy)
		copy = dstring_new ();

	memset (&sadr, 0, sizeof (sadr));
	sadr.s4.sin_family = AF_INET;

	sadr.s4.sin_port = 0;

	dstring_copystr (copy, s);
	// strip off a trailing :port if present
	for (colon = copy->str; *colon; colon++)
		if (*colon == ':') {
			*colon = 0;
			sadr.s4.sin_port = htons ((unsigned short) atoi (colon + 1));
		}

	if (copy->str[0] >= '0' && copy->str[0] <= '9') {
		int         addr = inet_addr (copy->str);
		memcpy (&sadr.s4.sin_addr, &addr, ADDR_SIZE);
	} else {
		if (!(h = gethostbyname (copy->str)))
			return 0;
		memcpy (&sadr.s4.sin_addr, h->h_addr_list[0], ADDR_SIZE);
	}

	SockadrToNetadr (&sadr, a);

	return true;
}

qboolean
NET_GetPacket (void)
{
	int         ret;
	socklen_t   fromlen;
	AF_address_t from;

	fromlen = sizeof (from);
	memset (&from, 0, sizeof (from));
	ret = recvfrom (net_socket, (void *) net_message_buffer,
					sizeof (net_message_buffer), 0, &from.sa, &fromlen);

	if (ret == -1) {
#ifdef _WIN32
		int         err = WSAGetLastError ();

		if (err == WSAEMSGSIZE) {
			Sys_Printf ("Warning:  Oversize packet from %s\n",
						NET_AdrToString (net_from));
			return false;
		}
		if (err == 10054) {
			Sys_Printf ("NET_GetPacket: error 10054 from %s\n",
						NET_AdrToString (net_from));
			return false;
		}
#else // _WIN32
		int         err = errno;

		if (err == ECONNREFUSED)
			return false;
#endif // _WIN32
		if (err == EWOULDBLOCK)
			return false;
		Sys_Printf ("NET_GetPacket: %s\n", strerror (err));
		return false;
	}

	SockadrToNetadr (&from, &net_from);

	// Check for malformed packets
	if (ntohs (net_from.port) < 1024) {
		Sys_Printf ("Warning: Packet from %s dropped: Bad port\n",
					NET_AdrToString (net_from));
		return false;
	}

	if (from.s4.sin_addr.s_addr == INADDR_ANY
		|| from.s4.sin_addr.s_addr == INADDR_BROADCAST) {
		Sys_Printf ("Warning: Packet dropped - bad address\n");
		return false;
	}

	_net_message_message.cursize = ret;
	if (ret == sizeof (net_message_buffer)) {
		Sys_Printf ("Oversize packet from %s\n", NET_AdrToString (net_from));
		return false;
	}

	return ret;
}

void
NET_SendPacket (int length, const void *data, netadr_t to)
{
	int         ret;
	AF_address_t addr;

	NetadrToSockadr (&to, &addr);

	ret = sendto (net_socket, data, length, 0, &addr.sa, SA_LEN (&addr.sa));
	if (ret == -1) {
#ifdef _WIN32
		int         err = WSAGetLastError ();

		if (err == WSAEADDRNOTAVAIL)
			Sys_Printf ("NET_SendPacket Warning: %i\n", err);
#else /* _WIN32 */
		int         err = errno;

		if (err == ECONNREFUSED)
			return;
#endif /* _WIN32 */
		if (err == EWOULDBLOCK)
			return;

		Sys_Printf ("NET_SendPacket: %s\n", strerror (err));
	}
}

static int
UDP_OpenSocket (int port)
{
	int         newsocket, i;
	AF_address_t address;

#ifdef _WIN32
#define ioctl ioctlsocket
	unsigned long flags;
#else
	int         flags;
#endif

	memset (&address, 0, sizeof(address));

	if ((newsocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		Sys_Error ("UDP_OpenSocket: socket: %s", strerror (errno));
#if defined (HAVE_IOCTL) || defined (_WIN32)
	flags = 1;
	if (ioctl (newsocket, FIONBIO, &flags) == -1)
		Sys_Error ("UDP_OpenSocket: ioctl FIONBIO: %s", strerror (errno));
#else
	flags = fcntl(newsocket, F_GETFL, 0);
	if (fcntl (newsocket, F_SETFL, flags | O_NONBLOCK) == -1)
		Sys_Error ("UDP_OpenSocket: fcntl O_NONBLOCK: %s", strerror (errno));
#endif
	address.s4.sin_family = AF_INET;
// ZOID -- check for interface binding option
	if ((i = COM_CheckParm ("-ip")) != 0 && i < com_argc) {
		address.s4.sin_addr.s_addr = inet_addr (com_argv[i + 1]);
		Sys_Printf ("Binding to IP Interface Address of %s\n",
					inet_ntoa (address.s4.sin_addr));
	} else
		address.s4.sin_addr.s_addr = INADDR_ANY;
	if (port == PORT_ANY)
		address.s4.sin_port = 0;
	else
		address.s4.sin_port = htons ((short) port);
	if (bind (newsocket, &address.sa, SA_LEN (&address.sa)) == -1)
		Sys_Error ("UDP_OpenSocket: bind: %s", strerror (errno));

	return newsocket;
}

static void
NET_GetLocalAddress (void)
{
	char        buff[MAXHOSTNAMELEN] = "127.0.0.1";	//FIXME
	socklen_t   namelen;
	AF_address_t address;

#ifdef HAVE_GETHOSTNAME
	if (gethostname (buff, MAXHOSTNAMELEN) == -1)
		Sys_Error ("Net_GetLocalAddress: gethostname: %s", strerror (errno));
	buff[MAXHOSTNAMELEN - 1] = 0;
#endif

	NET_StringToAdr (buff, &net_local_adr);

	namelen = sizeof (address);
	if (getsockname (net_socket, (struct sockaddr *) &address, &namelen) == -1)
		Sys_Error ("NET_GetLocalAddress: getsockname: %s", strerror (errno));
	net_local_adr.port = address.s4.sin_port;

	Sys_Printf ("IP address %s\n", NET_AdrToString (net_local_adr));
}

static void
NET_shutdown (void *data)
{
#ifdef _WIN32
	closesocket (net_socket);
	WSACleanup ();
#else
	close (net_socket);
#endif
}

void
NET_Init (int port)
{
#ifdef _WIN32
	int         r;
	WORD        wVersionRequested;

	wVersionRequested = MAKEWORD (1, 1);

	r = WSAStartup (wVersionRequested, &winsockdata);
	if (r)
		Sys_Error ("Winsock initialization failed.");
#endif /* _WIN32 */
	Sys_RegisterShutdown (NET_shutdown, 0);

	net_socket = UDP_OpenSocket (port);

	// init the message buffer
	_net_message_message.maxsize = sizeof (net_message_buffer);
	_net_message_message.data = net_message_buffer;

	// determine my name & address
	NET_GetLocalAddress ();

	net_loopback_adr.ip[0] = 127;
	net_loopback_adr.ip[3] = 1;

	Sys_Printf ("UDP (IPv4) Initialized\n");
}
