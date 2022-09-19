/*
	net_udp.c

	UDP lan driver.

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

#include <sys/types.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
# define model_t sun_model_t
# include <netinet/in.h>
# undef model_t
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
# include <netdb.h>
#endif
#ifdef HAVE_IFADDRS_H
# include <ifaddrs.h>
#endif
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef __sun__
# include <sys/filio.h>
#endif
#ifdef NeXT
# include <libc.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/qargs.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "compat.h"
#include "netmain.h"
#include "net_udp.h"

#ifdef HAVE_IN_PKTINFO
# ifndef SOL_IP		// BSD-based stacks don't define this.
#  define SOL_IP IPPROTO_IP
# endif
#endif

#ifdef _WIN32
# undef EWOULDBLOCK
# define EWOULDBLOCK    WSAEWOULDBLOCK
# undef ECONNREFUSED
# define ECONNREFUSED   WSAECONNREFUSED
#endif

#ifndef MAXHOSTNAMELEN
# define MAXHOSTNAMELEN 512
#endif

#ifndef HAVE_SOCKLEN_T
# ifdef _WIN64
	typedef int socklen_t;
# else
#  ifdef HAVE_SIZE
	typedef size_t socklen_t;
#  else
	typedef unsigned int socklen_t;
#  endif
# endif
#endif

#define ADDR_SIZE 4

typedef union address {
#ifdef HAVE_SS_LEN
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
	a->family = s->s4.sin_family;
}

static int  net_acceptsocket = -1;		// socket for fielding new
										// connections
static int  net_controlsocket;
static int  net_broadcastsocket = 0;
static netadr_t broadcastaddr;

static uint32_t myAddr;

static int num_ifaces;
uint32_t *ifaces;
uint32_t *default_iface;
uint32_t *last_iface;

static int
get_iface_list (int sock)
{
#ifdef HAVE_GETIFADDRS
	struct ifaddrs *ifa_head;
	struct ifaddrs *ifa;
	int         index;

	if (getifaddrs (&ifa_head) < 0)
		goto no_ifaddrs;
	for (ifa = ifa_head; ifa; ifa = ifa->ifa_next) {
		if (!(ifa->ifa_flags & IFF_UP))
			continue;
		if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
			continue;
		index = if_nametoindex (ifa->ifa_name);
		if (index > num_ifaces)
			num_ifaces = index;
	}
	ifaces = malloc (num_ifaces * sizeof (uint32_t));
	Sys_MaskPrintf (SYS_net, "%d interfaces\n", num_ifaces);
	for (ifa = ifa_head; ifa; ifa = ifa->ifa_next) {
		struct sockaddr_in *sa;

		if (!(ifa->ifa_flags & IFF_UP))
			continue;
		if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
			continue;
		index = if_nametoindex (ifa->ifa_name) - 1;
		sa = (struct sockaddr_in *) ifa->ifa_addr;
		memcpy (&ifaces[index], &sa->sin_addr, sizeof (uint32_t));
		Sys_MaskPrintf (SYS_net, "    %-10s %s\n", ifa->ifa_name,
						inet_ntoa (sa->sin_addr));
		if (!default_iface && ifaces[index] != htonl (0x7f000001))
			default_iface = &ifaces[index];
	}
	freeifaddrs (ifa_head);
	return 0;
no_ifaddrs:
#endif
	ifaces = &myAddr;
	default_iface = &ifaces[0];
	num_ifaces = 1;
	return 0;
}

int
UDP_Init (void)
{
	struct hostent *local = 0;
	char        buff[MAXHOSTNAMELEN];
	netadr_t    addr;
	char       *colon;
#ifdef _WIN32
	WSADATA winsockdata;
	int	r;

	r = WSAStartup (MAKEWORD (1, 1), &winsockdata);
	if (r)
		Sys_Error ("Winsock initialization failed.");
#endif
	if (COM_CheckParm ("-noudp"))
		return -1;

	// determine my name & address
#ifdef HAVE_GETHOSTNAME	//FIXME
	gethostname (buff, MAXHOSTNAMELEN);
	local = gethostbyname (buff);
#endif
	if (local)
		myAddr = *(uint32_t *) local->h_addr_list[0];
	else
		myAddr = 0;

	// if the quake hostname isn't set, set it to the machine name
	if (strcmp (hostname, "UNNAMED") == 0) {
		buff[15] = 0;
		Cvar_Set ("hostname", buff);
	}

	if ((net_controlsocket = UDP_OpenSocket (0)) == -1)
		Sys_Error ("UDP_Init: Unable to open control socket");

	get_iface_list (net_controlsocket);

	{
		AF_address_t t;
		t.s4.sin_family = AF_INET;
		t.s4.sin_addr.s_addr = INADDR_BROADCAST;
		t.s4.sin_port = htons (net_hostport);
		SockadrToNetadr (&t, &broadcastaddr);
	}

	UDP_GetSocketAddr (net_controlsocket, &addr);
	strcpy (my_tcpip_address, UDP_AddrToString (&addr));
	colon = strrchr (my_tcpip_address, ':');
	if (colon)
		*colon = 0;

	Sys_Printf ("UDP (IPv4) Initialized\n");
	tcpipAvailable = true;

	return net_controlsocket;
}

void
UDP_Shutdown (void)
{
	UDP_Listen (false);

	UDP_CloseSocket (net_controlsocket);
}

void
UDP_Listen (qboolean state)
{
	// enable listening
	if (state) {
		if (net_acceptsocket != -1)
			return;
		if ((net_acceptsocket = UDP_OpenSocket (net_hostport)) == -1)
			Sys_Error ("UDP_Listen: Unable to open accept socket");
		return;
	}
	// disable listening
	if (net_acceptsocket == -1)
		return;
	UDP_CloseSocket (net_acceptsocket);
	net_acceptsocket = -1;
}

int
UDP_OpenSocket (int port)
{
	int         newsocket;
	struct sockaddr_in address;
#ifdef _WIN32
#define ioctl ioctlsocket
	unsigned long flags;
#else
	int         flags;
#endif
#ifdef HAVE_IN_PKTINFO
	int         ip_pktinfo = 1;
#endif

	if ((newsocket = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		return -1;

#if defined (HAVE_IOCTL) || defined (_WIN32)
	flags = 1;
	if (ioctl (newsocket, FIONBIO, &flags) == -1)
		goto ErrorReturn;
#else
	flags = fcntl (newsocket, F_GETFL, 0);
	if (fcntl (newsocket, F_SETFL, flags | O_NONBLOCK) == -1)
		goto ErrorReturn;
#endif
#ifdef HAVE_IN_PKTINFO
	if (setsockopt (newsocket, SOL_IP, IP_PKTINFO, &ip_pktinfo,
					sizeof (ip_pktinfo)) == -1) {
		close (newsocket);
		return -1;
	}
#endif

	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons (port);
	if (bind (newsocket, (void *) &address, sizeof (address)) == -1)
		goto ErrorReturn;

	return newsocket;

  ErrorReturn:
	UDP_CloseSocket (newsocket);
	return -1;
}

int
UDP_CloseSocket (int socket)
{
	if (socket == net_broadcastsocket)
		net_broadcastsocket = 0;
#ifdef _WIN32
	closesocket (socket);
	WSACleanup ();
	return 0;
#else
	return close (socket);
#endif
}

/*
  PartialIPAddress

  this lets you type only as much of the net address as required, using
  the local network components to fill in the rest
*/
static int
PartialIPAddress (const char *in, netadr_t *hostaddr)
{
	char       *buff;
	char       *b;
	unsigned    addr, mask, num, port, run;

	buff = nva (".%s", in);
	b = buff;
	if (buff[1] == '.')
		b++;

	addr = 0;
	mask = -1;
	while (*b == '.') {
		b++;
		num = 0;
		run = 0;
		while (!(*b < '0' || *b > '9')) {
			num = num * 10 + *b++ - '0';
			if (++run > 3)
				goto error;
		}
		if ((*b < '0' || *b > '9') && *b != '.' && *b != ':' && *b != 0)
			goto error;
		if (num > 255)
			goto error;
		mask <<= 8;
		addr = (addr << 8) + num;
	}

	if (*b++ == ':')
		port = atoi (b);
	else
		port = net_hostport;

	hostaddr->family = AF_INET;
	hostaddr->port = htons ((short) port);
	{
		int32_t     t = myAddr & htonl (mask);
		t |= htonl (addr);
		memcpy (hostaddr->ip, &t, ADDR_SIZE);
	}

	free (buff);
	return 0;
error:
	free (buff);
	return -1;
}

