/*
	net_chan.c

	(description)

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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdarg.h>
#include <time.h>

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/msg.h"
#include "QF/sys.h"

#include "compat.h"
#include "netchan.h"

#include "../qw/include/client.h"

#define	PACKET_HEADER	8

/*
  packet header
  -------------
  31	sequence
  1	does this message contain a reliable payload
  31	acknowledge sequence
  1	acknowledge receipt of even/odd message
  16  qport

  The remote connection never knows if it missed a reliable message, the
  local side detects that it has been dropped by seeing a sequence acknowledge
  higher thatn the last reliable sequence, but without the correct evon/odd
  bit for the reliable set.

  If the sender notices that a reliable message has been dropped, it will be
  retransmitted.  It will not be retransmitted again until a message after the
  retransmit has been acknowledged and the reliable still failed to get there.

  if the sequence number is -1, the packet should be handled without a netcon

  The reliable message can be added to at any time by doing
  MSG_Write* (&netchan->message, <data>).

  If the message buffer is overflowed, either by a single message, or by
  multiple frames worth piling up while the last reliable transmit goes
  unacknowledged, the netchan signals a fatal error.

  Reliable messages are always placed first in a packet, then the unreliable
  message is included if there is sufficient room.

  To the receiver, there is no distinction between the reliable and unreliable
  parts of the message, they are just processed out as a single larger message.

  Illogical packet sequence numbers cause the packet to be dropped, but do
  not kill the connection.  This, combined with the tight window of valid
  reliable acknowledgement numbers provides protection against malicious
  address spoofing.

  The qport field is a workaround for bad address translating routers that
  sometimes remap the client's source port on a packet during gameplay.

  If the base part of the net address matches and the qport matches, then the
  channel matches even if the IP port differs.  The IP port should be updated
  to the new value before sending out any replies.
*/

int         net_nochoke;
int         net_blocksend;
double     *net_realtime;
cvar_t     *showpackets;
cvar_t     *showdrop;
cvar_t     *qport;


void
Netchan_Init (void)
{
	int         port;

	// pick a port value that should be nice and random
	port = Sys_TimeID ();

	Cvar_SetValue (qport, port);
}

void
Netchan_Init_Cvars (void)
{
	showpackets = Cvar_Get ("showpackets", "0", CVAR_NONE, NULL,
							"Show all network packets");
	showdrop = Cvar_Get ("showdrop", "0", CVAR_NONE, NULL, "Toggle the "
						 "display of how many packets you are dropping");
	qport = Cvar_Get ("qport", "0", CVAR_NONE, NULL, "The internal port "
					  "number for the game networking code. Useful for "
					  "clients who use multiple connections through one "
					  "IP address (NAT/IP-MASQ) because default port is "
					  "random.");
}

/*
	Netchan_OutOfBand

	Sends an out-of-band datagram
*/
void
Netchan_OutOfBand (netadr_t adr, int length, byte * data)
{
	byte        send_buf[MAX_MSGLEN + PACKET_HEADER];
	sizebuf_t   send;

	// write the packet header
	send.data = send_buf;
	send.maxsize = sizeof (send_buf);
	send.cursize = 0;

	MSG_WriteLong (&send, -1);			// -1 sequence means out of band
	SZ_Write (&send, data, length);

	// send the datagram
	// zoid, no input in demo playback mode
	if (!net_blocksend)
		Netchan_SendPacket (send.cursize, send.data, adr);
}

/*
	Netchan_OutOfBandPrint

	Sends a text message in an out-of-band datagram
*/
void
Netchan_OutOfBandPrint (netadr_t adr, const char *format, ...)
{
	static dstring_t *string;
	va_list     argptr;

	if (!string)
		string = dstring_new ();

	va_start (argptr, format);
	dvsprintf (string, format, argptr);
	va_end (argptr);

	Netchan_OutOfBand (adr, strlen (string->str), (byte *) string->str);
}

