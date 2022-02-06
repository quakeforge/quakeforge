/*
        net_packetlog.c

        packet logging/parsing - for debugging and educational purposes

        **EXPERIMENTAL**

        Copyright (C) 2000 Jukka Sorjonen <jukka.sorjonen@asikkala.fi>

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

#ifdef HAVE_IO_H
# include <io.h>
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

#include <ctype.h>
#include <stdarg.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/msg.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "compat.h"

#include "netchan.h"
#include "qw/protocol.h"
#include "qw/include/server.h"

cvar_t     *net_packetlog;
cvar_t     *net_loglevel;

// note: this is SUPPOSED to be duplicate, like many others
const char *svc_string[] = {
	"svc_bad",
	"svc_nop",
	"svc_disconnect",
	"svc_updatestat",
	"svc_version",						// [long] server version
	"svc_setview",						// [short] entity number
	"svc_sound",						// <see code>
	"svc_time",							// [float] server time
	"svc_print",						// [string] null terminated string
	"svc_stufftext",					// [string] stuffed into client's
										// console buffer the string
										// should be \n terminated
	"svc_setangle",						// [vec3] set the view angle to this
										// absolute value
	"svc_serverdata",					// [long] version ...
	"svc_lightstyle",					// [byte] [string]
	"svc_updatename",					// [byte] [string]
	"svc_updatefrags",					// [byte] [short]
	"svc_clientdata",					// <shortbits + data>
	"svc_stopsound",					// <see code>
	"svc_updatecolors",					// [byte] [byte]
	"svc_particle",						// [vec3] <variable>
	"svc_damage",						// [byte] impact [byte] blood [vec3]
										// from
	"svc_spawnstatic",
	"svc_spawnbinary",
	"svc_spawnbaseline",
	"svc_temp_entity",					// <variable>
	"svc_setpause",
	"svc_signonnum",
	"svc_centerprint",
	"svc_killedmonster",
	"svc_foundsecret",
	"svc_spawnstaticsound",
	"svc_intermission",
	"svc_finale",						// [string] music [string] text
	"svc_cdtrack",						// [byte] track [byte] looptrack
	"svc_sellscreen",
	"svc_smallkick",					// Quake svc_cutscene
	"svc_bigkick",
	"svc_updateping",
	"svc_updateentertime",
	"svc_updatestatlong",
	"svc_muzzleflash",
	"svc_updateuserinfo",
	"svc_download",
	"svc_playerinfo",
	"svc_nails",
	"svc_chokecount",
	"svc_modellist",
	"svc_soundlist",
	"svc_packetentities",
	"svc_deltapacketentities",
	"svc_maxspeed",
	"svc_entgravity",
	"svc_setinfo",
	"svc_serverinfo",
	"svc_updatepl",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL"
};

const char *clc_string[] = {
	"clc_bad",
	"clc_nop",
	"clc_disconnect",
	"clc_move",
	"clc_stringcmd",
	"clc_delta",
	"clc_tmove",
	"clc_upload"
};

#ifndef svc_spawnbinary
# define svc_spawnbinary 21
#endif

// Quake1, obsolete for QW
#define svc_time                7		// [float] server time
#define svc_updatename          13		// [byte] [string]
#define svc_version             4		// [long] server version
#define svc_clientdata          15		// <shortbits + data>
#define svc_updatecolors        17		// [byte] [byte]
#define svc_particle            18		// [vec3] <variable>
#define svc_signonnum           25		// [byte]  used for the signon
										// sequence
static QFile      *_stdout;
static QFile      *Net_PacketLog;
static const char **Net_sound_precache;
static sizebuf_t   _packet;
static qmsg_t      packet = {0, 0, &_packet};
static int         is_server = 0;

static int
Net_LogStart (const char *fname)
{
	Sys_Printf ("Opening packet logfile: %s\n", fname);
	Net_PacketLog = QFS_Open (fname, "at");
	if (!Net_PacketLog)
		return -1;
	return 0;
}

void
Net_LogStop (void *data)
{
	if (Net_PacketLog)
		Qclose (Net_PacketLog);
	Net_PacketLog = NULL;
}

static void
hex_dump_buf (unsigned char *buf, int len)
{
	int         pos = 0, llen, i;

	while (pos < len) {
		llen = (len - pos < 16 ? len - pos : 16);
		Net_LogPrintf ("%08x: ", pos);
		for (i = 0; i < llen; i++)
			Net_LogPrintf ("%02x ", buf[pos + i]);
		for (i = 0; i < 16 - llen; i++)
			Net_LogPrintf ("   ");
		Net_LogPrintf (" | ");

		for (i = 0; i < llen; i++)
			Net_LogPrintf ("%c", isprint (buf[pos + i]) ? buf[pos + i] : '.');
		for (i = 0; i < 16 - llen; i++)
			Net_LogPrintf (" ");
		Net_LogPrintf ("\n");
		pos += llen;
	}
}
/*
static void
ascii_dump_buf (unsigned char *buf, int len)
{
	int         pos = 0, llen, i;

	while (pos < len) {
		llen = (len - pos < 60 ? len - pos : 60);
		Net_LogPrintf ("%08x: ", pos);
		for (i = 0; i < llen; i++)
			Net_LogPrintf ("%c", isprint (buf[pos + i]) ? buf[pos + i] : '.');
		Net_LogPrintf ("\n");
		pos += llen;
	}
}
*/
void
Log_Incoming_Packet (const byte *p, int len, int has_sequence)
{
	if (!net_loglevel->int_val)
		return;

	if (is_server) {
		Net_LogPrintf ("\n<<<<<<<<<<<<<<<<<<<<< client to server %d bytes: "
					   "<<<<<<<<<<<<<<<<<<<<<<<<\n", len);
		if (net_loglevel->int_val != 3)
			hex_dump_buf ((unsigned char *) p, len);
		if (net_loglevel->int_val > 1)
			Analyze_Client_Packet (p, len, has_sequence);
	} else {
		Net_LogPrintf ("\n>>>>>>>>>>>>>>>>>>>>> server to client %d bytes: "
					   ">>>>>>>>>>>>>>>>>>>>>>>>\n", len);
		if (net_loglevel->int_val != 3)
			hex_dump_buf ((unsigned char *) p, len);
		if (net_loglevel->int_val > 1)
			Analyze_Server_Packet (p, len, has_sequence);
	}
	return;
}