__attribute__((const)) int
UDP_Connect (int socket, netadr_t *addr)
{
	return 0;
}

int
UDP_CheckNewConnections (void)
{
#if defined (HAVE_IOCTL) || defined (_WIN32)
# ifdef _WIN64
	u_long      available;
# else
	int         available;
# endif
	AF_address_t from;
	socklen_t   fromlen = sizeof (from);
	char        buff[1];
#endif

	if (net_acceptsocket == -1)
		return -1;
#if defined (HAVE_IOCTL) || defined (_WIN32)
	if (ioctl (net_acceptsocket, FIONREAD, &available) == -1)
		Sys_Error ("UDP: ioctlsocket (FIONREAD) failed");
	if (available)
		return net_acceptsocket;
	// quietly absorb empty packets
	// there is no way to tell between an empty packet and no packets, but
	// as non-blocking io is used, this is not a problem.
	// we don't care about the interface on which the packet arrived
	recvfrom (net_acceptsocket, buff, 0, 0, &from.sa, &fromlen);
	return -1;
#else
	return net_acceptsocket;
#endif
}

static void
hex_dump_buf (byte *buf, int len)
{
	int         pos = 0, llen, i;

	while (pos < len) {
		llen = (len - pos < 16 ? len - pos : 16);
		printf ("%08x: ", pos);
		for (i = 0; i < llen; i++)
			printf ("%02x ", buf[pos + i]);
		for (i = 0; i < 16 - llen; i++)
			printf ("   ");
		printf (" | ");

		for (i = 0; i < llen; i++)
			printf ("%c", isprint (buf[pos + i]) ? buf[pos + i] : '.');
		for (i = 0; i < 16 - llen; i++)
			printf (" ");
		printf ("\n");
		pos += llen;
	}
}