/*
	Netchan_Setup

	called to open a channel to a remote system
*/
void
Netchan_Setup (netchan_t *chan, netadr_t adr, int qport, int flags)
{
	memset (chan, 0, sizeof (*chan));

	chan->remote_address = adr;
	chan->last_received = *net_realtime;
	chan->incoming_sequence = -1;

	chan->message.data = chan->message_buf;
	chan->message.allowoverflow = true;
	chan->message.maxsize = sizeof (chan->message_buf);

	chan->qport = qport;
	chan->flags = flags;

	chan->rate = 1.0 / 2500.0;
}

#define	MAX_BACKUP	200

/*
	Netchan_CanPacket

	Returns true if the bandwidth choke isn't active
*/
qboolean
Netchan_CanPacket (netchan_t *chan)
{
	if (chan->cleartime < *net_realtime + MAX_BACKUP * chan->rate)
		return true;
	return false;
}

/*
	Netchan_CanReliable

	Returns true if the bandwidth choke isn't 
*/
qboolean
Netchan_CanReliable (netchan_t *chan)
{
	if (chan->reliable_length)
		return false;					// waiting for ack
	return Netchan_CanPacket (chan);
}

/*
	Netchan_Transmit

	tries to send an unreliable message to a connection, and handles the
	transmition / retransmition of the reliable messages.

	0 length will still generate a packet and deal with the reliable messages.
*/
void
Netchan_Transmit (netchan_t *chan, int length, byte *data)
{
	byte        send_buf[MAX_MSGLEN + PACKET_HEADER];
	int         i;
	unsigned int w1, w2;
	qboolean    send_reliable;
	sizebuf_t   send;

	// check for message overflow
	if (chan->message.overflowed) {
		chan->fatal_error = true;
		Con_Printf ("%s:Outgoing message overflow\n",
					NET_AdrToString (chan->remote_address));
		return;
	}
	// if the remote side dropped the last reliable message, resend it
	send_reliable = false;

	if (chan->incoming_acknowledged > chan->last_reliable_sequence
		&& chan->incoming_reliable_acknowledged != chan->reliable_sequence)
		send_reliable = true;

	// if the reliable transmit buffer is empty, copy the current message out
	if (!chan->reliable_length && chan->message.cursize) {
		memcpy (chan->reliable_buf, chan->message_buf, chan->message.cursize);
		chan->reliable_length = chan->message.cursize;
		chan->message.cursize = 0;
		chan->reliable_sequence ^= 1;
		send_reliable = true;
	}
	// write the packet header
	send.data = send_buf;
	send.maxsize = sizeof (send_buf);
	send.cursize = 0;

	w1 = chan->outgoing_sequence | (send_reliable << 31);
	w2 = chan->incoming_sequence | (chan->incoming_reliable_sequence << 31);

	chan->outgoing_sequence++;

	MSG_WriteLong (&send, w1);
	MSG_WriteLong (&send, w2);

	// send the qport if we are a client
	if (chan->flags & NC_SEND_QPORT)
		MSG_WriteShort (&send, chan->qport);

	// copy the reliable message to the packet first
	if (send_reliable) {
		SZ_Write (&send, chan->reliable_buf, chan->reliable_length);
		chan->last_reliable_sequence = chan->outgoing_sequence;
	}
	// add the unreliable part if space is available
	if (send.maxsize - send.cursize >= length)
		SZ_Write (&send, data, length);

	// send the datagram
	i = chan->outgoing_sequence & (MAX_LATENT - 1);
	chan->outgoing_size[i] = send.cursize;
	chan->outgoing_time[i] = *net_realtime;

	// zoid, no input in demo playback mode
	if (!net_blocksend)
		Netchan_SendPacket (send.cursize, send.data, chan->remote_address);

	if (chan->cleartime < *net_realtime)
		chan->cleartime = *net_realtime + send.cursize * chan->rate;
	else
		chan->cleartime += send.cursize * chan->rate;
	if (net_nochoke)
		chan->cleartime = *net_realtime;

	if (showpackets->int_val & 1)
		Con_Printf ("--> s=%i(%i) a=%i(%i) %-4i %i\n", chan->outgoing_sequence,
					send_reliable, chan->incoming_sequence,
					chan->incoming_reliable_sequence, send.cursize,
					chan->outgoing_sequence - chan->incoming_sequence);
}

