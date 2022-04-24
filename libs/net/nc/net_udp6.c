/*
	net_udp6.c

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2000       Marcus Sundberg [mackan@stacken.kth.se]
	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

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
# ifdef HAVE_IPV6
#  include <winsock2.h>
#  undef IP_MULTICAST_IF
#  undef IP_MULTICAST_TTL
#  undef IP_MULTICAST_LOOP
#  undef IP_ADD_MEMBERSHIP
#  undef IP_DROP_MEMBERSHIP
#  define ip_mreq ip_mreq_icky_hack
#  include <ws2tcpip.h>
#  undef ip_mreq
#  ifndef WINSOCK_API_LINKAGE
#   define WINSOCK_API_LINKAGE
#  endif
#  ifndef _WINSOCK2API_
#   define _WINSOCK2API_
#  endif
#  define  _WINSOCKAPI_
#  define HAVE_SOCKLEN_T
# endif
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

#if defined(__GLIBC__) && !defined(s6_addr32)	// glibc macro
# define s6_addr32 in6_u.u6_addr32
# if ! __GLIBC_PREREQ (2,2)
#  define ss_family __ss_family
# endif
#endif

static char *net_family;
static cvar_t net_family_cvar = {
	.name = "net_family",
	.description =
		"Set the address family to ipv4, ipv6 or unspecified",
	.default_value = "unspecified",
	.flags = CVAR_ROM,
	.value = { .type = 0, .value = &net_family },
};

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

typedef union address {
	struct sockaddr_storage ss;
	struct sockaddr         sa;
	struct sockaddr_in      s4;
	struct sockaddr_in6     s6;
} AF_address_t;

#undef SA_LEN
#undef SS_LEN

#ifdef HAVE_SA_LEN
#define SA_LEN(sa) (sa)->sa_len
#else
#define SA_LEN(sa) (((sa)->sa_family == AF_INET6) \
					? sizeof(struct sockaddr_in6) \
					: sizeof(struct sockaddr_in))
#endif

#ifdef HAVE_SS_LEN
#define SS_LEN(ss) (ss)->ss_len
#else
#define SS_LEN(ss) (((ss)->ss_family == AF_INET6) \
					? sizeof(struct sockaddr_in6) \
					: sizeof(struct sockaddr_in))
#endif


static void
NetadrToSockadr (netadr_t *a, AF_address_t *s)
{
	memset (s, 0, sizeof (*s));

	switch (a->family) {
		case AF_INET: {
			Sys_MaskPrintf (SYS_net, "err, converting v4 to v6...\n");
			s->ss.ss_family = AF_INET6;
			s->s6.sin6_addr.s6_addr[10] = s->s6.sin6_addr.s6_addr[11] = 0xff;
			memcpy (&s->s6.sin6_addr.s6_addr[12], &a->ip, sizeof (s->s4.sin_addr));
			s->s4.sin_port = a->port;
#ifdef HAVE_SS_LEN
			s->ss.ss_len = sizeof (struct sockaddr_in6);
#endif
			break;
		}
		case AF_INET6: {
			s->ss.ss_family = a->family;
			memcpy (&s->s6.sin6_addr, &a->ip, sizeof (s->s6.sin6_addr));
			s->s6.sin6_port = a->port;
#ifdef HAVE_SS_LEN
			s->ss.ss_len = sizeof (struct sockaddr_in6);
#endif
			break;
		}
		default:
			Sys_MaskPrintf (SYS_net, "%s: Unknown address family %d", __FUNCTION__, a->family);
			break;
	}
}

static void
SockadrToNetadr (AF_address_t *s, netadr_t *a)
{
	a->family = s->ss.ss_family;
	switch (a->family) {
		case AF_INET: {
			memcpy (a->ip, &(s->s4.sin_addr), sizeof (s->s4.sin_addr));
			a->port = s->s4.sin_port;
			break;
		}
		case AF_INET6: {
			memcpy (a->ip, &(s->s6.sin6_addr), sizeof (s->s6.sin6_addr));
			a->port = s->s6.sin6_port;
			break;
		}
		default:
			Sys_MaskPrintf (SYS_net, "%s: Unknown address family 0x%x\n", __FUNCTION__, s->ss.ss_family);
			break;
	}
}

/*
static qboolean
NET_AdrIsLoopback (netadr_t a)
{
	if (IN6_IS_ADDR_LOOPBACK ((struct in6_addr *) &a.ip))
		return true;
	else if (IN6_IS_ADDR_V4MAPPED ((struct in6_addr *) &a.ip) &&
			 ((struct in_addr *) &a.ip[3])->s_addr == htonl (INADDR_LOOPBACK))
		return true;

	return false;
}
*/