void
Log_Outgoing_Packet (const byte *p, int len, int has_sequence)
{
	if (!net_loglevel->int_val)
		return;

	if (is_server) {
		Net_LogPrintf ("\n>>>>>>>>>>>>>>>>>>>>> server to client %d bytes: "
					   ">>>>>>>>>>>>>>>>>>>>>>>>\n", len);
		if (net_loglevel->int_val != 3)
			hex_dump_buf ((unsigned char *) p, len);
		if (net_loglevel->int_val > 1)
			Analyze_Server_Packet (p, len, has_sequence);
	} else {
		Net_LogPrintf ("\n<<<<<<<<<<<<<<<<<<<<< client to server %d bytes: "
					   "<<<<<<<<<<<<<<<<<<<<<<<<\n", len);
		if (net_loglevel->int_val != 3)
			hex_dump_buf ((unsigned char *) p, len);
		if (net_loglevel->int_val > 1)
			Analyze_Client_Packet (p, len, has_sequence);
	}
	return;
}

static void
Log_Delta (int bits)
{
	entity_state_t to;
	int i;

	// set everything to the state we are delta'ing from

	to.number = bits & 511;
	to.frame = 0;
	to.effects = 0;
	bits &= ~511;

	if (bits & U_MOREBITS) {			// read in the low order bits
		i = MSG_ReadByte (&packet);
		bits |= i;
	}

	// LordHavoc: Endy neglected to mark this as being part of the QSG
	// version 2 stuff...
	if (bits & U_EXTEND1) {
		bits |= MSG_ReadByte (&packet) << 16;
		if (bits & U_EXTEND2)
			bits |= MSG_ReadByte (&packet) << 24;
	}

	to.flags = bits;

	if (bits & U_MODEL)
		Net_LogPrintf (" MdlIdx: %d", MSG_ReadByte (&packet));
	if (bits & U_FRAME) {
		to.frame = MSG_ReadByte (&packet);
		Net_LogPrintf (" Frame: %d", to.frame);
	}
	if (bits & U_COLORMAP)
		Net_LogPrintf (" Colormap: %d", MSG_ReadByte (&packet));
	if (bits & U_SKIN)
		Net_LogPrintf (" Skinnum: %d", MSG_ReadByte (&packet));
	if (bits & U_EFFECTS) {
		to.effects = MSG_ReadByte (&packet);
		Net_LogPrintf (" Effects: %d", to.effects);
	}
	if (bits & U_ORIGIN1)
		Net_LogPrintf (" X: %f", MSG_ReadCoord (&packet));
	if (bits & U_ANGLE1)
		Net_LogPrintf (" Pitch: %f", MSG_ReadAngle (&packet));
	if (bits & U_ORIGIN2)
		Net_LogPrintf (" Y: %f", MSG_ReadCoord (&packet));
	if (bits & U_ANGLE2)
		Net_LogPrintf (" Yaw: %f", MSG_ReadAngle (&packet));
	if (bits & U_ORIGIN3)
		Net_LogPrintf (" Z: %f", MSG_ReadCoord (&packet));
	if (bits & U_ANGLE3)
		Net_LogPrintf (" Roll: %f", MSG_ReadAngle (&packet));

// Ender (QSG - Begin)
	if (bits & U_ALPHA)
		Net_LogPrintf (" Alpha: %d", MSG_ReadByte (&packet));
	if (bits & U_SCALE)
		Net_LogPrintf (" Scale: %d", MSG_ReadByte (&packet));
	if (bits & U_EFFECTS2)
		Net_LogPrintf (" U_EFFECTS2: %d", (to.effects & 0xFF) |
					   (MSG_ReadByte (&packet) << 8));
	if (bits & U_GLOWSIZE)
		Net_LogPrintf (" GlowSize: %d", MSG_ReadByte (&packet));
	if (bits & U_GLOWCOLOR)
		Net_LogPrintf (" ColorGlow: %d", MSG_ReadByte (&packet));
	if (bits & U_COLORMOD)
		Net_LogPrintf (" Colormod: %d", MSG_ReadByte (&packet));
	if (bits & U_FRAME2)
		Net_LogPrintf (" Uframe2: %d", ((to.frame & 0xFF) |
										(MSG_ReadByte (&packet) << 8)));
// Ender (QSG - End)

	return;
}


