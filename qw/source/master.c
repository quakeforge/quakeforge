/*
	master.c

	Quakeworld master server

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

		eree Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
		/* afternoon hack (james was here.)
		   FIXME: I coded this outside of the QF tree so it has non-quake
		   stuff like malloc and things... ie, it's POSIX, not quake. */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_WINDOWS_H
# include "winquake.h"
#endif
#ifdef HAVE_UNISTD_H
# include "unistd.h"
#endif
#include <sys/types.h>
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
#ifdef HAVE_WINSOCK_H
# include <winsock.h>
#endif
#ifdef HAVE_NETDB_H
# include <netdb.h>
#endif
#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#endif

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <getopt.h>
#include <stdarg.h>

#include "qw/protocol.h"

#ifdef HAVE_IN_PKTINFO
# ifndef SOL_IP		// BSD-based stacks don't define this.
#  define SOL_IP IPPROTO_IP
# endif
#endif

static void __attribute__ ((format (PRINTF, 1, 2)))
ma_log (const char *fmt, ...);

#ifdef HAVE_IN_PKTINFO
static int
qf_sendmsg (int sock, void *data, size_t len, struct msghdr *msghdr)
{
	struct iovec iovec = {data, len};
	msghdr->msg_iov = &iovec;
	msghdr->msg_iovlen = 1;
	return sendmsg (sock, msghdr, 0);
}

static int
qf_recvmsg (int sock, void *data, size_t len, struct msghdr *msghdr)
{
	struct iovec iovec = {data, len};
	msghdr->msg_iov = &iovec;
	msghdr->msg_iovlen = 1;
	return recvmsg (sock, msghdr, 0);
}

static int
qf_initsocket (int sock)
{
	int ip_pktinfo = 1;
	if (setsockopt (sock, SOL_IP, IP_PKTINFO, &ip_pktinfo,
					sizeof (ip_pktinfo)) == -1) {
		perror ("setsockopt");
		return -1;
	}
	return 0;
}

#define MSGHDR	\
	char ancillary[CMSG_SPACE (sizeof (struct in_pktinfo))];	\
	struct sockaddr_in recvaddr;								\
	struct msghdr msghdr = {									\
		&recvaddr,												\
		sizeof (recvaddr),										\
		0, 0,													\
		ancillary,												\
		sizeof (ancillary),										\
		0														\
	}

typedef struct msghdr msghdr_t;

#else
static int
qf_sendmsg (int sock, void *data, size_t len, struct sockaddr_in *addr)
{
	return sendto (sock, data, len, 0, (struct sockaddr *)addr, sizeof (*addr));
}

static int
qf_recvmsg (int sock, void *data, size_t len, struct sockaddr_in *addr)
{
#ifdef HAVE_SOCKLEN_T
	socklen_t   addr_len = sizeof (*addr);
#else
	int         addr_len = sizeof (*addr);
#endif
	return recvfrom (sock, data, len, 0, (struct sockaddr *)addr, &addr_len);
}

static int
qf_initsocket (int sock)
{
	return 0;
}

#define MSGHDR	 struct sockaddr_in msghdr
#define recvaddr msghdr

typedef struct sockaddr_in msghdr_t;

#endif

#define MASTER_TIMEOUT 600
#define SLIST_MULTIPLE 1

typedef struct {
	struct		sockaddr_in addr;
	time_t		updated;
	bool		notimeout;
} server_t;

static int
QW_AddHeartbeat (server_t **servers_p, int slen,
				 struct sockaddr_in *addr, const char *buf, bool notimeout)
{
	server_t *servers = *servers_p;
	int freeslot = -1;
	int i;
	int sequence, players;
	char *c;

	sequence = atoi (buf + 2);
	c = strchr (buf + 2, '\n');
	players = c ? atoi (c + 1) : 0;

