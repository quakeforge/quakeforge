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

*/

#ifndef _NET_H
#define _NET_H

#include "QF/cvar.h"
#include "QF/msg.h"
#include "QF/qdefs.h"
#include "QF/sizebuf.h"

/** \defgroup network Network support
*/

/** \defgroup qw-net QuakeWorld network support.
	\ingroup network
*/
///@{
#define MAX_MSGLEN		1450		///< max length of a reliable message
#define MAX_DATAGRAM	1450		///< max length of unreliable message

#define	PORT_ANY	-1

typedef struct
{
#ifdef HAVE_IPV6
	byte        ip[16];
#else
	byte        ip[4];
#endif
	unsigned short	port;
	unsigned short	family;
} netadr_t;

extern	int		net_socket;
extern	netadr_t	net_local_adr;
extern	netadr_t	net_loopback_adr;
extern	netadr_t	net_from;		// address of who sent the packet
extern	struct msg_s *net_message;

extern	struct cvar_s	*qport;

int Net_Log_Init (const char **sound_precache);
void Net_LogPrintf (const char *fmt, ...) __attribute__ ((format (PRINTF, 1, 2)));
void Log_Incoming_Packet (const byte *p, int len, int has_sequence,
						  int is_server);
void Log_Outgoing_Packet (const byte *p, int len, int has_sequence,
						  int is_server);
void Net_LogStop (void *data);
void Analyze_Client_Packet (const byte * data, int len, int has_sequence);
void Analyze_Server_Packet (const byte * data, int len, int has_sequence);

extern struct cvar_s *net_packetlog;
///@}

/** \defgroup qw-udp QuakeWorld udp support.
	\ingroup qw-net
*/
///@{

/** Initialize the UDP network interface.

	Opens a single socket to be used for all communications.

	\param port		The port to which the socket will be bound.
*/
void NET_Init (int port);

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
qboolean NET_CompareAdr (netadr_t a, netadr_t b) __attribute__((const));

/** Compare two network addresses.

	Ignore the port number.

	\param a		The first address to compare.
	\param b		The second address to compare.
	\return			True of the addresses match, otherwise false.
*/
qboolean NET_CompareBaseAdr (netadr_t a, netadr_t b) __attribute__((const));

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

	\warning	The return value is a pointer to a static buffer. The returned
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

///@}

/** \defgroup netchan Netchan
	\ingroup qw-net

	<table>
	<tr><td colspan=2>Packet Header</td></tr>
	<tr><td>Bits</td>	<td>Meaning</td></tr>
	<tr><td>31</td>		<td>sequence</td></tr>
	<tr><td>1</td>		<td>this message contains a reliable payload</td></tr>
	<tr><td>31</td>		<td>acknowledge sequence</td></tr>
	<tr><td>1</td>		<td>acknowledge receipt of even/odd message</td></tr>
	<tr><td>16</td>		<td>qport</td></tr>
	</table>

	The remote connection never knows if it missed a reliable message, the
	local side detects that it has been dropped by seeing a sequence
	acknowledge higher than the last reliable sequence, but without the
	correct even/odd bit for the reliable set.

	If the sender notices that a reliable message has been dropped, it will
	be retransmitted.  It will not be retransmitted again until a message
	after the retransmit has been acknowledged and the reliable still failed
	to get there.

	If the sequence number and reliable payload bits are all 1 (32bit -1),
	the packet is an out-of-band packet and should be handled without a
	netcon.

	The reliable message can be added to at any time by doing
	MSG_Write* (&netchan->message, data).

	If the message buffer is overflowed, either by a single message, or by
	multiple frames worth piling up while the last reliable transmit goes
	unacknowledged, the netchan signals a fatal error.

	Reliable messages are always placed first in a packet, then the unreliable
	message is included if there is sufficient room.

	To the receiver, there is no distinction between the reliable and
	unreliable parts of the message, they are just processed out as a single
	larger message.

	Illogical packet sequence numbers cause the packet to be dropped, but do
	not kill the connection.  This, combined with the tight window of valid
	reliable acknowledgement numbers provides protection against malicious
	address spoofing.

	The qport field is a workaround for bad address translating routers that
	sometimes remap the client's source port on a packet during gameplay.

	If the base part of the net address matches and the qport matches, then
	the channel matches even if the IP port differs.  The IP port should be
	updated to the new value before sending out any replies.
*/
///@{
#define	OLD_AVG		0.99		// total = oldtotal*OLD_AVG + new*(1-OLD_AVG)

#define	MAX_LATENT	32