static void
Parse_Server_Packet (int has_sequence)
{
	const char *s;
	int         c = 0, i, ii, iii, mask1, mask2;
	long        seq1 = 0, seq2;

	if (has_sequence) {
		seq1 = MSG_ReadLong (&packet);
		if (packet.badread)
			return;
	}

	if (seq1 == -1) {
		Net_LogPrintf ("Special Packet");
	} else {
		if (has_sequence) {
			seq2 = MSG_ReadLong (&packet);
			// FIXME: display seqs right when reliable
			Net_LogPrintf ("\nSeq: %ld Ack: %ld ", seq1 & 0x7FFFFFFF,
						   seq2 & 0x7FFFFFFF);

			if ((seq1 >> 31) & 0x01)
				Net_LogPrintf ("SV_REL ");
			if ((seq2 >> 31) & 0x01)
				Net_LogPrintf ("SV_RELACK");
			Net_LogPrintf ("\n");
		}

		while (1) {
			if (packet.badread) {
				Net_LogPrintf ("*** BAD READ *** %s", svc_string[c]);
				break;
			}
			c = MSG_ReadByte (&packet);
			if (c == -1)
				break;
			Net_LogPrintf ("<%06x> [0x%02x] ", MSG_GetReadCount (&packet), c);

			if (c < 53)
				Net_LogPrintf ("%s: ", svc_string[c]);

			if (MSG_GetReadCount (&packet) > packet.message->cursize)
				return;

			switch (c) {
				case svc_bad:
					Net_LogPrintf (" - should not happen");
					break;
				case svc_nop:
					Net_LogPrintf (" No operation");
					break;
				case svc_disconnect:
					Net_LogPrintf (" <Quit>");
					break;
				case svc_updatestat:
					i = MSG_ReadByte (&packet);
					Net_LogPrintf (" index: %d value: %d", i, MSG_ReadByte
								   (&packet));
					break;
				case svc_version:
					Net_LogPrintf ("**QW OBSOLETE**");
					break;
				case svc_setview:
					Net_LogPrintf ("**QW OBSOLETE**");
					break;
				case svc_sound:
					i = MSG_ReadShort (&packet);
					Net_LogPrintf (": (%d) ", i);
					if (i & SND_VOLUME)
						Net_LogPrintf ("Volume %d ", MSG_ReadByte (&packet));
					if (i & SND_ATTENUATION)
						Net_LogPrintf ("Ann: %f",
									   (float) MSG_ReadByte (&packet) / 64.0);
					ii = MSG_ReadByte (&packet);

					Net_LogPrintf ("%d (%s) ", ii, Net_sound_precache[ii]);
					Net_LogPrintf ("Pos: ");
					for (ii = 0; ii < 3; ii++)
						Net_LogPrintf ("%f ", MSG_ReadCoord (&packet));
					Net_LogPrintf ("Ent: %d ", (i >> 3) & 1023);
					Net_LogPrintf ("Channel %d ", i & 7);
					break;
				case svc_time:
					Net_LogPrintf ("**QW OBSOLETE**\n");
					break;
				case svc_print:
					// FIXME: i==PRINT_CHAT
					Net_LogPrintf ("[%d] ", MSG_ReadByte (&packet));
					Net_LogPrintf ("%s", MSG_ReadString (&packet));
					break;
				case svc_stufftext:
					Net_LogPrintf ("%s", MSG_ReadString (&packet));
					break;
				case svc_setangle:
					for (i = 0; i < 3; i++)
						Net_LogPrintf ("%f ", MSG_ReadAngle (&packet));
					break;
				case svc_serverdata:
					Net_LogPrintf ("Ver: %d", MSG_ReadLong (&packet));
					Net_LogPrintf (" Client ID: %d", MSG_ReadLong (&packet));
					Net_LogPrintf (" Dir: %s", MSG_ReadString (&packet));
					Net_LogPrintf (" User ID: %d", MSG_ReadByte (&packet));
					Net_LogPrintf (" Map: %s", MSG_ReadString (&packet));
					for (i = 0; i < 10; i++)
						MSG_ReadFloat (&packet);
					break;

				case svc_lightstyle:
					i = MSG_ReadByte (&packet);
					if (i >= MAX_LIGHTSTYLES)
						return;
					Net_LogPrintf ("%d %s", i, MSG_ReadString (&packet));
					break;
				case svc_updatename:
					Net_LogPrintf ("**QW OBSOLETE**");
					break;
				case svc_updatefrags:
					Net_LogPrintf ("player: %d ", MSG_ReadByte (&packet));
					Net_LogPrintf ("frags: %d",
								   (short) MSG_ReadShort (&packet));
					break;
				case svc_clientdata:
					Net_LogPrintf ("**QW OBSOLETE**");
					break;
				case svc_stopsound:
					Net_LogPrintf ("%d", MSG_ReadShort (&packet));
					break;

				case svc_updatecolors:
					Net_LogPrintf ("**QW OBSOLETE**");
					break;
				case svc_particle:
					Net_LogPrintf ("**QW OBSOLETE**");
					break;

				case svc_damage:
					// FIXME: parse damage
					Net_LogPrintf ("armor: %d ", MSG_ReadByte (&packet));
					Net_LogPrintf ("health: %d from", MSG_ReadByte (&packet));
					for (i = 0; i < 3; i++)
						Net_LogPrintf ("%f%s", MSG_ReadCoord (&packet),
									   i < 2 ? "," : "");
					break;
				case svc_spawnstatic:
					Net_LogPrintf ("Model: %d", MSG_ReadByte (&packet));
					Net_LogPrintf (" Frame: %d ", MSG_ReadByte (&packet));
					Net_LogPrintf ("Color: %d ", MSG_ReadByte (&packet));
					Net_LogPrintf ("Skin: %d", MSG_ReadByte (&packet));
					for (i = 0; i < 3; i++) {
						Net_LogPrintf ("%d: %f ", i + 1,
									   MSG_ReadCoord (&packet));
						Net_LogPrintf ("%f", MSG_ReadAngle (&packet));
					}
					break;
				case svc_spawnbinary:
					Net_LogPrintf ("**OBSOLETE**");
					break;
				case svc_spawnbaseline:
					Net_LogPrintf ("%d", MSG_ReadShort (&packet));
					Net_LogPrintf (" idx: %d", MSG_ReadByte (&packet));
					Net_LogPrintf (" Frame: %d", MSG_ReadByte (&packet));
					Net_LogPrintf (" Colormap: %d", MSG_ReadByte (&packet));
					Net_LogPrintf (" Skin: %d", MSG_ReadByte (&packet));
					for (i = 0; i < 3; i++) {
						Net_LogPrintf (" %f", MSG_ReadCoord (&packet));
						Net_LogPrintf (" %f", MSG_ReadAngle (&packet));
					};
					break;
				case svc_temp_entity:
					i = MSG_ReadByte (&packet);
					switch (i) {
						case 0:
						case 1:
						case 3:
						case 4:
						case 7:
						case 8:
						case 10:
						case 11:
						case 13:
							Net_LogPrintf (" origin");
							for (i = 0; i < 3; i++)
								Net_LogPrintf (" %f", MSG_ReadCoord (&packet));
							break;
						case 5:
						case 6:
						case 9:
							Net_LogPrintf (" created by %d",
										   MSG_ReadShort (&packet));
							Net_LogPrintf (" origin:");
							for (i = 0; i < 3; i++)
								Net_LogPrintf ("%c%f", i ? ',' : ' ',
											   MSG_ReadCoord (&packet));
							Net_LogPrintf (" trace endpos:");
							for (i = 0; i < 3; i++)
								Net_LogPrintf ("%c%f", i ? ',' : ' ',
											   MSG_ReadCoord (&packet));
							break;
						case 2:
						case 12:
							Net_LogPrintf (" count: %d", MSG_ReadByte
										   (&packet));
							Net_LogPrintf (" origin:");
							for (i = 0; i < 3; i++)
								Net_LogPrintf ("%c%f", i ? ',' : ' ',
											   MSG_ReadCoord (&packet));
							break;
						default:
							Net_LogPrintf (" unknown value %d for tempentity",
										   i);
							break;
					}
					break;

				case svc_setpause:
					Net_LogPrintf (" %d", MSG_ReadByte (&packet));
					break;
				case svc_signonnum:
					Net_LogPrintf ("**QW OBSOLETE**");
					break;
				case svc_centerprint:
					Net_LogPrintf ("%s\n", MSG_ReadString (&packet));
					break;
				case svc_killedmonster:
					break;
				case svc_foundsecret:
					break;
				case svc_spawnstaticsound:
					Net_LogPrintf ("pos");
					for (i = 0; i < 3; i++)
						Net_LogPrintf ("%c%f", i ? ',' : ' ',
									   MSG_ReadCoord (&packet));
					for (i = 0; i < 3; i++)
						Net_LogPrintf (" %d", MSG_ReadByte (&packet));
					break;
				case svc_intermission:
					for (i = 0; i < 3; i++)
						Net_LogPrintf ("%f ", MSG_ReadCoord (&packet));
					Net_LogPrintf ("\n");
					for (i = 0; i < 3; i++)
						Net_LogPrintf ("%f ", MSG_ReadAngle (&packet));
					break;
				case svc_finale:
					Net_LogPrintf ("%s", MSG_ReadString (&packet));
					break;
				case svc_cdtrack:
					Net_LogPrintf ("%d", MSG_ReadByte (&packet));
					break;
				case svc_sellscreen:
					break;
				case svc_smallkick:
					break;
				case svc_bigkick:
					break;
				case svc_updateping:
					Net_LogPrintf ("Player: %d ", MSG_ReadByte (&packet));
					Net_LogPrintf ("Ping: %d", MSG_ReadShort (&packet));
					break;
				case svc_updateentertime:
					Net_LogPrintf ("Player: %d ", MSG_ReadByte (&packet));
					Net_LogPrintf ("Time: %f", MSG_ReadFloat (&packet));
					break;

				case svc_updatestatlong:
					i = MSG_ReadByte (&packet);
					Net_LogPrintf ("%d value: %d", i, MSG_ReadLong (&packet));
					break;

				case svc_muzzleflash:
					Net_LogPrintf ("%d", MSG_ReadShort (&packet));
					break;

				case svc_updateuserinfo:
					Net_LogPrintf ("Player: %d ", MSG_ReadByte (&packet));
					Net_LogPrintf ("ID: %d ", MSG_ReadLong (&packet));
					Net_LogPrintf ("Info: %s", MSG_ReadString (&packet));
					break;
				case svc_download:
					ii = MSG_ReadShort (&packet);
					Net_LogPrintf ("%d bytes at %d", ii, MSG_ReadByte
								   (&packet));
					for (i = 0; i < ii; i++)
						MSG_ReadByte (&packet);
					break;
				case svc_playerinfo:
					Net_LogPrintf ("\n\tPlayer: %d", MSG_ReadByte (&packet));
					mask1 = MSG_ReadShort (&packet);
					Net_LogPrintf (" Mask1: %d", mask1);
#if 1
					Net_LogPrintf (" Origin:");
					for (i = 0; i < 3; i++)
						Net_LogPrintf ("%c%f", i ? ',' : ' ',
									   MSG_ReadCoord (&packet));
#endif
					Net_LogPrintf (" Frame: %d", MSG_ReadByte (&packet));

#if 1
					if (mask1 & PF_MSEC)
						Net_LogPrintf (" Ping: %d", MSG_ReadByte (&packet));

					if (mask1 & PF_COMMAND) {
						mask2 = MSG_ReadByte (&packet);	// command
						if (mask2 & 0x01)
							Net_LogPrintf (" Pitch: %f", MSG_ReadAngle16
										   (&packet));
						if (mask2 & 0x80)
							Net_LogPrintf (" Yaw: %f", MSG_ReadAngle16
										   (&packet));
						if (mask2 & 0x02)
							Net_LogPrintf (" Roll: %f", MSG_ReadAngle16
										   (&packet));
						if (mask2 & 0x04)
							Net_LogPrintf (" Speed1: %d",
										   MSG_ReadShort (&packet));
						if (mask2 & 0x08)
							Net_LogPrintf (" Speed2: %d",
										   MSG_ReadShort (&packet));
						if (mask2 & 0x10)
							Net_LogPrintf (" Speed3: %d",
										   MSG_ReadShort (&packet));
						if (mask2 & 0x20)
							Net_LogPrintf (" Flag: %d", MSG_ReadByte
										   (&packet));
						if (mask2 & 0x40)
							Net_LogPrintf (" Impulse: %d", MSG_ReadByte
										   (&packet));
						Net_LogPrintf (" Msec: %d", MSG_ReadByte (&packet));
					}
					if (mask1 & PF_VELOCITY1)
						Net_LogPrintf (" Xspd: %f", MSG_ReadCoord (&packet));
					if (mask1 & PF_VELOCITY2)
						Net_LogPrintf (" Yspd: %f", MSG_ReadCoord (&packet));
					if (mask1 & PF_VELOCITY3)
						Net_LogPrintf (" ZSpd: %f", MSG_ReadCoord (&packet));
					if (mask1 & PF_MODEL)
						Net_LogPrintf (" Model: %d", MSG_ReadByte (&packet));
					if (mask1 & PF_SKINNUM)
						Net_LogPrintf (" Skin: %d", MSG_ReadByte (&packet));
					if (mask1 & PF_EFFECTS)
						Net_LogPrintf (" Effects: %d", MSG_ReadByte (&packet));

					if (mask1 & PF_WEAPONFRAME)
						Net_LogPrintf (" Weapon frame: %d", MSG_ReadByte
									   (&packet));

#else
					if (mask1 & (DF_ORIGIN | (DF_ORIGIN << 1)
								 | (DF_ORIGIN << 2))) {
						Net_LogPrintf (" Origin:");
						for (i = 0; i < 3; i++)
							if (mask1 & (DF_ORIGIN << i))
								Net_LogPrintf ("%c%f", i ? ',' : ' ',
											   MSG_ReadCoord (&packet));
							else
								Net_LogPrintf ("%c", i ? ',' : ' ');
					}
					if (mask1 & (DF_ANGLES | (DF_ANGLES << 1)
								 | (DF_ANGLES << 2))) {
						Net_LogPrintf (" Angles:");
						for (i = 0; i < 3; i++)
							if (mask1 & (DF_ANGLES << i))
								Net_LogPrintf ("%c%f", i ? ',' : ' ',
											   MSG_ReadAngle16 (&packet));
							else
								Net_LogPrintf ("%c", i ? ',' : ' ');
					}
					if (mask1 & DF_MODEL)
						Net_LogPrintf (" Model: %d", MSG_ReadByte (&packet));
					if (mask1 & DF_SKINNUM)
						Net_LogPrintf (" Skin: %d", MSG_ReadByte (&packet));
					if (mask1 & DF_EFFECTS)
						Net_LogPrintf (" Effects: %d", MSG_ReadByte (&packet));

					if (mask1 & DF_WEAPONFRAME)
						Net_LogPrintf (" Weapon frame: %d", MSG_ReadByte
									   (&packet));
#endif
					break;
				case svc_nails:
					ii = MSG_ReadByte (&packet);
					Net_LogPrintf (" %d (bits not parsed)", ii);
					for (i = 0; i < ii; i++) {
						for (iii = 0; iii < 6; iii++)
							MSG_ReadByte (&packet);
					}
					break;
				case svc_chokecount:
					Net_LogPrintf ("%d", MSG_ReadByte (&packet));
					break;
				case svc_modellist:
					ii = MSG_ReadByte (&packet);
					Net_LogPrintf ("start %d", ii);
					for (i = ii; i < 256; i++) {
						s = MSG_ReadString (&packet);
						if (packet.badread)
							break;
						if (!s)
							break;
						if (strlen (s) == 0)
							break;
						Net_LogPrintf ("\n\tModel %d: %s", i, s);
					}
					i = MSG_ReadByte (&packet);
					if (i)
						Net_LogPrintf ("\n\tnext at %d", i);
					else
						Net_LogPrintf ("\n\t*End of modellist*");
					break;
				case svc_soundlist:
					ii = MSG_ReadByte (&packet);
					Net_LogPrintf ("start %d", ii);
					for (i = ii; i < 256; i++) {
						s = MSG_ReadString (&packet);
						if (packet.badread)
							break;
						if (!s)
							break;
						if (strlen (s) == 0)
							break;
						Net_LogPrintf ("\n\tSound %d: %s", i, s);
					}
					i = MSG_ReadByte (&packet);

					if (i)
						Net_LogPrintf ("\n\tnext at %d", i);
					else
						Net_LogPrintf ("\n\t*End of sound list*");
					break;
				case svc_packetentities:
packetentities:
					while (1) {
						mask1 = MSG_ReadShort (&packet);
						if (packet.badread) {
							Net_LogPrintf ("Badread\n");
							return;
						}
						if (!mask1)
							break;
						Net_LogPrintf ("%d", mask1 & 511);
						if (mask1 & U_REMOVE)
							Net_LogPrintf (" UREMOVE");
						Log_Delta (mask1);
						Net_LogPrintf ("\n");
					}
					break;
				case svc_deltapacketentities:
					Net_LogPrintf ("idx: %d\n", MSG_ReadByte (&packet));
					goto packetentities;
					break;
				case svc_maxspeed:
					Net_LogPrintf ("%f", MSG_ReadFloat (&packet));
					break;
				case svc_entgravity:
					Net_LogPrintf ("%f", MSG_ReadFloat (&packet));
					break;
				case svc_setinfo:
					Net_LogPrintf ("Player: %d ", MSG_ReadByte (&packet));
					Net_LogPrintf ("Keyname: %s ", MSG_ReadString (&packet));
					Net_LogPrintf ("Value: %s", MSG_ReadString (&packet));
					break;
				case svc_serverinfo:
					Net_LogPrintf ("Name: %s ", MSG_ReadString (&packet));
					Net_LogPrintf ("Value: %s", MSG_ReadString (&packet));
					break;
				case svc_updatepl:
					Net_LogPrintf ("Player: %d ", MSG_ReadByte (&packet));
					Net_LogPrintf ("Ploss: %d", MSG_ReadByte (&packet));
					break;
				default:
					Net_LogPrintf ("**UNKNOWN**: [%d]", c);
					break;
			}
			Net_LogPrintf ("\n");
		}
	}
}

