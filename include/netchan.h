/*
	netchan.h

	quake's interface to the networking layer

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

	$Id$
*/

#ifndef _NET_H
#define _NET_H

#include "QF/cvar.h"
#include "QF/msg.h"
#include "QF/qdefs.h"
#include "QF/sizebuf.h"

#define MAX_MSGLEN		1450		// max length of a reliable message
#define MAX_DATAGRAM	1450		// max length of unreliable message

#define	PORT_ANY	-1

typedef struct
{
#ifdef HAVE_IPV6
	byte	ip[16];
#else
	byte	ip[4];
#endif
	unsigned short	port;
	unsigned short	family;	// used to be pad, before IPV6
} netadr_t;

extern	netadr_t	net_local_adr;
extern	netadr_t	net_loopback_adr;
extern	netadr_t	net_from;		// address of who sent the packet
extern	struct msg_s *net_message;

extern	struct cvar_s	*hostname;
extern	struct cvar_s	*qport;

extern	int		net_socket;

/** Initialize the UDP network interface.

	Opens a single socket to be used for all communications.

	\param port		The port to which the socket will be bound.
*/
void NET_Init (int port);

/** Shutdown the UDP network interface.
*/
void NET_Shutdown (void);

/** Read a single packet from the network into net_message.

	\return			True if successfully read, otherwise false.
*/
qboolean NET_GetPacket (void);

/**	Send a data packet out to the network.

	\param length	The length of the data to be sent.
	\param data		The data to be sent.
	\param to		The address to which the data will be sent.
*/
void NET_SendPacket (int length, const void *data, netadr_t to);

/** Compare two network addresses.

	Compare the port number too.

	\param a		The first address to compare.
	\param b		The second address to compare.
	\return			True of the addresses match, otherwise false.
*/
qboolean NET_CompareAdr (netadr_t a, netadr_t b);

/** Compare two network addresses.

	Ignore the port number.

	\param a		The first address to compare.
	\param b		The second address to compare.
	\return			True of the addresses match, otherwise false.
*/
qboolean NET_CompareBaseAdr (netadr_t a, netadr_t b);

/** Convert an address to a string.

	Include the port number in the string.

	\warning	The return value is a pointer to a static buffer. The returned
				string must be saved if mixing calls with different addresses.

	\param a		The address to convert
	\return			The address in human readable form.
*/
const char *NET_AdrToString (netadr_t a);

/** Convert an address to a string.

	Do not include the port number in the string.

	\note	The return value is a pointer to a static buffer. The returned
			string must be saved if mixing calls with different addresses.

	\param a		The address to convert
	\return			The address in human readable form.
*/
const char *NET_BaseAdrToString (netadr_t a);

/** Convert a human readable address to a quake address.

	Accepts both host names (full or partial) and numeric form.

	If a port is specified, the port value in the address will be set to that
	value, otherwise it will be set to 0.

	\param[in] s	The human readable address to be converted.
	\param[out] a	The resulting address of the conversion.
	\return			True if the conversion is successful, otherwise false.
*/
qboolean NET_StringToAdr (const char *s, netadr_t *a);

int Net_Log_Init (const char **sound_precache);
void Net_LogPrintf (const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void Log_Incoming_Packet (const byte *p, int len, int has_sequence);
void Log_Outgoing_Packet (const byte *p, int len, int has_sequence);
void Net_LogStop (void);
void Analyze_Client_Packet (const byte * data, int len, int has_sequence);
void Analyze_Server_Packet (const byte * data, int len, int has_sequence);

extern struct cvar_s *net_packetlog;

extern qboolean is_server;
qboolean ServerPaused (void);

//============================================================================

#define	OLD_AVG		0.99		// total = oldtotal*OLD_AVG + new*(1-OLD_AVG)

#define	MAX_LATENT	32

typedef struct netchan_s {
	qboolean	fatal_error;

	float		last_received;		// for timeouts

// the statistics are cleared at each client begin, because
// the server connecting process gives a bogus picture of the data
	float		frame_latency;		// rolling average
	float		frame_rate;

	int			drop_count;			// dropped packets, cleared each level
	int			net_drop;			//packets dropped before this one
	int			good_count;			// cleared each level

	netadr_t	remote_address;
	int			qport;
	int			flags;

// bandwidth estimator
	double		cleartime;			// if realtime > nc->cleartime, free to go
	double		rate;				// seconds / byte

// sequencing variables
	int			incoming_sequence;
	int			incoming_acknowledged;
	int			incoming_reliable_acknowledged;	// single bit

	int			incoming_reliable_sequence;		// single bit, maintained local

	int			outgoing_sequence;
	int			reliable_sequence;			// single bit
	int			last_reliable_sequence;		// sequence number of last send

// reliable staging and holding areas
	sizebuf_t	message;		// writing buffer to send to server
	byte		message_buf[MAX_MSGLEN];

	int			reliable_length;
	byte		reliable_buf[MAX_MSGLEN];	// unacked reliable message

// time and size data to calculate bandwidth
	int			outgoing_size[MAX_LATENT];
	double		outgoing_time[MAX_LATENT];
} netchan_t;

extern	int net_nochoke;	// don't choke packets
extern	int net_blocksend;	// don't send packets (used by client for demos)
extern	double *net_realtime;

void Netchan_Init (void);
void Netchan_Init_Cvars (void);
void Netchan_Transmit (netchan_t *chan, int length, byte *data);
void Netchan_OutOfBand (netadr_t adr, int length, byte *data);
void Netchan_OutOfBandPrint (netadr_t adr, const char *format, ...) __attribute__((format(printf,2,3)));
qboolean Netchan_Process (netchan_t *chan);
void Netchan_Setup (netchan_t *chan, netadr_t adr, int qport, int flags);

#define NC_SEND_QPORT	0x01
#define NC_READ_QPORT	0x02

qboolean Netchan_CanPacket (netchan_t *chan);
qboolean Netchan_CanReliable (netchan_t *chan);

void Netchan_SendPacket (int length, const void *data, netadr_t to);

#endif // _NET_H