	for (i = 0; i < slen; i++) {
		if (servers[i].updated
			&& servers[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr
			&& servers[i].addr.sin_port == addr->sin_port) {
			time (&servers[i].updated);
#if 1
			ma_log ("Refreshed %s:%d (seq %d, players %d)\n",
					inet_ntoa (servers[i].addr.sin_addr),
					ntohs (servers[i].addr.sin_port),
					sequence, players);
#endif
			return slen;
		} else if (freeslot == -1 && servers[i].updated == 0) {
			freeslot = i;
		}
	}
	if (freeslot == -1) {
		server_t *newservers;
		newservers = realloc (servers, sizeof (server_t)
									   * (slen + SLIST_MULTIPLE));
		if (!newservers) {
			printf ("realloc failed\n");
			return slen; // boo.
		}

		for (i = slen; i < slen + SLIST_MULTIPLE; i++)
			memset (&newservers[i], 0, sizeof (newservers[i]));

		freeslot = slen;
		slen += SLIST_MULTIPLE;
		*servers_p = newservers;
		servers = *servers_p;
	}
	servers[freeslot].addr = *addr;
	servers[freeslot].notimeout = notimeout;
#if 1
	ma_log ("Added %s:%d (seq %d, players %d)\n",
			inet_ntoa (servers[freeslot].addr.sin_addr),
			ntohs (servers[freeslot].addr.sin_port),
			sequence, players);
#endif
	time (&(servers[freeslot].updated));
	return slen;
}

static void
QW_TimeoutHearts (server_t *servers, int slen)
{
	time_t t;
	int i;
	time (&t);
	for (i = 0; i < slen; i++) {
		if (servers[i].updated != 0
			&& servers[i].updated < t - MASTER_TIMEOUT
			&& !servers[i].notimeout) {
			servers[i].updated = 0;
#if 1
			ma_log ("Removed %s:%d (timeout)\n",
				   inet_ntoa (servers[i].addr.sin_addr),
				   ntohs (servers[i].addr.sin_port));
#endif
		}
	}
}

static void
QW_HeartShutdown (struct sockaddr_in *addr, server_t *servers,
				  int slen)
{
	int i;
	for (i = 0; i < slen; i++) {
		if (servers[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr
			&& servers[i].addr.sin_port == addr->sin_port) {
			servers[i].updated = 0;
#if 1
			ma_log ("Removed %s:%d\n",
				   inet_ntoa (servers[i].addr.sin_addr),
				   ntohs (servers[i].addr.sin_port));
#endif
			return;
		}
	}
}

static void
QW_SendHearts (int sock, msghdr_t *msghdr, server_t *servers, int serverlen)
{
	unsigned char *out;
	int count, cpos;
	int i;

	count = 0;

	for (i = 0; i < serverlen; i++)
		if (servers[i].updated != 0)
			count++;

	out = malloc ((count + 1) * 6);
	if (!out) {
		printf ("malloc failed\n");
		return;
	}
	cpos = 1;
	out[0] = out[1] = out[2] = out[3] = 0xff;	// connectionless packet
	out[4] = M2C_MASTER_REPLY;
	out[5] = '\n';

	for (i = 0; i < serverlen; i++) {
		if (servers[i].updated != 0) {
			unsigned char *p = out + (cpos * 6);
			in_addr_t   addr = ntohl (servers[i].addr.sin_addr.s_addr);
			in_port_t   port = ntohs (servers[i].addr.sin_port);
			p[0] = addr >> 24;
			p[1] = addr >> 16;
			p[2] = addr >>  8;
			p[3] = addr >>  0;
			p[4] = port >>  8;
			p[5] = port >>  0;
			++cpos;
		}
	}
	qf_sendmsg (sock, out, (count + 1) * 6, msghdr);
	free (out);
}

static void
QW_Pong (int sock, msghdr_t *msghdr)
{
	// connectionless packet
	char data[6] = {0xFF, 0xFF, 0xFF, 0xFF, A2A_ACK, 0};
	//ma_log ("Ping\n");
	qf_sendmsg (sock, data, sizeof (data), msghdr);
}

int serverlen = SLIST_MULTIPLE;
server_t *servers;

static void
QW_Master (struct sockaddr_in *addr)
{
	int sock;

	if (!servers) {
		printf ("initial malloc failed\n");
		return;
	}

	sock = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		printf ("socket failed\n");
		return;
	}

	if (qf_initsocket (sock) == -1)
		return;

	if (bind (sock, (struct sockaddr *) addr, sizeof (struct sockaddr_in))) {
		printf ("bind failed\n");
		return;
	}

	while (1) {
		int size;
		char buf[31];
		MSGHDR;

		buf[30] = '\0'; // a sentinal for string ops

		size = qf_recvmsg (sock, buf, sizeof (buf) - 1, &msghdr);
		if (size == -1) {
			perror ("qf_recvmsg failed\n");
			continue;
		}
#if 0
		{
			/* Dump the destination information (ie, our iface/address) of
			 * the packet. Mostly so I know how this stuff works.
			 */
			struct cmsghdr *cmsg;

			for (cmsg = CMSG_FIRSTHDR (&msghdr); cmsg;
				 cmsg = CMSG_NXTHDR (&msghdr, cmsg)) {
				struct in_pktinfo *d;
				d = (struct in_pktinfo *) CMSG_DATA (cmsg);
				printf ("iface %d %s %s\n", d->ipi_ifindex,
						inet_ntoa(d->ipi_spec_dst), inet_ntoa (d->ipi_addr));
			}
		}
#endif
#if 0
		ma_log ("Got %d bytes: 0x%x from %s\n", size, buf[0],
				inet_ntoa (recvaddr.sin_addr));
#endif

#if 0
		ma_log ("Message Contents: '");
		{ // so that 'j' isn't unused when commented out
			int j;
			for (j = 0; j < size; j++)
				if (isprint (buf[j]))
					printf ("%c", buf[j]);
				else
					switch (buf[j]) {
						case '\n': printf ("\\n"); break;
						case '\0': printf ("\\0"); break;
						default:   printf ("(%02x)", buf[j]);
					}
		}
		printf ("'\n");
#endif

		QW_TimeoutHearts (servers, serverlen);
		switch (buf[0]) {
			case S2C_CHALLENGE:
				QW_SendHearts (sock, &msghdr, servers, serverlen);
				break;
			case S2M_HEARTBEAT:
				serverlen = QW_AddHeartbeat (&servers, serverlen,
											 &recvaddr, buf, false);
				break;
			case S2M_SHUTDOWN:
				QW_HeartShutdown (&recvaddr, servers, serverlen);
				break;
			case A2A_PING:
				QW_Pong (sock, &msghdr);
				break;
			default:
				ma_log ("Unknown 0x%x\n", buf[0]);
				break;
		}
	}
	free (servers);
}

static int
make_host_addr (const char *host, int port, struct sockaddr_in *host_addr)
{
	struct hostent *host_ent;

	host_ent = gethostbyname (host);
	if (!host_ent)
		return -1;
	host_addr->sin_family = AF_INET;
	memcpy (&host_addr->sin_addr, host_ent->h_addr_list[0],
			sizeof (host_addr->sin_addr));
	host_addr->sin_port = htons (port);
	return 0;
}

static void
read_hosts (const char *fname)
{
	FILE       *host_file;
	int         host_port;
	char        host_name[256];
	static const char *fake_heartbeat = "      ";
	char       *buf;
	struct sockaddr_in host_addr;

	host_file = fopen (fname, "rt");
	if (!host_file) {
		fprintf (stderr, "could not open `%s'\n", fname);
		return;
	}
	while (fgets (host_name, sizeof (host_name), host_file)) {
		for (buf = host_name; *buf && !isspace ((byte)*buf) && *buf !=':';
			 buf++)
			;
		if (*buf)
			*buf++ = 0;
		if (*buf)
			host_port = atoi (buf);
		else
			host_port = PORT_SERVER;
		if (make_host_addr (host_name, host_port, &host_addr) == -1) {
			fprintf (stderr, "could not resolve `%s', skipping", host_name);
			continue;
		}
		serverlen = QW_AddHeartbeat (&servers, serverlen, &host_addr,
									 fake_heartbeat, true);
	}
	fclose (host_file);
}

static int
net_init (void)
{
#ifdef _WIN32
	int i;
	WSADATA winsockdata;

	i = WSAStartup (MAKEWORD (1, 1), &winsockdata);
	if (i) {
		printf ("Winsock initialization failed.\n");
		return 0;
	}
#endif
	return 1;
}

int
main (int argc, char **argv)
{
	struct sockaddr_in addr;
	int c;

	if (!net_init ())
		return 1;

	servers = calloc (sizeof (server_t), serverlen);

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons (PORT_MASTER);

	while ((c = getopt (argc, argv, "b:p:f:")) != -1) {
		switch (c) {
			case 'b':
				c = inet_addr (optarg);
				if (c == -1) {
					fprintf (stderr, "bad bind address %s\n", optarg);
					return 1;
				}
				addr.sin_addr.s_addr = c;
				break;
			case 'f':
				read_hosts (optarg);
				break;
			case 'p':
				addr.sin_port = htons (atoi (optarg));
				break;
		}
	}

	QW_Master (&addr);
	return 0;
}

static void __attribute__ ((format (PRINTF, 1, 2)))
ma_log (const char *fmt, ...)
{
	va_list     args;
	time_t      mytime = 0;
	struct tm  *local = NULL;
	char        stamp[123];

	mytime = time (NULL);
	local = localtime (&mytime);
#ifdef _WIN32
	strftime (stamp, sizeof (stamp), "[%b %d %X] ", local);
#else
	strftime (stamp, sizeof (stamp), "[%b %e %X] ", local);
#endif
	fprintf (stdout, "%s", stamp);
	va_start (args, fmt);
	vfprintf (stdout, fmt, args);
	va_end (args);
	fflush (stdout);
}
