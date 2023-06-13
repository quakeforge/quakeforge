/*
	net_wins.c

	Winsock lan driver.

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

#include <winsock2.h>
#include "winquake.h"

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/qargs.h"
#include "QF/sys.h"

#include "compat.h"
#include "netmain.h"

#define ADDR_SIZE 4

typedef union address {
	struct sockaddr_storage ss;
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

#define MAXHOSTNAMELEN		256

static int  net_acceptsocket = -1;		// socket for fielding new

										// connections
static int  net_controlsocket;
static int  net_broadcastsocket = 0;
static netadr_t broadcastaddr;

static unsigned long myAddr;

bool        winsock_lib_initialized;

int         (PASCAL FAR * pWSAStartup) (WORD wVersionRequired,

										LPWSADATA lpWSAData);
int         (PASCAL FAR * pWSACleanup) (void);
int         (PASCAL FAR * pWSAGetLastError) (void);

SOCKET (PASCAL FAR * psocket) (int af, int type, int protocol);
int         (PASCAL FAR * pioctlsocket) (SOCKET s, long cmd, u_long FAR * argp);
int         (PASCAL FAR * psetsockopt) (SOCKET s, int level, int optname,
										const char FAR * optval, int optlen);
int         (PASCAL FAR * precvfrom) (SOCKET s, char FAR * buf, int len,
									  int flags, struct sockaddr FAR * from,
									  int FAR * fromlen);
int         (PASCAL FAR * psendto) (SOCKET s, const char FAR * buf, int len,
									int flags, const struct sockaddr FAR * to,
									int tolen);
int         (PASCAL FAR * pclosesocket) (SOCKET s);
int         (PASCAL FAR * pgethostname) (char FAR * name, int namelen);
struct hostent FAR *(PASCAL FAR * pgethostbyname) (const char FAR * name);
struct hostent FAR *(PASCAL FAR * pgethostbyaddr) (const char FAR * addr,
												   int len, int type);
int         (PASCAL FAR * pgetsockname) (SOCKET s, struct sockaddr FAR * name,
										 int FAR * namelen);

#include "net_wins.h"

int         winsock_initialized = 0;
WSADATA     winsockdata;

//=============================================================================

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


static double blocktime;

#ifdef _WIN64
static INT_PTR PASCAL FAR
#else
static BOOL PASCAL FAR
#endif
BlockingHook (void)
{
	MSG         msg;
	BOOL        ret;

	if ((Sys_DoubleTime () - blocktime) > 2.0) {
		WSACancelBlockingCall ();
		return FALSE;
	}

	/* get the next message, if any */
	ret = (BOOL) PeekMessage (&msg, NULL, 0, 0, PM_REMOVE);

	/* if we got one, process it */
	if (ret) {
		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}

	/* TRUE if we got a message */
	return ret;
}


static void
WINS_GetLocalAddress (void)
{
	struct hostent *local = NULL;
	char        buff[MAXHOSTNAMELEN];
	unsigned long addr;

	if (myAddr != INADDR_ANY)
		return;

	if (pgethostname (buff, MAXHOSTNAMELEN) == SOCKET_ERROR)
		return;

	blocktime = Sys_DoubleTime ();
	WSASetBlockingHook (BlockingHook);
	local = pgethostbyname (buff);
	WSAUnhookBlockingHook ();
	if (local == NULL)
		return;

	myAddr = *(int *) local->h_addr_list[0];

	addr = ntohl (myAddr);
	snprintf (my_tcpip_address, sizeof (my_tcpip_address), "%ld.%ld.%ld.%ld",
			  (addr >> 24) & 0xff, (addr >> 16) & 0xff, (addr >> 8) & 0xff,
			  addr & 0xff);
}