typedef enum {
	NC_QPORT_SEND = 0x01,
	NC_QPORT_READ = 0x02,
} ncqport_e;

typedef struct netchan_s {
	qboolean	fatal_error;	///< True if the message overflowed

	double      last_received;	///< Time the last packet was received.

	/// \name statistics
	/// the statistics are cleared at each client begin, because
	/// the server connection process gives a bogus picture of the data
	///@{
	float		frame_latency;		///< rolling average
	float		frame_rate;

	int			drop_count;			///< dropped packets, cleared each level
	int			net_drop;			///< packets dropped before this one
	int			good_count;			///< cleared each level
	///@}

	netadr_t	remote_address;
	int			qport;
	ncqport_e	flags;

	/// \name bandwidth estimator
	///@{
	double		cleartime;			///< if realtime > nc->cleartime,
									///< free to go
	double		rate;				///< seconds / byte
	///@}

	/// \name sequencing variables
	///@{
	int			incoming_sequence;
	int			incoming_acknowledged;
	int			incoming_reliable_acknowledged;	///< single bit

	int			incoming_reliable_sequence;	///< single bit, maintained local

	int			outgoing_sequence;
	int			reliable_sequence;			///< single bit
	int			last_reliable_sequence;		///< sequence number of last send
	///@}

	/// \name reliable staging and holding areas
	///@{
	sizebuf_t	message;		///< writing buffer to send to server
	byte		message_buf[MAX_MSGLEN];

	int			reliable_length;
	byte		reliable_buf[MAX_MSGLEN];	///< unacked reliable message
	///@}

	/// \name time and size data to calculate bandwidth
	///@{
	int			outgoing_size[MAX_LATENT];
	double		outgoing_time[MAX_LATENT];
	///@}
} netchan_t;

/** Disable packet choking.
*/
extern	int net_nochoke;

/** Disable packet sending.

	Used by clients in demo playback mode.
*/
extern	int net_blocksend;

/** Pointer to variable holding the current time in seconds.
*/
extern	double *net_realtime;

/** Callback to log outgoing packets
*/
extern void (*net_log_packet) (int length, const void *data, netadr_t to);

/** Initialize the netchan system.

	Currently only sets the qport cvar default to a random value.
*/
void Netchan_Init (void);

/** Initialize the netchan cvars.
*/
void Netchan_Init_Cvars (void);

/** Try to send an unreliable packet to a connection.

	Handles transmission or retransmission of the reliable packet.

	0 length will still generate a packet and deal with the reliable messages.

	\param chan		The netchan representing the connection.
	\param length	The size of the unreliable packet.
	\param data		The data of the unreliable packet.
*/
void Netchan_Transmit (netchan_t *chan, unsigned length, byte *data);

/** Send an out-of-band packet.

	\param adr		The address to which the data will be sent.
	\param length	The length of the data to be sent.
	\param data		The data to be sent.
*/
void Netchan_OutOfBand (netadr_t adr, unsigned length, byte *data);
/** Send a formatted string as an out-of-band packet.

	\param adr		The address to which the data will be sent.
	\param format	The printf style format string.
*/
void Netchan_OutOfBandPrint (netadr_t adr, const char *format, ...)
	__attribute__ ((format (PRINTF,2,3)));

/** Process a packet for the specifiied connection.

	Called when the current net_message is from remote_address.
	Modifies net_message so that it points to the packet payload.

	\param chan		The netchan representing the connection.
*/
qboolean Netchan_Process (netchan_t *chan);

/** Initialize a new connection.

	\param chan		The netchan representing the connection.
	\param adr		The address of the remote end of the connection.
	\param qport	The qport associated with the connection.
	\param flags	Control of the sending/reading of the qport on this
					connection.
*/
void Netchan_Setup (netchan_t *chan, netadr_t adr, int qport, ncqport_e flags);

/** Check if a packet can be sent to the connection.

	\param chan     The netchan representing the connection.
	\return			True if the connection isn't chocked.
*/
qboolean Netchan_CanPacket (netchan_t *chan) __attribute__((pure));

/** Check if a reliable packet can be sent to the connection.

	\param chan     The netchan representing the connection.
	\return			True if there is no outstanding reliable packet and the
					connection isn't chocked.
*/
qboolean Netchan_CanReliable (netchan_t *chan) __attribute__((pure));

/** Send a packet.

	Very raw. Just calls NET_SendPacket().
	\param length	The length of the data to be sent.
	\param data		The data to be sent.
	\param to		The address to which the data will be sent.
*/
void Netchan_SendPacket (int length, const void *data, netadr_t to);

///@}

#endif // _NET_H