int
UDP_Read (int socket, byte *buf, int len, netadr_t *from)
{
	int         ret;
	AF_address_t addr;
#ifdef HAVE_IN_PKTINFO
	char        ancillary[CMSG_SPACE (sizeof (struct in_pktinfo))];
	struct msghdr msghdr = {
		&addr,
		sizeof (addr),
		0, 0,
		ancillary,
		sizeof (ancillary),
		0
	};
	struct iovec iovec = {buf, len};
	struct cmsghdr *cmsg;
	struct in_pktinfo *info = 0;

	memset (&addr, 0, sizeof (addr));
	msghdr.msg_iov = &iovec;
	msghdr.msg_iovlen = 1;
	ret = recvmsg (socket, &msghdr, 0);
	if (ret == -1 && (errno == EWOULDBLOCK || errno == ECONNREFUSED))
		return 0;
	for (cmsg = CMSG_FIRSTHDR (&msghdr); cmsg;
		 cmsg = CMSG_NXTHDR (&msghdr, cmsg)) {
		Sys_MaskPrintf (SYS_net, "%d\n", cmsg->cmsg_type);
		if (cmsg->cmsg_type == IP_PKTINFO) {
			info = (struct in_pktinfo *) CMSG_DATA (cmsg);
			break;
		}
	}
	last_iface = 0;
	if (info) {
		/* Our iterface list is 0 based, but the pktinfo interface index is 1
		 * based.
		 */
		last_iface = &ifaces[info->ipi_ifindex - 1];
	}
	SockadrToNetadr (&addr, from);
	Sys_MaskPrintf (SYS_net, "got %d bytes from %s on iface %d (%s)\n", ret,
					UDP_AddrToString (from), info ? (int) info->ipi_ifindex - 1 : -1,
					last_iface ? inet_ntoa (info->ipi_addr) : "?");
#else
	socklen_t   addrlen = sizeof (AF_address_t);

	memset (&addr, 0, sizeof (addr));
	ret = recvfrom (socket, (void *) buf, len, 0, (struct sockaddr *) &addr,
					&addrlen);
	if (ret == -1 && (errno == EWOULDBLOCK || errno == ECONNREFUSED))
		return 0;
	SockadrToNetadr (&addr, from);
	Sys_MaskPrintf (SYS_net, "got %d bytes from %s\n", ret,
					UDP_AddrToString (from));
	last_iface = default_iface;
#endif
	if (developer & SYS_net) {
		hex_dump_buf (buf, ret);
	}
	return ret;
}

