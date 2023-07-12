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

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/msg.h"
#include "QF/sys.h"

#include "compat.h"
#include "netchan.h"

#define	PACKET_HEADER	8

int         net_nochoke;
int         net_blocksend;
double     *net_realtime;
int showpackets;
static cvar_t showpackets_cvar = {
	.name = "showpackets",
	.description =
		"Show all network packets",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &showpackets },
};
int showdrop;
static cvar_t showdrop_cvar = {
	.name = "showdrop",
	.description =
		"Toggle the display of how many packets you are dropping",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &showdrop },
};
int qport;
static cvar_t qport_cvar = {
	.name = "qport",
	.description =
		"The internal port number for the game networking code. Useful for "
		"clients who use multiple connections through one IP address (NAT/IP-"
		"MASQ) because default port is random.",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &qport },
};
void (*net_log_packet) (int length, const void *data, netadr_t to);


void
Netchan_Init (void)
{
	int         port;

	// pick a port value that should be nice and random
	port = Sys_TimeID ();

	qport = port;
}

void
Netchan_Init_Cvars (void)
{
	Cvar_Register (&showpackets_cvar, 0, 0);
	Cvar_Register (&showdrop_cvar, 0, 0);
	Cvar_Register (&qport_cvar, 0, 0);
}

/*
	Netchan_OutOfBand

	Sends an out-of-band datagram
*/
void
Netchan_OutOfBand (netadr_t adr, unsigned length, byte * data)
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
Netchan_Setup (netchan_t *chan, netadr_t adr, int qport, ncqport_e flags)
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
bool
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
bool
Netchan_CanReliable (netchan_t *chan)
{
	if (chan->reliable_length)
		return false;					// waiting for ack
	return Netchan_CanPacket (chan);
}

void
Netchan_Transmit (netchan_t *chan, unsigned length, byte *data)
{
	byte        send_buf[MAX_MSGLEN + PACKET_HEADER];
	int         i;
	unsigned int w1, w2;
	bool        send_reliable;
	sizebuf_t   send;

	// check for message overflow
	if (chan->message.overflowed) {
		chan->fatal_error = true;
		Sys_Printf ("%s:Outgoing message overflow\n",
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

	/// Send the qport if appropriate (we are a client).
	if (chan->flags & NC_QPORT_SEND)
		MSG_WriteShort (&send, chan->qport);

	/// First copy the reliable message to the packet.
	if (send_reliable) {
		SZ_Write (&send, chan->reliable_buf, chan->reliable_length);
		chan->last_reliable_sequence = chan->outgoing_sequence;
	}
	/// Then add the unreliable part if space is available.
	if (send.maxsize - send.cursize >= length)
		SZ_Write (&send, data, length);

	/// Send the datagram if not blocked (in demo playback mode)
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

	if (showpackets & 1) {
		Sys_Printf ("--> s=%i(%i) a=%i(%i) %-4i %i\n",
					chan->outgoing_sequence, send_reliable,
					chan->incoming_sequence, chan->incoming_reliable_sequence,
					send.cursize,
					chan->outgoing_sequence - chan->incoming_sequence);
		if (showpackets & 4) {
			SZ_Dump (&send);
		}
	}
}

bool
Netchan_Process (netchan_t *chan)
{
	unsigned int reliable_ack, reliable_message, sequence, sequence_ack;

	if (!net_blocksend)
		if (!NET_CompareAdr (net_from, chan->remote_address))
			return false;

	/// Get the sequence numbers.
	MSG_BeginReading (net_message);
	sequence = MSG_ReadLong (net_message);
	sequence_ack = MSG_ReadLong (net_message);

	/// Read the qport if appropriate (we are a server), but ignore it.
	if (chan->flags & NC_QPORT_READ)
		MSG_ReadShort (net_message);

	reliable_message = sequence >> 31;
	reliable_ack = sequence_ack >> 31;

	sequence &= ~(1 << 31);
	sequence_ack &= ~(1 << 31);

	if (showpackets & 2) {
		Sys_Printf ("<-- s=%i(%i) a=%i(%i) %i\n", sequence, reliable_message,
					sequence_ack, reliable_ack, net_message->message->cursize);
		if (showpackets & 8) {
			SZ_Dump (net_message->message);
		}
	}

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
			if (chan->outgoing_size[i] < 512) {	// deal with only small
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

	/// Discard stale or duplicated packets.
	if (sequence < (unsigned int) chan->incoming_sequence + 1) {
		if (showdrop)
			Sys_Printf ("%s:Out of order packet %i at %i\n",
						NET_AdrToString (chan->remote_address), sequence,
						chan->incoming_sequence);
		return false;
	}

	/// Dropped packets don't keep the message from being used.
	chan->net_drop = sequence - (chan->incoming_sequence + 1);
	if (chan->net_drop > 0) {
		chan->drop_count += 1;

		if (showdrop)
			Sys_Printf ("%s:Dropped %i packets at %i\n",
						NET_AdrToString (chan->remote_address),
						sequence - (chan->incoming_sequence + 1), sequence);
	}

	/// If the current outgoing reliable message has been acknowledged,
	/// clear the buffer to make way for the next.
	if (reliable_ack == (unsigned int) chan->reliable_sequence)
		chan->reliable_length = 0;		// it has been received

	/// If this message contains a reliable message, bump
	/// incoming_reliable_sequence
	chan->incoming_sequence = sequence;
	chan->incoming_acknowledged = sequence_ack;
	chan->incoming_reliable_acknowledged = reliable_ack;
	if (reliable_message)
		chan->incoming_reliable_sequence ^= 1;

	// The message can now be read from the current message pointer.
	/// Update statistics counters.
	chan->frame_latency = chan->frame_latency * OLD_AVG
		+ (chan->outgoing_sequence - sequence_ack) * (1.0 - OLD_AVG);
	chan->frame_rate = chan->frame_rate * OLD_AVG
		+ (*net_realtime - chan->last_received) * (1.0 - OLD_AVG);
	chan->good_count += 1;

	chan->last_received = *net_realtime;

	return true;
}

void
Netchan_SendPacket (int length, const void *data, netadr_t to)
{
	if (net_log_packet) {
		net_log_packet (length, data, to);
	}
	NET_SendPacket (length, data, to);
}