void
Analyze_Server_Packet (const byte * data, int len, int has_sequence)
{
	if (!Net_PacketLog)
		Net_PacketLog = _stdout;
	packet.message->data = (byte*)data;
	packet.message->cursize = len;
	MSG_BeginReading (&packet);
	Parse_Server_Packet (has_sequence);
	if (Net_PacketLog == _stdout)
		Net_PacketLog = NULL;
}

static void
Parse_Client_Packet (int has_sequence)
{
	int         mask, i, c, ii;
	long        seq1 = 0, seq2;

	if (has_sequence)
		seq1 = MSG_ReadLong (&packet);
	if (seq1 == -1) {
		Net_LogPrintf ("Special: %s\n", MSG_ReadString (&packet));
		return;
	} else {
		if (has_sequence) {
			// FIXME: display seqs right when reliable
			seq2 = MSG_ReadLong (&packet);

			Net_LogPrintf ("\nSeq: %ld Ack: %ld ", seq1 & 0x7FFFFFFF,
						   seq2 & 0x7FFFFFFF);

			Net_LogPrintf ("QP: %u\n", MSG_ReadShort (&packet) & 0xFFFF);
		}

		while (1) {
			if (packet.badread)
				break;
			c = MSG_ReadByte (&packet);
			if (c == -1)
				break;
			Net_LogPrintf ("\n<%06x> [0x%02x] ", MSG_GetReadCount (&packet),
						   c);
			if (c < 8)
				Net_LogPrintf ("%s: ", clc_string[c]);

			switch (c) {
				case clc_nop:
					break;
				case clc_delta:
					Net_LogPrintf ("%d", MSG_ReadByte (&packet));
					break;
				case clc_move:
					Net_LogPrintf ("checksum = %02x ", MSG_ReadByte (&packet));
					Net_LogPrintf ("PacketLoss: %d", MSG_ReadByte (&packet));

					for (i = 0; i < 3; i++) {
						mask = MSG_ReadByte (&packet);
						Net_LogPrintf ("\n\t(%d) mask = %02x", i, mask);
						if (mask & 0x01)
							Net_LogPrintf (" Tilt: %f", MSG_ReadAngle16
										   (&packet));
						if (mask & 0x80)
							Net_LogPrintf (" Yaw: %f", MSG_ReadAngle16
										   (&packet));
						if (mask & 0x02)
							Net_LogPrintf (" Roll: %f", MSG_ReadAngle16
										   (&packet));
						if (mask & 0x04)
							Net_LogPrintf (" Fwd: %d",
										   MSG_ReadShort (&packet));
						if (mask & 0x08)
							Net_LogPrintf (" Right: %d",
										   MSG_ReadShort (&packet));
						if (mask & 0x10)
							Net_LogPrintf (" Up: %d",
										   MSG_ReadShort (&packet));
						if (mask & 0x20)
							Net_LogPrintf (" Flags: %d", MSG_ReadByte
										   (&packet));
						if (mask & 0x40)
							Net_LogPrintf (" Impulse: %d", MSG_ReadByte
										   (&packet));
						Net_LogPrintf (" Msec: %d", MSG_ReadByte (&packet));
					}
					break;
				case clc_stringcmd:
					Net_LogPrintf ("%s", MSG_ReadString (&packet));
					break;
				case clc_tmove:
					for (i = 0; i < 3; i++)
						Net_LogPrintf ("%f ", MSG_ReadCoord (&packet));
					break;
				case clc_upload:
					ii = MSG_ReadShort (&packet);
					Net_LogPrintf ("%d bytes at %d", ii, MSG_ReadByte
								   (&packet));
					for (i = 0; i < ii; i++)
						MSG_ReadByte (&packet);
					break;
				default:
					Net_LogPrintf ("**UNKNOWN**: [%d]", c);
					break;
			}
			Net_LogPrintf ("\n");
		}
	}
}