int
WINS_Init (void)
{
	int         i;
	char        buff[MAXHOSTNAMELEN];
	char       *p;
	int         r;
	WORD        wVersionRequested;
	HINSTANCE   hInst;

// initialize the Winsock function vectors (we do this instead of statically linking
// so we can run on Win 3.1, where there isn't necessarily Winsock)
	hInst = LoadLibrary ("wsock32.dll");

	if (hInst == NULL) {
		Sys_Printf ("Failed to load winsock.dll\n");
		winsock_lib_initialized = false;
		return -1;
	}

	winsock_lib_initialized = true;

	pWSAStartup = (void *) GetProcAddress (hInst, "WSAStartup");
	pWSACleanup = (void *) GetProcAddress (hInst, "WSACleanup");
	pWSAGetLastError = (void *) GetProcAddress (hInst, "WSAGetLastError");
	psocket = (void *) GetProcAddress (hInst, "socket");
	pioctlsocket = (void *) GetProcAddress (hInst, "ioctlsocket");
	psetsockopt = (void *) GetProcAddress (hInst, "setsockopt");
	precvfrom = (void *) GetProcAddress (hInst, "recvfrom");
	psendto = (void *) GetProcAddress (hInst, "sendto");
	pclosesocket = (void *) GetProcAddress (hInst, "closesocket");
	pgethostname = (void *) GetProcAddress (hInst, "gethostname");
	pgethostbyname = (void *) GetProcAddress (hInst, "gethostbyname");
	pgethostbyaddr = (void *) GetProcAddress (hInst, "gethostbyaddr");
	pgetsockname = (void *) GetProcAddress (hInst, "getsockname");

	if (!pWSAStartup || !pWSACleanup || !pWSAGetLastError ||
		!psocket || !pioctlsocket || !psetsockopt ||
		!precvfrom || !psendto || !pclosesocket ||
		!pgethostname || !pgethostbyname || !pgethostbyaddr || !pgetsockname) {
		Sys_Printf ("Couldn't GetProcAddress from winsock.dll\n");
		return -1;
	}

	if (COM_CheckParm ("-noudp"))
		return -1;

	if (winsock_initialized == 0) {
		wVersionRequested = MAKEWORD (1, 1);

		r = pWSAStartup (wVersionRequested, &winsockdata);

		if (r) {
			Sys_Printf ("Winsock initialization failed.\n");
			return -1;
		}
	}
	winsock_initialized++;

	// determine my name
	if (pgethostname (buff, MAXHOSTNAMELEN) == SOCKET_ERROR) {
		Sys_MaskPrintf (SYS_net, "Winsock TCP/IP Initialization failed.\n");
		if (--winsock_initialized == 0)
			pWSACleanup ();
		return -1;
	}
	// if the quake hostname isn't set, set it to the machine name
	if (strcmp (hostname, "UNNAMED") == 0) {
		// see if it's a text IP address (well, close enough)
		for (p = buff; *p; p++)
			if ((*p < '0' || *p > '9') && *p != '.')
				break;

		// if it is a real name, strip off the domain; we want only the host
		if (*p) {
			for (i = 0; i < 15; i++)
				if (buff[i] == '.')
					break;
			buff[i] = 0;
		}
		Cvar_Set ("hostname", buff);
	}

	i = COM_CheckParm ("-ip");
	if (i) {
		if (i < com_argc - 1) {
			myAddr = inet_addr (com_argv[i + 1]);
			if (myAddr == INADDR_NONE)
				Sys_Error ("%s is not a valid IP address", com_argv[i + 1]);
			strcpy (my_tcpip_address, com_argv[i + 1]);
		} else {
			Sys_Error ("NET_Init: you must specify an IP address after -ip");
		}
	} else {
		myAddr = INADDR_ANY;
		strcpy (my_tcpip_address, "INADDR_ANY");
	}

	if ((net_controlsocket = WINS_OpenSocket (0)) == -1) {
		Sys_Printf ("WINS_Init: Unable to open control socket\n");
		if (--winsock_initialized == 0)
			pWSACleanup ();
		return -1;
	}

	{
		AF_address_t t;
		t.s4.sin_family = AF_INET;
		t.s4.sin_addr.s_addr = INADDR_BROADCAST;
		t.s4.sin_port = htons (net_hostport);
		SockadrToNetadr (&t, &broadcastaddr);
	}

	Sys_Printf ("Winsock TCP/IP Initialized\n");
	tcpipAvailable = true;

	return net_controlsocket;
}