static int
UDP_MakeSocketBroadcastCapable (int socket)
{
	int         i = 1;

	// make this socket broadcast capable
	if (setsockopt (socket, SOL_SOCKET, SO_BROADCAST, (char *) &i,
					sizeof (i)) < 0)
		return -1;
	net_broadcastsocket = socket;

	return 0;
}

int
UDP_Broadcast (int socket, byte *buf, int len)
{
	int         ret;

	if (socket != net_broadcastsocket) {
		if (net_broadcastsocket != 0)
			Sys_Error ("Attempted to use multiple broadcasts sockets");
		ret = UDP_MakeSocketBroadcastCapable (socket);
		if (ret == -1) {
			Sys_Printf ("Unable to make socket broadcast capable\n");
			return ret;
		}
	}

	return UDP_Write (socket, buf, len, &broadcastaddr);
}

int
UDP_Write (int socket, byte *buf, int len, netadr_t *to)
{
	int         ret;
	AF_address_t addr;

	NetadrToSockadr (to, &addr);
	ret = sendto (socket, (const void *) buf, len, 0, &addr.sa,
				  SA_LEN (&addr.sa));
	if (ret == -1 && errno == EWOULDBLOCK)
		return 0;
	Sys_MaskPrintf (SYS_net, "sent %d bytes to %s\n", ret,
					UDP_AddrToString (to));
	if (developer & SYS_net) {
		hex_dump_buf (buf, len);
	}
	return ret;
}

const char *
UDP_AddrToString (netadr_t *addr)
{
	static dstring_t *buffer;

	if (!buffer)
		buffer = dstring_new ();

	dsprintf (buffer, "%d.%d.%d.%d:%d", addr->ip[0],
			  addr->ip[1], addr->ip[2], addr->ip[3],
			  ntohs (addr->port));
	return buffer->str;
}

int
UDP_GetSocketAddr (int socket, netadr_t *na)
{
	unsigned int a;
	socklen_t    addrlen = sizeof (AF_address_t);
	AF_address_t addr;

	memset (&addr, 0, sizeof (AF_address_t));

	getsockname (socket, &addr.sa, &addrlen);
	SockadrToNetadr (&addr, na);
	memcpy (&a, na->ip, ADDR_SIZE);
	if (a == 0 || a == inet_addr ("127.0.0.1")) {
		if (default_iface)
			memcpy (na->ip, default_iface, ADDR_SIZE);
		if (last_iface)
			memcpy (na->ip, last_iface, ADDR_SIZE);
	}

	return 0;
}

int
UDP_GetNameFromAddr (netadr_t *addr, char *name)
{
	struct hostent *hostentry;

	//FIXME the cast is an evil hack for android
	hostentry = gethostbyaddr ((char *) &addr->ip, ADDR_SIZE, AF_INET);

	if (hostentry) {
		strncpy (name, (char *) hostentry->h_name, NET_NAMELEN - 1);
		return 0;
	}

	strcpy (name, UDP_AddrToString (addr));
	return 0;
}

int
UDP_GetAddrFromName (const char *name, netadr_t *addr)
{
	struct hostent *hostentry;

	if (name[0] >= '0' && name[0] <= '9')
		return PartialIPAddress (name, addr);

	hostentry = gethostbyname (name);
	if (!hostentry)
		return -1;

	addr->family = AF_INET;
	addr->port = htons (net_hostport);
	memcpy (addr->ip, hostentry->h_addr_list[0], ADDR_SIZE);

	return 0;
}

__attribute__((pure)) int
UDP_AddrCompare (netadr_t *addr1, netadr_t *addr2)
{
	if (addr1->family != addr2->family)
		return -2;

	if (memcmp (addr1->ip, addr2->ip, ADDR_SIZE))
		return -1;

	if (addr1->port != addr2->port)
		return 1;

	return 0;
}

__attribute__((pure)) int
UDP_GetSocketPort (netadr_t *addr)
{
	return ntohs (addr->port);
}

int
UDP_SetSocketPort (netadr_t *addr, int port)
{
	addr->port = htons (port);
	return 0;
}