void
Analyze_Client_Packet (const byte * data, int len, int has_sequence)
{
	if (!Net_PacketLog)
		Net_PacketLog = _stdout;
	packet.message->data = (byte*)data;
	packet.message->cursize = len;
	MSG_BeginReading (&packet);
	Parse_Client_Packet (has_sequence);
	if (Net_PacketLog == _stdout)
		Net_PacketLog = NULL;
}

static void
net_packet_log_f (int length, const void *data, netadr_t to)
{
	Log_Outgoing_Packet (data, length, 1);
}

static void
Net_PacketLog_f (cvar_t *var)
{
	if (var->int_val) {
		Net_LogStart ("qfpacket.log");
		net_log_packet = net_packet_log_f;
	} else {
		Net_LogStop (0);
		net_log_packet = 0;
	}
}

static void
Net_PacketLog_Zap_f (void)
{
	if (Net_PacketLog && Net_PacketLog != _stdout) {
		Sys_Printf ("truncating packet logfile: %s\n", "qfpacket.log");
		Qseek (Net_PacketLog, 0, 0);
		Qwrite (Net_PacketLog, 0, 0);
	} else {
		Sys_Printf ("Deleting packet logfile: %s\n", "qfpacket.log");
		QFS_Remove ("qfpacket.log");
	}
}