/*
	Netchan_Process

	called when the current net_message is from remote_address
	modifies net_message so that it points to the packet payload
*/
qboolean
Netchan_Process (netchan_t *chan)
{
	int          qport;
	unsigned int reliable_ack, reliable_message, sequence, sequence_ack;

	if (!net_blocksend)
		if (!NET_CompareAdr (net_from, chan->remote_address))
			return false;

	// get sequence numbers     
	MSG_BeginReading (net_message);
	sequence = MSG_ReadLong (net_message);
	sequence_ack = MSG_ReadLong (net_message);

	// read the qport if we are a server
	if (chan->flags & NC_READ_QPORT)
		qport = MSG_ReadShort (net_message);

	reliable_message = sequence >> 31;
	reliable_ack = sequence_ack >> 31;

	sequence &= ~(1 << 31);
	sequence_ack &= ~(1 << 31);

	if (showpackets->int_val & 2)
		Con_Printf ("<-- s=%i(%i) a=%i(%i) %i\n", sequence, reliable_message,
					sequence_ack, reliable_ack, net_message->message->cursize);

	// get a rate estimation
#if 0 // FIXME: Dead code
	if (chan->outgoing_sequence - sequence_ack < MAX_LATENT) {
		int         i;
		double      time, rate;

		i = sequence_ack & (MAX_LATENT - 1);
		time = *net_realtime - chan->outgoing_time[i];
		time -= 0.1;					// subtract 100 ms
		if (time <= 0) {				// gotta be a digital link for <100
										// ms ping
			if (chan->rate > 1.0 / 5000)
				chan->rate = 1.0 / 5000;
		} else {
			if (chan->outgoing_size[i] < 512) {	// only deal with small
												// messages
				rate = chan->outgoing_size[i] / time;
				if (rate > 5000)
					rate = 5000;
				rate = 1.0 / rate;
				if (chan->rate > rate)
					chan->rate = rate;
			}
		}
	}
#endif

	// discard stale or duplicated packets
	if (sequence < (unsigned int) chan->incoming_sequence + 1) {
		if (showdrop->int_val)
			Con_Printf ("%s:Out of order packet %i at %i\n",
						NET_AdrToString (chan->remote_address), sequence,
						chan->incoming_sequence);
		return false;
	}

	// dropped packets don't keep the message from being used
	chan->net_drop = sequence - (chan->incoming_sequence + 1);
	if (chan->net_drop > 0) {
		chan->drop_count += 1;

		if (showdrop->int_val)
			Con_Printf ("%s:Dropped %i packets at %i\n",
						NET_AdrToString (chan->remote_address),
						sequence - (chan->incoming_sequence + 1), sequence);
	}

	// if the current outgoing reliable message has been acknowledged
	// clear the buffer to make way for the next
	if (reliable_ack == (unsigned int) chan->reliable_sequence)
		chan->reliable_length = 0;		// it has been received

	// if this message contains a reliable message, bump
	// incoming_reliable_sequence 
	chan->incoming_sequence = sequence;
	chan->incoming_acknowledged = sequence_ack;
	chan->incoming_reliable_acknowledged = reliable_ack;
	if (reliable_message)
		chan->incoming_reliable_sequence ^= 1;

	// the message can now be read from the current message pointer
	// update statistics counters
	chan->frame_latency = chan->frame_latency * OLD_AVG
		+ (chan->outgoing_sequence - sequence_ack) * (1.0 - OLD_AVG);
	chan->frame_rate = chan->frame_rate * OLD_AVG
		+ (*net_realtime - chan->last_received) * (1.0 - OLD_AVG);
	chan->good_count += 1;

	chan->last_received = *net_realtime;

	return true;
}