//=============================================================================

void
WINS_Shutdown (void)
{
	WINS_Listen (false);
	WINS_CloseSocket (net_controlsocket);
	if (--winsock_initialized == 0)
		pWSACleanup ();
}

//=============================================================================

void
WINS_Listen (bool state)
{
	// enable listening
	if (state) {
		if (net_acceptsocket != -1)
			return;
		WINS_GetLocalAddress ();
		if ((net_acceptsocket = WINS_OpenSocket (net_hostport)) == -1)
			Sys_Error ("WINS_Listen: Unable to open accept socket");
		return;
	}
	// disable listening
	if (net_acceptsocket == -1)
		return;
	WINS_CloseSocket (net_acceptsocket);
	net_acceptsocket = -1;
}

//=============================================================================

int
WINS_OpenSocket (int port)
{
	int         newsocket;
	AF_address_t address;
	netadr_t    na;
	u_long      _true = 1;

	if ((newsocket = psocket (PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
		return -1;

	if (pioctlsocket (newsocket, FIONBIO, &_true) == -1)
		goto ErrorReturn;

	address.s4.sin_family = AF_INET;
	address.s4.sin_addr.s_addr = myAddr;
	address.s4.sin_port = htons ((unsigned short) port);
	if (bind (newsocket, &address.sa, SA_LEN (&address)) == 0)
		return newsocket;

	SockadrToNetadr (&address, &na);
	Sys_Error ("Unable to bind to %s", WINS_AddrToString (&na));
  ErrorReturn:
	pclosesocket (newsocket);
	return -1;
}

//=============================================================================

int
WINS_CloseSocket (int socket)
{
	if (socket == net_broadcastsocket)
		net_broadcastsocket = 0;
	return pclosesocket (socket);
}


//=============================================================================
/*
============
PartialIPAddress

this lets you type only as much of the net address as required, using
the local network components to fill in the rest
============
*/
static int
PartialIPAddress (const char *in, netadr_t *hostaddr)
{
	char        buff[256];
	char       *b;
	int         addr;
	int         num;
	int         mask;
	int         run;
	int         port;

	buff[0] = '.';
	b = buff;
	strcpy (buff + 1, in);
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
				return -1;
		}
		if ((*b < '0' || *b > '9') && *b != '.' && *b != ':' && *b != 0)
			return -1;
		if (num < 0 || num > 255)
			return -1;
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
	return 0;
}
//=============================================================================
static int WINS_Connect_called;
int
WINS_Connect (int socket, netadr_t *addr)
{
	WINS_Connect_called++;
	return 0;
}

//=============================================================================

int
WINS_CheckNewConnections (void)
{
	char        buf[4096];

	if (net_acceptsocket == -1)
		return -1;

	if (precvfrom (net_acceptsocket, buf, sizeof (buf),
				   MSG_PEEK, NULL, NULL) >= 0) {
		return net_acceptsocket;
	}
	return -1;
}

//=============================================================================

int
WINS_Read (int socket, byte *buf, int len, netadr_t *from)
{
	int         addrlen = sizeof (AF_address_t);
	int         ret;
	AF_address_t addr;

	ret = precvfrom (socket, (char *) buf, len, 0, &addr.sa, &addrlen);
	SockadrToNetadr (&addr, from);
	if (ret == -1) {
		int         err = pWSAGetLastError ();

		if (err == WSAEWOULDBLOCK || err == WSAECONNREFUSED)
			return 0;

	}
	return ret;
}

//=============================================================================

static int
WINS_MakeSocketBroadcastCapable (int socket)
{
	int         i = 1;

	// make this socket broadcast capable
	if (psetsockopt (socket, SOL_SOCKET, SO_BROADCAST, (char *) &i, sizeof (i))
		< 0)
		return -1;
	net_broadcastsocket = socket;

	return 0;
}

//=============================================================================

int
WINS_Broadcast (int socket, byte * buf, int len)
{
	int         ret;

	if (socket != net_broadcastsocket) {
		if (net_broadcastsocket != 0)
			Sys_Error ("Attempted to use multiple broadcasts sockets");
		WINS_GetLocalAddress ();
		ret = WINS_MakeSocketBroadcastCapable (socket);
		if (ret == -1) {
			Sys_Printf ("Unable to make socket broadcast capable\n");
			return ret;
		}
	}

	return WINS_Write (socket, buf, len, &broadcastaddr);
}

//=============================================================================

int
WINS_Write (int socket, byte *buf, int len, netadr_t *to)
{
	int         ret;
	AF_address_t addr;

	NetadrToSockadr (to, &addr);
	ret = psendto (socket, (char *) buf, len, 0, &addr.sa, SA_LEN (&addr));
	if (ret == -1)
		if (pWSAGetLastError () == WSAEWOULDBLOCK)
			return 0;

	return ret;
}

//=============================================================================

const char *
WINS_AddrToString (netadr_t *addr)
{
	static dstring_t *buffer;

	if (!buffer) {
		buffer = dstring_new ();
	}

	dsprintf (buffer, "%d.%d.%d.%d:%d", addr->ip[0],
			  addr->ip[1], addr->ip[2], addr->ip[3],
			  ntohs (addr->port));
	return buffer->str;
}

//=============================================================================

int
WINS_GetSocketAddr (int socket, netadr_t *addr)
{
	int         addrlen = sizeof (AF_address_t);
	AF_address_t ta;
	unsigned int a;

	memset (&ta, 0, sizeof (AF_address_t));

	pgetsockname (socket, &ta.sa, &addrlen);
	SockadrToNetadr (&ta, addr);
	memcpy (&a, addr->ip, ADDR_SIZE);
	if (a == 0 || a == inet_addr ("127.0.0.1"))
		memcpy (addr->ip, &myAddr, ADDR_SIZE);

	return 0;
}

//=============================================================================

int
WINS_GetNameFromAddr (netadr_t *addr, char *name)
{
	struct hostent *hostentry;

	hostentry = pgethostbyaddr ((char *) addr->ip, ADDR_SIZE, AF_INET);

	if (hostentry) {
		strncpy (name, (char *) hostentry->h_name, NET_NAMELEN - 1);
		return 0;
	}

	strcpy (name, WINS_AddrToString (addr));
	return 0;
}

//=============================================================================

int
WINS_GetAddrFromName (const char *name, netadr_t *addr)
{
	struct hostent *hostentry;

	if (name[0] >= '0' && name[0] <= '9')
		return PartialIPAddress (name, addr);

	hostentry = pgethostbyname (name);
	if (!hostentry)
		return -1;

	addr->family = AF_INET;
	addr->port = htons ((unsigned short) net_hostport);
	memcpy (addr->ip, hostentry->h_addr_list[0], ADDR_SIZE);

	return 0;
}

//=============================================================================

int
WINS_AddrCompare (netadr_t *addr1, netadr_t *addr2)
{
	if (addr1->family != addr2->family)
		return -1;

	if (memcmp (addr1->ip, addr2->ip, ADDR_SIZE))
		return -1;

	if (addr1->port != addr2->port)
		return 1;

	return 0;
}

//=============================================================================

int
WINS_GetSocketPort (netadr_t *addr)
{
	return ntohs (addr->port);
}


int
WINS_SetSocketPort (netadr_t *addr, int port)
{
	addr->port = htons ((unsigned short) port);
	return 0;
}

//=============================================================================