int
Net_Log_Init (const char **sound_precache, int server)
{
	is_server = server;
	Net_sound_precache = sound_precache;

	_stdout = Qdopen (1, "wt");	// create a QFile of stdout

	net_packetlog = Cvar_Get ("net_packetlog", "0", CVAR_NONE, Net_PacketLog_f,
							 "enable/disable packet logging");

// 0 = no logging
// 1 = hex dump only
// 2 = parse/hexdump
// 3 = just parse
// 4 = parse/hexdump, skip movement/empty messages

	net_loglevel = Cvar_Get ("net_loglevel", "2", CVAR_NONE, NULL,
							 "Packet logging/parsing");

	Cmd_AddCommand ("net_packetlog_zap", Net_PacketLog_Zap_f,
					"clear the packet log file");
	return 0;
}

/*
	NET_LogPrintf

	Prints packet to logfile, adds time stamp etc.
*/
void
Net_LogPrintf (const char *fmt, ...)
{
	char        text[2048];
	va_list     argptr;

	va_start (argptr, fmt);
	vsnprintf (text, sizeof (text), fmt, argptr);
	va_end (argptr);
	if (!Net_PacketLog)
		return;

	Qprintf (Net_PacketLog, "%s", text);
	Qflush (Net_PacketLog);
}
