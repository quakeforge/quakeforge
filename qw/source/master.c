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

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA
		
*/

		/* afternoon hack (james was here.)
		   FIXME: I coded this outside of the QF tree so it has non-quake
		   stuff like malloc and things... ie, it's POSIX, not quake. */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_UNISTD_H
# include "unistd.h"
#endif
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif

#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "protocol.h"

#if 0
#define S2C_CHALLENGE 'c'
#define S2M_HEARTBEAT 'a'
#define S2M_SHUTDOWN 'C'
#endif
#ifndef MASTER_TIMEOUT
# define MASTER_TIMEOUT 300
#endif

typedef struct {
	struct sockaddr_in addr;
	time_t updated;
} server_t;

int 
QW_AddHeartbeat (server_t **servers_p, int slen, struct sockaddr_in *addr, char *buf)
{
	server_t *servers = *servers_p;
	int freeslot = -1;
	int i;
	int sequence, players;

	sequence = atoi (buf + 2);
	players = atoi (strchr (buf + 2, '\n') + 1);

	for (i = 0; i < slen; i++) {
		if (servers[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr
			&& servers[i].addr.sin_port == addr->sin_port) {
			time (&servers[i].updated);
#if 1
			printf ("Refreshed %s:%d (seq %d, players %d)\n",
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
		newservers = realloc (servers, sizeof (server_t) * (slen + 50));
		if (!newservers) {
			return slen; // boo.
		}
		for (i = slen; i < slen + 50; i++) {
			newservers[i].updated = 0;
		}
		memcpy (newservers, servers, slen * sizeof (server_t));
		freeslot = slen;
		slen += 50;
		*servers_p = newservers;
	}
	servers[freeslot].addr = *addr;
#if 1
	printf ("Added %s:%d (seq %d, players %d)\n",
			inet_ntoa (servers[freeslot].addr.sin_addr),
			ntohs (servers[freeslot].addr.sin_port),
			sequence, players);
#endif
	time (&(servers[freeslot].updated));
	return slen;
}

void 
QW_TimeoutHearts (server_t *servers, int slen)
{
	time_t t;
	int i;
	time (&t);
	for (i = 0; i < slen; i++) {
		if (servers[i].updated != 0
			&& servers[i].updated < t - MASTER_TIMEOUT) {
			servers[i].updated = 0;
#if 1
			printf ("Removed %s:%d (timeout)\n",
				   inet_ntoa (servers[i].addr.sin_addr),
				   ntohs (servers[i].addr.sin_port));
#endif
		}
	}
}

void
QW_HeartShutdown (struct sockaddr_in *addr,server_t *servers, int slen) {
	int i;
	for (i = 0; i < slen; i++) {
		if (servers[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr
			&& servers[i].addr.sin_port == addr->sin_port) {
			servers[i].updated = 0;
#if 1
			printf ("Removed %s:%d\n",
				   inet_ntoa (servers[i].addr.sin_addr),
				   ntohs (servers[i].addr.sin_port));
#endif
			return;
		}
	}
}

void
QW_SendHearts (int sock, struct sockaddr_in *addr, 
			  server_t *servers, int serverlen) 
{
	unsigned char *out;
	int count, cpos;
	int i;

	count = 0;

	for (i = 0; i < serverlen; i++) {
		if (servers[i].updated != 0)
			count++;
	}
	out = malloc ((count + 1) * 6);
	cpos = 1;
	out[0] = out[1] = out[2] = out[3] = 0xff;
	out[4] = 0x64;
	out[5] = 0x0a;

	for (i = 0; i < serverlen; i++) {
		if (servers[i].updated != 0) {
			unsigned char *p = out + (cpos * 6);
			p[0] = ((unsigned char*) &servers[i].addr.sin_addr.s_addr)[0];
			p[1] = ((unsigned char*) &servers[i].addr.sin_addr.s_addr)[1];
			p[2] = ((unsigned char*) &servers[i].addr.sin_addr.s_addr)[2];
			p[3] = ((unsigned char*) &servers[i].addr.sin_addr.s_addr)[3];
			p[4] = (unsigned char) (ntohs(servers[i].addr.sin_port) >> 8);
			p[5] = (unsigned char) (ntohs(servers[i].addr.sin_port) & 0xFF);
			++cpos;
		}
	}
	sendto (sock, out, (count + 1) * 6, 0,
			(struct sockaddr*) addr, sizeof (struct sockaddr_in));
	free (out);
}

void
QW_Pong (int sock, struct sockaddr_in *addr)
{
	char data[6] = {0xFF, 0xFF, 0xFF, 0xFF, A2A_ACK, 0};
	printf ("ping\n");
	sendto (sock, data, sizeof (data), 0,
			(struct sockaddr*) addr, sizeof (struct sockaddr_in));
}
      

void 
QW_Master (struct sockaddr_in *addr)
{
	int sock;
	server_t *servers = malloc (sizeof (server_t) * 50);
	int serverlen = 50;
	int i;

	if (servers == NULL)
		return;

	for (i = 0; i < 50; i++) {
		servers[i].updated = 0;
	}

	sock = socket (AF_INET,SOCK_DGRAM, 0);
	if (sock < 0)
		return;

	if (bind (sock, (struct sockaddr*)addr, sizeof (struct sockaddr_in)))
		return;
   
	listen (sock, 4); // probably don't need this.
   
	while (1) {
		char buf[31];
		struct sockaddr_in recvaddr;
		int recvsize = sizeof (struct sockaddr_in);
		int size;

		size = recvfrom (sock, buf, sizeof (buf) - 1, 0,
			(struct sockaddr*) &recvaddr, &recvsize);
		if (size == -1) {
			printf ("recvfrom failed\n");
			continue;
		}
#if 0
		printf ("Got %d bytes: 0x%x from %s\n", size, buf[0],
				inet_ntoa (recvaddr.sin_addr));
#endif

#if 0
		printf ("Message Contents: '");
		{ // so that 'j' isn't unused when commented out
			int j;
			for (j = 0; j < size; j++)
				if (isprint (buf[j]))
					printf ("%c", buf[j]);
				else {
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
				QW_SendHearts (sock, &recvaddr, servers, serverlen);
				break;
			case S2M_HEARTBEAT:
				serverlen = QW_AddHeartbeat (&servers, serverlen, &recvaddr, buf);
				break;
			case S2M_SHUTDOWN:
				QW_HeartShutdown (&recvaddr, servers, serverlen);
				break;
			case A2A_PING:
				QW_Pong (sock, &recvaddr);
				break;
			default:
				printf ("Unknown 0x%x\n", buf[0]);
				break;
		}
	}
	free (servers);
}

int
main (int argc, char **argv)
{
	int c;
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons (27000);

	while ((c = getopt (argc, argv, "p:")) != -1) {
		if (c == 'p') {
			addr.sin_port = htons (atoi (optarg));
		}
	}
	QW_Master (&addr);
	return 0;
}