qboolean
NET_CompareBaseAdr (netadr_t a, netadr_t b)
{
	if (memcmp (a.ip, b.ip, sizeof (a.ip)) == 0)
		return true;
	return false;
}

qboolean
NET_CompareAdr (netadr_t a, netadr_t b)
{
	if (memcmp (a.ip, b.ip, sizeof (a.ip)) == 0 && a.port == b.port)
		return true;
	return false;
}

const char *
NET_AdrToString (netadr_t a)
{
	static dstring_t *s;
	char		base[64];
	AF_address_t ss;

	/*
		Yes, this duplication is lame, but we want to know the real address
		family of the address so that we can know whether or not to put a
		bracket around it, and this is less ugly than trying to check the
		string returned from NET_BaseAdrToString()
	*/
	memset (&ss, 0, sizeof (ss));
	NetadrToSockadr (&a, &ss);

	// Convert any "mapped" addresses back to v4
	if (a.family == AF_INET6 && IN6_IS_ADDR_V4MAPPED (&(ss.s6.sin6_addr))) {
#ifdef HAVE_SS_LEN
		ss.ss.ss_len = sizeof (ss.s4);
#endif
		ss.ss.ss_family = AF_INET;
		memcpy (&(ss.s4.sin_addr), &ss.s6.sin6_addr.s6_addr[12], sizeof (ss.s4.sin_addr));
	}

	if (getnameinfo (&ss.sa, SS_LEN(&ss.ss), base, sizeof (base),
					 NULL, 0, NI_NUMERICHOST)) strcpy (base, "<invalid>");

	if (!s)
		s = dstring_new ();
	if (ss.ss.ss_family == AF_INET6) {
		dsprintf (s, "[%s]:%d", base, ntohs (a.port));
	} else {
		dsprintf (s, "%s:%d", base, ntohs (a.port));
	}

	return s->str;
}

const char *
NET_BaseAdrToString (netadr_t a)
{
	static char s[64];
	AF_address_t ss;

	memset (&ss, 0, sizeof (ss));
	NetadrToSockadr (&a, &ss);

	// Convert any "mapped" addresses back to v4
	if (a.family == AF_INET6 && IN6_IS_ADDR_V4MAPPED (&(ss.s6.sin6_addr))) {
#ifdef HAVE_SS_LEN
		ss.ss.ss_len = sizeof (ss.s4);
#endif
		ss.ss.ss_family = AF_INET;
		memcpy (&(ss.s4.sin_addr), &ss.s6.sin6_addr.s6_addr[12],
				sizeof (ss.s4.sin_addr));
	}

	if (getnameinfo (&ss.sa, SS_LEN(&ss.ss), s, sizeof (s),
					 NULL, 0, NI_NUMERICHOST)) strcpy (s, "<invalid>");
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
	char       *addrs, *space;
	char       *ports = NULL;
	int         err;
	struct addrinfo hints;
	struct addrinfo *resultp;
	AF_address_t	addr;
	AF_address_t	resadr;

	if (!copy)
		copy = dstring_new ();

	memset (&hints, 0, sizeof (hints));
	hints.ai_socktype = SOCK_DGRAM;
	if (strchr (net_family, '6')) {
		hints.ai_family = AF_INET6;
	} else if (strchr (net_family, '4')) {
		hints.ai_family = AF_INET;
	} else {
		hints.ai_family = AF_UNSPEC;
	}

	dstring_copystr (copy, s);
	addrs = space = copy->str;
	if (*addrs == '[') {
		addrs++;
		for (; *space && *space != ']'; space++);
		if (!*space) {
			Sys_Printf ("NET_StringToAdr: invalid IPv6 address %s\n", s);
			return 0;
		}
		*space++ = '\0';
	}

	for (; *space; space++) {
		if (*space == ':') {
			*space = '\0';
			ports = space + 1;
		}
	}

	if ((err = getaddrinfo (addrs, ports, &hints, &resultp))) {	// Error
		Sys_Printf ("NET_StringToAdr: string %s:\n%s\n", s, gai_strerror (err));
		return 0;
	}

	switch (resultp->ai_family) {
		case AF_INET:
			// convert to ipv6 addr
			memset (&addr, 0, sizeof (addr));
			memset (&resadr, 0, sizeof (resadr));
			memcpy (&resadr.s4, resultp->ai_addr, sizeof (resadr.s4));

			addr.ss.ss_family = AF_INET6;
			addr.s6.sin6_addr.s6_addr[10] = addr.s6.sin6_addr.s6_addr[11] = 0xff;
			memcpy (&(addr.s6.sin6_addr.s6_addr[12]), &resadr.s4.sin_addr,
					sizeof (resadr.s4.sin_addr));
			addr.s6.sin6_port = resadr.s4.sin_port;
			break;
		case AF_INET6:
			memcpy (&addr, resultp->ai_addr, sizeof (addr.s6));

			break;
		default:
			Sys_Printf ("NET_StringToAdr: string %s:\nprotocol family %d not "
						"supported\n", s, resultp->ai_family);
			return 0;
	}
	freeaddrinfo (resultp);

	SockadrToNetadr (&addr, a);
	Sys_MaskPrintf (SYS_net, "Raw address: %s\n", NET_BaseAdrToString (*a));

	return true;
}

qboolean
NET_GetPacket (void)
{
	int          ret;
	unsigned int fromlen;
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
	char        Buf[BUFSIZ];
	const char *Host, *Service;
	int         newsocket, Error;
	struct sockaddr_in6 address;
	struct addrinfo hints, *res;

#ifdef IPV6_V6ONLY
	int         off = 0;
#endif
#ifdef _WIN32
#define ioctl ioctlsocket
	unsigned long _true = true;
#else
	int         _true = 1;
#endif
	int         i;

	if ((newsocket = socket (PF_INET6, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		Sys_Error ("UDP_OpenSocket: socket: %s", strerror (errno));
	if (ioctl (newsocket, FIONBIO, &_true) == -1)
		Sys_Error ("UDP_OpenSocket: ioctl FIONBIO: %s", strerror (errno));
	memset (&address, 0, sizeof (address));
	address.sin6_family = AF_INET6;

	memset (&hints, 0, sizeof (hints));
	if (strchr (net_family, '6')) {
		hints.ai_family = AF_INET6;
	} else if (strchr (net_family, '4')) {
		hints.ai_family = AF_INET;
	} else {
		hints.ai_family = AF_UNSPEC;
	}
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags = AI_PASSIVE;

	// ZOID -- check for interface binding option
	if ((i = COM_CheckParm ("-ip")) && i < com_argc) {
		Host = com_argv[i + 1];
	} else {
		Host = "::0";
	}
	Sys_MaskPrintf (SYS_net, "Binding to IP address [%s]\n", Host);

	if (port == PORT_ANY)
		Service = NULL;
	else {
		sprintf (Buf, "%5d", port);
		Service = Buf;
	}

	if ((Error = getaddrinfo (Host, Service, &hints, &res)))
		Sys_Error ("UDP_OpenSocket: getaddrinfo: %s", gai_strerror (Error));

	if ((newsocket = socket (res->ai_family,
							 res->ai_socktype,
							 res->ai_protocol)) == -1)
		Sys_Error ("UDP_OpenSocket: socket: %s", strerror (errno));

	// FIONBIO sets non-blocking IO for this socket
#ifdef _WIN32
	if (ioctl (newsocket, FIONBIO, &_true) == -1)
#else /* _WIN32 */
	if (ioctl (newsocket, FIONBIO, (char *) &_true) == -1)
#endif /* _WIN32 */
		Sys_Error ("UDP_OpenSocket: ioctl FIONBIO: %s", strerror (errno));

	// don't care about the result code
#ifdef IPV6_V6ONLY
	setsockopt (newsocket, IPPROTO_IPV6, IPV6_V6ONLY, &off,	sizeof (off));
#endif

	if (bind (newsocket, res->ai_addr, res->ai_addrlen) < 0)
		Sys_Error ("UDP_OpenSocket: bind: %s", strerror (errno));

	freeaddrinfo (res);

	return newsocket;
}

static void
NET_GetLocalAddress (void)
{
	char        buff[MAXHOSTNAMELEN];
	socklen_t   namelen;
	AF_address_t address;

	if (gethostname (buff, MAXHOSTNAMELEN) == -1)
		Sys_Error ("Net_GetLocalAddress: gethostname: %s", strerror (errno));
	buff[MAXHOSTNAMELEN - 1] = 0;

	NET_StringToAdr (buff, &net_local_adr);

	namelen = sizeof (address);
	if (getsockname (net_socket, (struct sockaddr *) &address, &namelen) == -1)
		Sys_Error ("NET_GetLocalAddress: getsockname: %s", strerror (errno));
	net_local_adr.port = address.s6.sin6_port;

	Sys_Printf ("IP address %s\n", NET_AdrToString (net_local_adr));
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
	Cvar_Register (&net_family_cvar, 0, 0);

	// open the single socket to be used for all communications
	net_socket = UDP_OpenSocket (port);

	// init the message buffer
	_net_message_message.maxsize = sizeof (net_message_buffer);
	_net_message_message.data = net_message_buffer;

	// determine my name & address
	NET_GetLocalAddress ();

	net_loopback_adr.ip[15] = 1;

	Sys_Printf ("UDP (IPv6) Initialized\n");
}

static void
NET_shutdown (void)
{
#ifdef _WIN32
	closesocket (net_socket);
	WSACleanup ();
#else
	close (net_socket);
#endif
}
