/*
	client.c

	quakeworld client processing

	Copyright (C) 2004 Bill Currie <bill@taniwha.org>

	Author: Bill Currie
	Date: 2004/02/20

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/checksum.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/idparse.h"
#include "QF/info.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "qw/bothdefs.h"
#include "qw/msg_ucmd.h"
#include "qw/protocol.h"

#include "qtv/include/client.h"
#include "qtv/include/connection.h"
#include "qtv/include/qtv.h"
#include "qtv/include/server.h"

int client_count;
static client_t *clients;

typedef struct ucmd_s {
	const char *name;
	void        (*func) (client_t *cl, void *userdata);
	unsigned    no_redirect:1;
	unsigned    overridable:1;
	unsigned    freeable:1;
	void        *userdata;
	void        (*on_free) (void *userdata);
} ucmd_t;

static void
client_drop (client_t *cl)
{
	MSG_WriteByte (&cl->netchan.message, svc_disconnect);
	qtv_printf ("Client %s removed\n", Info_ValueForKey (cl->userinfo, "name"));
	cl->drop = 1;
}

static void
cl_new_f (client_t *cl, void *unused)
{
	if (!cl->server) {
		qtv_printf ("\"cmd list\" for a list of servers\n");
		qtv_printf ("\"cmd connect <servername>\" to connect to a server\n");
	} else {
		Client_New (cl);
	}
}

static void
cl_modellist_f (client_t *cl, void *unused)
{
	server_t   *sv = cl->server;
	unsigned    n;
	char      **s;

	if (atoi (Cmd_Argv (1)) != sv->spawncount) {
		qtv_printf ("modellist from different level\n");
		Client_New (cl);
		return;
	}
	n = atoi (Cmd_Argv (2));
	if (n >= MAX_MODELS) {
		qtv_printf ("invalid modellist index\n");
		Client_New (cl);
		return;
	}

	MSG_WriteByte (&cl->netchan.message, svc_modellist);
	MSG_WriteByte (&cl->netchan.message, n);
	for (s = sv->modellist + n;
		 *s && cl->netchan.message.cursize < (MAX_MSGLEN /2);
		 s++, n++)
		MSG_WriteString (&cl->netchan.message, *s);
	MSG_WriteByte (&cl->netchan.message, 0);
	if (*s)
		MSG_WriteByte (&cl->netchan.message, n);
	else
		MSG_WriteByte (&cl->netchan.message, 0);
}

static void
cl_soundlist_f (client_t *cl, void *unused)
{
	server_t   *sv = cl->server;
	unsigned    n;
	char      **s;

	if (!cl->server)
		return;
	if (atoi (Cmd_Argv (1)) != sv->spawncount) {
		qtv_printf ("soundlist from different level\n");
		Client_New (cl);
		return;
	}
	n = atoi (Cmd_Argv (2));
	if (n >= MAX_SOUNDS) {
		qtv_printf ("invalid soundlist index\n");
		Client_New (cl);
		return;
	}

	MSG_WriteByte (&cl->netchan.message, svc_soundlist);
	MSG_WriteByte (&cl->netchan.message, n);
	for (s = sv->soundlist + n;
		 *s && cl->netchan.message.cursize < (MAX_MSGLEN /2);
		 s++, n++)
		MSG_WriteString (&cl->netchan.message, *s);
	MSG_WriteByte (&cl->netchan.message, 0);
	if (*s)
		MSG_WriteByte (&cl->netchan.message, n);
	else
		MSG_WriteByte (&cl->netchan.message, 0);
}

static void
cl_prespawn_f (client_t *cl, void *unused)
{
	const char *cmd;
	server_t   *sv = cl->server;
	int         buf, size;
	sizebuf_t  *msg;

	if (!cl->server)
		return;
	if (atoi (Cmd_Argv (1)) != sv->spawncount) {
		qtv_printf ("prespawn from different level\n");
		Client_New (cl);
		return;
	}
	buf = atoi (Cmd_Argv (2));
	if (buf >= sv->num_signon_buffers)
		buf = 0;
	if (buf == sv->num_signon_buffers - 1)
		cmd = va (0, "cmd spawn %i 0\n", cl->server->spawncount);
	else
		cmd = va (0, "cmd prespawn %i %i\n", cl->server->spawncount, buf + 1);
	size = sv->signon_buffer_size[buf] + 1 + strlen (cmd) + 1;
	msg = MSG_ReliableCheckBlock (&cl->backbuf, size);
	SZ_Write (msg, sv->signon_buffers[buf], sv->signon_buffer_size[buf]);
	MSG_WriteByte (msg, svc_stufftext);
	MSG_WriteString (msg, cmd);
}

static void
cl_spawn_f (client_t *cl, void *unused)
{
	const char *cmd;
	server_t   *sv = cl->server;
	const char *info;
	player_t   *pl;
	int         i;
	sizebuf_t  *msg;

	if (!cl->server)
		return;
	if (atoi (Cmd_Argv (1)) != sv->spawncount) {
		qtv_printf ("spawn from different level\n");
		Client_New (cl);
		return;
	}
	for (i = 0, pl = sv->players; i < MAX_SV_PLAYERS; i++, pl++) {
		if (!pl->info)
			continue;
		msg = MSG_ReliableCheckBlock (&cl->backbuf,
									  24 + Info_CurrentSize(pl->info));
		MSG_WriteByte (msg, svc_updatefrags);
		MSG_WriteByte (msg, i);
		MSG_WriteShort (msg, pl->frags);
		MSG_WriteByte (msg, svc_updateping);
		MSG_WriteByte (msg, i);
		MSG_WriteShort (msg, pl->ping);
		MSG_WriteByte (msg, svc_updatepl);
		MSG_WriteByte (msg, i);
		MSG_WriteByte (msg, pl->pl);
		MSG_WriteByte (msg, svc_updateentertime);
		MSG_WriteByte (msg, i);
		MSG_WriteFloat (msg, pl->time);
		info = pl->info ? Info_MakeString (pl->info, 0) : "";
		MSG_WriteByte (msg, svc_updateuserinfo);
		MSG_WriteByte (msg, i);
		MSG_WriteLong (msg, pl->uid);
		MSG_WriteString (msg, info);
		if (cl->backbuf.num_backbuf)
			MSG_Reliable_FinishWrite (&cl->backbuf);
	}
	for (i = 0; i < MAX_LIGHTSTYLES; i++) {
		MSG_ReliableWrite_Begin (&cl->backbuf, svc_lightstyle,
								 3 + (sv->lightstyles[i] ?
									  strlen (sv->lightstyles[i]) : 1));
		MSG_ReliableWrite_Byte (&cl->backbuf, i);
		MSG_ReliableWrite_String (&cl->backbuf, sv->lightstyles[i]);
	}
	MSG_ReliableWrite_Begin (&cl->backbuf, svc_updatestatlong, 6);
	MSG_ReliableWrite_Byte (&cl->backbuf, STAT_TOTALSECRETS);
	MSG_ReliableWrite_Long (&cl->backbuf,
							sv->players[0].stats[STAT_TOTALSECRETS]);
	MSG_ReliableWrite_Begin (&cl->backbuf, svc_updatestatlong, 6);
	MSG_ReliableWrite_Byte (&cl->backbuf, STAT_TOTALMONSTERS);
	MSG_ReliableWrite_Long (&cl->backbuf,
							sv->players[0].stats[STAT_TOTALMONSTERS]);
	MSG_ReliableWrite_Begin (&cl->backbuf, svc_updatestatlong, 6);
	MSG_ReliableWrite_Byte (&cl->backbuf, STAT_SECRETS);
	MSG_ReliableWrite_Long (&cl->backbuf, sv->players[0].stats[STAT_SECRETS]);
	MSG_ReliableWrite_Begin (&cl->backbuf, svc_updatestatlong, 6);
	MSG_ReliableWrite_Byte (&cl->backbuf, STAT_SECRETS);
	MSG_ReliableWrite_Long (&cl->backbuf, sv->players[0].stats[STAT_SECRETS]);
	cmd = "skins\n";
	MSG_ReliableWrite_Begin (&cl->backbuf, svc_stufftext, strlen (cmd) + 2);
	MSG_ReliableWrite_String (&cl->backbuf, cmd);
}

static void
cl_begin_f (client_t *cl, void *unused)
{
	server_t   *sv = cl->server;

	if (!cl->server)
		return;
	if (atoi (Cmd_Argv (1)) != sv->spawncount) {
		qtv_printf ("spawn from different level\n");
		Client_New (cl);
		return;
	}
	cl->connected = 1;
}

static void
cl_drop_f (client_t *cl, void *unused)
{
	if (cl->server)
		Server_Disconnect (cl);
	client_drop (cl);
}

static void
cl_pings_f (client_t *cl, void *unused)
{
}

static void
cl_rate_f (client_t *cl, void *unused)
{
}

static void
cl_say_f (client_t *cl, void *unused)
{
}

static void
cl_setinfo_f (client_t *cl, void *unused)
{
}

static void
cl_serverinfo_f (client_t *cl, void *unused)
{
	if (cl->server) {
		Info_Print (cl->server->info);
		return;
	}
	qtv_printf ("not connected to a server");
}

static void
cl_download_f (client_t *cl, void *unused)
{
	qtv_printf ("download denied\n");
	MSG_ReliableWrite_Begin (&cl->backbuf, svc_download, 4);
	MSG_ReliableWrite_Short (&cl->backbuf, -1);
	MSG_ReliableWrite_Byte (&cl->backbuf, 0);
}

static void
cl_nextdl_f (client_t *cl, void *unused)
{
}

static void
cl_ptrack_f (client_t *cl, void *unused)
{
	int         i;

	if (Cmd_Argc () != 2) {
		cl->spec_track = 0;
		return;
	}
	i = atoi (Cmd_Argv (1));
	if (i < 0 || i >= MAX_CLIENTS) {
		cl->spec_track = 0;
		qtv_printf ("Invalid client to track\n");
		return;
	}
	cl->spec_track = 1 << i;
}

static void
cl_list_f (client_t *cl, void *unused)
{
	Server_List ();
}

static void
cl_connect_f (client_t *cl, void *unused)
{
	if (cl->server) {
		qtv_printf ("already connected to server %s\n", cl->server->name);
		qtv_printf ("\"cmd disconnect\" first\n");
		return;
	}
	Server_Connect (Cmd_Argv (1), cl);
}

static void
cl_disconnect_f (client_t *cl, void *unused)
{
	if (!cl->server) {
		qtv_printf ("not connected to a server\n");
		return;
	}
	client_drop (cl);
}

static ucmd_t ucmds[] = {
	{"new",			cl_new_f,			0, 0},
	{"modellist",	cl_modellist_f,		0, 0},
	{"soundlist",	cl_soundlist_f,		0, 0},
	{"prespawn",	cl_prespawn_f,		0, 0},
	{"spawn",		cl_spawn_f,			0, 0},
	{"begin",		cl_begin_f,			1, 0},

	{"drop",		cl_drop_f,			1, 0},
	{"pings",		cl_pings_f,			0, 0},

// issued by hand at client consoles
	{"rate",		cl_rate_f,			0, 0},
	{"kill",		0,					1, 1},
	{"pause",		0,					1, 0},
	{"msg",			0,					0, 0},

	{"say",			cl_say_f,			1, 1},
	{"say_team",	cl_say_f,			1, 1},

	{"setinfo",		cl_setinfo_f,		1, 0},

	{"serverinfo",	cl_serverinfo_f,	0, 0},

	{"download",	cl_download_f,		1, 0},
	{"nextdl",		cl_nextdl_f,		0, 0},

	{"ptrack",		cl_ptrack_f,		0, 1},		// ZOID - used with autocam

	{"snap",		0,					0, 0},

	{"list",		cl_list_f,			0, 0},
	{"connect",		cl_connect_f,		0, 0},
	{"disconnect",	cl_disconnect_f,	0, 0},
};

static hashtab_t *ucmd_table;
int (*ucmd_unknown)(void);

static void
client_exec_command (client_t *cl, const char *s)
{
	ucmd_t     *u;

	COM_TokenizeString (s, qtv_args);
	cmd_args = qtv_args;

	u = (ucmd_t*) Hash_Find (ucmd_table, qtv_args->argv[0]->str);

	if (!u) {
		if (!ucmd_unknown || !ucmd_unknown ()) {
			qtv_begin_redirect (RD_CLIENT, cl);
			qtv_printf ("Bad user command: %s\n", qtv_args->argv[0]->str);
			qtv_end_redirect ();
		}
	} else {
		if (u->func) {
			if (!u->no_redirect)
				qtv_begin_redirect (RD_CLIENT, cl);
			u->func (cl, u->userdata);
			if (!u->no_redirect)
				qtv_end_redirect ();
		}
	}
}

static void
spectator_move (client_t *cl, usercmd_t *ucmd)
{
	float       frametime = ucmd->msec * 0.001;
	float       control, drop, friction, fmove, smove, speed, newspeed;
	float       currentspeed, addspeed, accelspeed, wishspeed;
	int         i;
	vec3_t      wishdir, wishvel;
	vec3_t      forward, right, up;
	server_t   *sv = cl->server;

	AngleVectors (cl->state.cmd.angles, forward, right, up);

	speed = DotProduct (cl->state.es.velocity, cl->state.es.velocity);
	if (speed < 1) {
		VectorZero (cl->state.es.velocity);
	} else {
		speed = sqrt (speed);
		drop = 0;

		friction = sv->movevars.friction * 1.5;
		control = speed < sv->movevars.stopspeed ? sv->movevars.stopspeed
												 : speed;
		drop += control * friction * frametime;
		newspeed = speed - drop;
		if (newspeed < 0)
			newspeed = 0;
		newspeed /= speed;

		VectorScale (cl->state.es.velocity, newspeed, cl->state.es.velocity);
	}

	fmove = ucmd->forwardmove;
	smove = ucmd->sidemove;

	VectorNormalize (forward);
	VectorNormalize (right);

	for (i = 0; i < 3; i++)
		wishvel[i] = forward[i] * fmove + right[i] * smove;
	wishvel[2] += ucmd->upmove;

	VectorCopy (wishvel, wishdir);
	wishspeed = VectorNormalize (wishdir);

	if (wishspeed > sv->movevars.spectatormaxspeed) {
		VectorScale (wishvel, sv->movevars.spectatormaxspeed / wishspeed,
					 wishvel);
		wishspeed = sv->movevars.spectatormaxspeed;
	}

	currentspeed = DotProduct (cl->state.es.velocity, wishdir);
	addspeed = wishspeed - currentspeed;
	if (addspeed <= 0)
		return;
	accelspeed = sv->movevars.accelerate * frametime * wishspeed;
	if (accelspeed > addspeed)
		accelspeed = addspeed;

	VectorMultAdd (cl->state.es.velocity, accelspeed, wishdir,
				   cl->state.es.velocity);
	VectorMultAdd (cl->state.es.origin, frametime, cl->state.es.velocity,
				   cl->state.es.origin);
}

static void
run_command (client_t *cl, usercmd_t *ucmd)
{
	if (ucmd->msec > 50) {
		usercmd_t   cmd = *ucmd;
		int         oldmsec = ucmd->msec;
		cmd.msec = oldmsec / 2;
		run_command (cl, &cmd);
		cmd.msec = oldmsec / 2;
		cmd.impulse = 0;
		run_command (cl, &cmd);
		return;
	}

	VectorCopy (ucmd->angles, cl->state.cmd.angles);

	spectator_move (cl, ucmd);
}

static void
client_parse_message (client_t *cl)
{
	int         c, size;
	vec3_t      o;
	const char *s;
	usercmd_t   oldest, oldcmd, newcmd;
	byte        checksum, calculatedChecksum;
	int         checksumIndex, seq_hash;
	qboolean    move_issued = false;

	// make sure the reply sequence number matches the incoming
	// sequence number
	if (cl->netchan.incoming_sequence >= cl->netchan.outgoing_sequence)
		cl->netchan.outgoing_sequence = cl->netchan.incoming_sequence;
	else
		cl->send_message = false;		// don't reply, sequences have slipped

	seq_hash = cl->netchan.incoming_sequence;
	cl->delta_sequence = -1;
	while (1) {
		if (net_message->badread) {
			qtv_printf ("SV_ReadClientMessage: badread\n");
			client_drop (cl);
			return;
		}

		c = MSG_ReadByte (net_message);
		if (c == -1)
			return;

		switch (c) {
			default:
				qtv_printf ("SV_ReadClientMessage: unknown command char\n");
				client_drop (cl);
				return;
			case clc_nop:
				break;
			case clc_delta:
				cl->delta_sequence = MSG_ReadByte (net_message);
				break;
			case clc_move:
				checksumIndex = MSG_GetReadCount (net_message);
				checksum = MSG_ReadByte (net_message);
				// read loss percentage
				/*cl->lossage = */MSG_ReadByte (net_message);
				MSG_ReadDeltaUsercmd (net_message, &nullcmd, &oldest);
				MSG_ReadDeltaUsercmd (net_message, &oldest, &oldcmd);
				MSG_ReadDeltaUsercmd (net_message, &oldcmd, &newcmd);
				if (!cl->server)
					break;
				if (move_issued)
					break;				// someone is trying to cheat...
				move_issued = true;
//				if (cl->state != cs_spawned)
//					break;
				// if the checksum fails, ignore the rest of the packet
				calculatedChecksum =
					COM_BlockSequenceCRCByte (net_message->message->data +
											  checksumIndex + 1,
											  MSG_GetReadCount (net_message) -
											  checksumIndex - 1, seq_hash);
				if (calculatedChecksum != checksum) {
					Sys_MaskPrintf (SYS_DEV,
									"Failed command checksum for %s(%d) "
									"(%d != %d)\n",
									Info_ValueForKey (cl->userinfo, "name"),
									cl->netchan.incoming_sequence, checksum,
									calculatedChecksum);
					return;
				}
//				if (!sv.paused) {
//					SV_PreRunCmd ();
					if (cl->netchan.net_drop < 20) {
						while (cl->netchan.net_drop > 2) {
							run_command (cl, &cl->lastcmd);
							cl->netchan.net_drop--;
						}
						if (cl->netchan.net_drop > 1)
							run_command (cl, &oldest);
						if (cl->netchan.net_drop > 0)
							run_command (cl, &oldcmd);
					}
					run_command (cl, &newcmd);
//					SV_PostRunCmd ();
//				}
				cl->lastcmd = newcmd;
				cl->lastcmd.buttons = 0;	// avoid multiple fires on lag
				break;
			case clc_stringcmd:
				s = MSG_ReadString (net_message);
				client_exec_command (cl, s);
				break;
			case clc_tmove:
				MSG_ReadCoordV (net_message, o);
				VectorCopy (o, cl->state.es.origin);
				break;
			case clc_upload:
				size = MSG_ReadShort (net_message);
				MSG_ReadByte (net_message);
				net_message->readcount += size;
				break;
		}
	}
}

static void
write_player (int num, plent_state_t *pl, server_t *sv, sizebuf_t *msg)
{
	int         i;
	int         pflags = (pl->es.flags & (PF_GIB | PF_DEAD))
						| PF_MSEC | PF_COMMAND;
	int         qf_bits = 0;

	if (pl->es.modelindex != sv->playermodel)
		pflags |= PF_MODEL;
	for (i = 0; i < 3; i++)
		if (pl->es.velocity[i])
			pflags |= PF_VELOCITY1 << i;
	if (pl->es.effects & 0xff)
		pflags |= PF_EFFECTS;
	if (pl->es.skinnum)
		pflags |= PF_SKINNUM;

	qf_bits = 0;
#if 0
	if (client->stdver > 1) {
		if (sv_fields.alpha != -1 && pl->alpha)
			qf_bits |= PF_ALPHA;
		if (sv_fields.scale != -1 && pl->scale)
			qf_bits |= PF_SCALE;
		if (pl->effects & 0xff00)
			qf_bits |= PF_EFFECTS2;
		if (sv_fields.glow_size != -1 && pl->glow_size)
			qf_bits |= PF_GLOWSIZE;
		if (sv_fields.glow_color != -1 && pl->glow_color)
			qf_bits |= PF_GLOWCOLOR;
		if (sv_fields.colormod != -1
			&& !VectorIsZero (pl->colormod))
			qf_bits |= PF_COLORMOD;
		if (pl->frame & 0xff00)
			qf_bits |= PF_FRAME2;
		if (qf_bits)
			pflags |= PF_QF;
	}
#endif

//	if (cl->spectator) {
//		// send only origin and velocity to spectators
//		pflags &= PF_VELOCITY1 | PF_VELOCITY2 | PF_VELOCITY3;
//	} else if (ent == clent) {
//		// don't send a lot of data on personal entity
//		pflags &= ~(PF_MSEC | PF_COMMAND);
//		if (pl->es.weaponframe)
//			pflags |= PF_WEAPONFRAME;
//	}

//	if (client->spec_track && client->spec_track - 1 == j
//		&& pl->es.weaponframe)
//		pflags |= PF_WEAPONFRAME;

	MSG_WriteByte (msg, svc_playerinfo);
	MSG_WriteByte (msg, num);
	MSG_WriteShort (msg, pflags);

	MSG_WriteCoordV (msg, &pl->es.origin[0]);//FIXME
	pl->es.origin[3] = 1;

	MSG_WriteByte (msg, pl->es.frame);

	if (pflags & PF_MSEC) {
		//msec = 1000 * (sv.time - cl->localtime);
		//if (msec > 255)
		//	msec = 255;
		MSG_WriteByte (msg, pl->msec);
	}

	if (pflags & PF_COMMAND) {
		MSG_WriteDeltaUsercmd (msg, &nullcmd, &pl->cmd);
	}

	for (i = 0; i < 3; i++)
		if (pflags & (PF_VELOCITY1 << i))
			MSG_WriteShort (msg, pl->es.velocity[i]);

	if (pflags & PF_MODEL)
		MSG_WriteByte (msg, pl->es.modelindex);

	if (pflags & PF_SKINNUM)
		MSG_WriteByte (msg, pl->es.skinnum);

	if (pflags & PF_EFFECTS)
		MSG_WriteByte (msg, pl->es.effects);

	if (pflags & PF_WEAPONFRAME)
		MSG_WriteByte (msg, pl->es.weaponframe);

	if (pflags & PF_QF) {
		MSG_WriteByte (msg, qf_bits);
		if (qf_bits & PF_ALPHA)
			MSG_WriteByte (msg, pl->es.alpha);
		if (qf_bits & PF_SCALE)
			MSG_WriteByte (msg, pl->es.scale);
		if (qf_bits & PF_EFFECTS2)
			MSG_WriteByte (msg, pl->es.effects >> 8);
		if (qf_bits & PF_GLOWSIZE)
			MSG_WriteByte (msg, pl->es.glow_size);
		if (qf_bits & PF_GLOWCOLOR)
			MSG_WriteByte (msg, pl->es.glow_color);
		if (qf_bits & PF_COLORMOD)
			MSG_WriteByte (msg, pl->es.colormod);
		if (qf_bits & PF_FRAME2)
			MSG_WriteByte (msg, pl->es.frame >> 8);
	}
}
#if 0
#define MAX_NAILS   32
edict_t    *nails[MAX_NAILS];
int         numnails;
int         nailcount;

static void
emit_nails (sizebuf_t *msg, qboolean recorder)
{
	byte	   *buf;				// [48 bits] xyzpy 12 12 12 4 8
	int			n, p, x, y, z, yaw;
	int         bpn = recorder ? 7 : 6;	// bytes per nail
	edict_t	   *ent;

	if (!numnails)
		return;

	buf =  SZ_GetSpace (msg, numnails * bpn + 2);
	*buf++ = recorder ? svc_nails2 : svc_nails;
	*buf++ = numnails;

	for (n = 0; n < numnails; n++) {
		ent = nails[n];
		if (recorder) {
			if (!ent->colormap) {
				if (!((++nailcount) & 255))
					nailcount++;
				ent->colormap = nailcount&255;
			}
			*buf++ = (byte)ent->colormap;
		}

		x = ((int) (ent->origin[0] + 4096 + 1) >> 1) & 4095;
		y = ((int) (ent->origin[1] + 4096 + 1) >> 1) & 4095;
		z = ((int) (ent->origin[2] + 4096 + 1) >> 1) & 4095;
		p = (int) (ent->angles[0] * (16.0 / 360.0)) & 15;
		yaw = (int) (ent->angles[1] * (256.0 / 360.0)) & 255;

		*buf++ = x;
		*buf++ = (x >> 8) | (y << 4);
		*buf++ = (y >> 4);
		*buf++ = z;
		*buf++ = (z >> 8) | (p << 4);
		*buf++ = yaw;
	}
}
#endif
static void
write_delta (entity_state_t *from, entity_state_t *to, sizebuf_t *msg,
			 qboolean force)//, int stdver)
{
	int			bits, i;
	float		miss;

	// send an update
	bits = 0;

	for (i = 0; i < 3; i++) {
		miss = to->origin[i] - from->origin[i];
		if (miss < -0.1 || miss > 0.1)
			bits |= U_ORIGIN1 << i;
	}

	if (to->angles[0] != from->angles[0])
		bits |= U_ANGLE1;

	if (to->angles[1] != from->angles[1])
		bits |= U_ANGLE2;

	if (to->angles[2] != from->angles[2])
		bits |= U_ANGLE3;

	if (to->colormap != from->colormap)
		bits |= U_COLORMAP;

	if (to->skinnum != from->skinnum)
		bits |= U_SKIN;

	if (to->frame != from->frame)
		bits |= U_FRAME;

	if (to->effects != from->effects)
		bits |= U_EFFECTS;

	if (to->modelindex != from->modelindex)
		bits |= U_MODEL;

	// LordHavoc: cleaned up Endy's coding style, and added missing effects
// Ender (QSG - Begin)
#if 0
	if (stdver > 1) {
		if (to->alpha != from->alpha)
			bits |= U_ALPHA;

		if (to->scale != from->scale)
			bits |= U_SCALE;

		if (to->effects > 255)
			bits |= U_EFFECTS2;

		if (to->glow_size != from->glow_size)
			bits |= U_GLOWSIZE;

		if (to->glow_color != from->glow_color)
			bits |= U_GLOWCOLOR;

		if (to->colormod != from->colormod)
			bits |= U_COLORMOD;

		if (to->frame > 255)
			bits |= U_EFFECTS2;
	}
	if (bits >= 16777216)
		bits |= U_EXTEND2;

	if (bits >= 65536)
		bits |= U_EXTEND1;
#endif
	if (bits & 511)
		bits |= U_MOREBITS;

	if (to->flags & U_SOLID)
		bits |= U_SOLID;

	if (!bits && !force)
		return;							// nothing to send!
	i = to->number | (bits & ~511);
//	if (i & U_REMOVE)
//		Sys_Error ("U_REMOVE");
	MSG_WriteShort (msg, i);

	if (bits & U_MOREBITS)
		MSG_WriteByte (msg, bits & 255);

	// LordHavoc: cleaned up Endy's tabs
// Ender (QSG - Begin)
	if (bits & U_EXTEND1)
		MSG_WriteByte (msg, bits >> 16);
	if (bits & U_EXTEND2)
		MSG_WriteByte (msg, bits >> 24);
// Ender (QSG - End)

	if (bits & U_MODEL)
		MSG_WriteByte (msg, to->modelindex);
	if (bits & U_FRAME)
		MSG_WriteByte (msg, to->frame);
	if (bits & U_COLORMAP)
		MSG_WriteByte (msg, to->colormap);
	if (bits & U_SKIN)
		MSG_WriteByte (msg, to->skinnum);
	if (bits & U_EFFECTS)
		MSG_WriteByte (msg, to->effects);
	if (bits & U_ORIGIN1)
		MSG_WriteCoord (msg, to->origin[0]);
	if (bits & U_ANGLE1)
		MSG_WriteAngle (msg, to->angles[0]);
	if (bits & U_ORIGIN2)
		MSG_WriteCoord (msg, to->origin[1]);
	if (bits & U_ANGLE2)
		MSG_WriteAngle (msg, to->angles[1]);
	if (bits & U_ORIGIN3)
		MSG_WriteCoord (msg, to->origin[2]);
	if (bits & U_ANGLE3)
		MSG_WriteAngle (msg, to->angles[2]);

	// LordHavoc: cleaned up Endy's tabs, rearranged bytes, and implemented
	// missing effects
// Ender (QSG - Begin)
	if (bits & U_ALPHA)
		MSG_WriteByte (msg, to->alpha);
	if (bits & U_SCALE)
		MSG_WriteByte (msg, to->scale);
	if (bits & U_EFFECTS2)
		MSG_WriteByte (msg, (to->effects >> 8));
	if (bits & U_GLOWSIZE)
		MSG_WriteByte (msg, to->glow_size);
	if (bits & U_GLOWCOLOR)
		MSG_WriteByte (msg, to->glow_color);
	if (bits & U_COLORMOD)
		MSG_WriteByte (msg, to->colormod);
	if (bits & U_FRAME2)
		MSG_WriteByte (msg, (to->frame >> 8));
// Ender (QSG - End)
}

static void
emit_entities (client_t *client, packet_entities_t *to, sizebuf_t *msg)
{
	int         newindex, oldindex, newnum, oldnum, oldmax;
	entity_state_t *ent;
	frame_t    *fromframe;
	packet_entities_t *from;
	server_t   *sv = client->server;

	// this is the frame that we are going to delta update from
	if (client->delta_sequence != -1) {
		fromframe = &client->frames[client->delta_sequence & UPDATE_MASK];
		from = &fromframe->entities;
		oldmax = from->num_entities;

		MSG_WriteByte (msg, svc_deltapacketentities);
		MSG_WriteByte (msg, client->delta_sequence);
	} else {
		oldmax = 0;						// no delta update
		from = NULL;

		MSG_WriteByte (msg, svc_packetentities);
	}

	newindex = 0;
	oldindex = 0;
	while (newindex < to->num_entities || oldindex < oldmax) {
		newnum = newindex >= to->num_entities ? 9999 :
			to->entities[newindex].number;
		oldnum = oldindex >= oldmax ? 9999 : from->entities[oldindex].number;

		if (newnum == oldnum) {			// delta update from old position
			write_delta (&from->entities[oldindex], &to->entities[newindex],
						 msg, false);//, client->stdver);
			oldindex++;
			newindex++;
			continue;
		}

		if (newnum < oldnum) {
			// this is a new entity, send it from the baseline
			ent = sv->baselines + newnum;
			write_delta (ent, &to->entities[newindex], msg, true);//,
						 //client->stdver);
			newindex++;
			continue;
		}

		if (newnum > oldnum) {			// the old entity isn't present in
										// the new message
			MSG_WriteShort (msg, oldnum | U_REMOVE);
			oldindex++;
			continue;
		}
	}

	MSG_WriteShort (msg, 0);			// end of packetentities
}

static void
write_entities (client_t *client, sizebuf_t *msg)
{
	//byte       *pvs = 0;
	//int         i;
	int         e;
	//vec3_t      org;
	frame_t    *frame;
	entity_state_t *ent;
	entity_state_t *state;
	packet_entities_t *pack;
	server_t   *sv = client->server;

	// this is the frame we are creating
	frame = &client->frames[client->netchan.incoming_sequence & UPDATE_MASK];

	// find the client's PVS
	//clent = client->edict;
	//VectorAdd (SVvector (clent, origin), SVvector (clent, view_ofs), org);
	//pvs = SV_FatPVS (org);

	// put other visible entities into either a packet_entities or a nails
	// message
	pack = &frame->entities;
	pack->num_entities = 0;

	//numnails = 0;

	for (e = MAX_CLIENTS + 1, ent = sv->entities + e; e < MAX_SV_ENTITIES;
		 e++, ent++) {
		if (!sv->ent_valid[e])
			continue;
		if (ent->number && ent->number != e)
			qtv_printf ("%d %d\n", e, ent->number);
#if 0
		if (pvs) {
			// ignore if not touching a PV leaf
			for (i = 0; i < ent->num_leafs; i++)
				if (pvs[ent->leafnums[i] >> 3] & (1 << (ent->leafnums[i] & 7)))
					break;

			if (i == ent->num_leafs)
				continue;					// not visible
		}
#endif
//		if (SV_AddNailUpdate (ent))
//			continue;					// added to the special update list

		// add to the packetentities
		if (pack->num_entities == MAX_PACKET_ENTITIES) {
			qtv_printf ("mpe overflow\n");
			continue;					// all full
		}

		state = &pack->entities[pack->num_entities];
		pack->num_entities++;
		*state = *ent;
		state->flags = 0;
	}
	// encode the packet entities as a delta from the
	// last packetentities acknowledged by the client
	emit_entities (client, pack, msg);

	// now add the specialized nail update
	//emit_nails (msg, 0);
}

static void
write_players (client_t *client, sizebuf_t *msg)
{
	server_t   *sv = client->server;
	int         i;

	for (i = 0; i < MAX_SV_PLAYERS; i++) {
		if (!sv->players[i].info)
			continue;
		write_player (i, &sv->players[i].ent, sv, msg);
	}
}

void
Client_SendMessages (client_t *cl)
{
	byte        buf[MAX_DATAGRAM];
	sizebuf_t   msg;

	memset (&msg, 0, sizeof (msg));
	msg.allowoverflow = true;
	msg.maxsize = sizeof (buf);
	msg.data = buf;

	if (cl->backbuf.num_backbuf)
		MSG_Reliable_Send (&cl->backbuf);
	if (cl->connected) {
		write_players (cl, &msg);
		write_player (31, &cl->state, cl->server, &msg);
		write_entities (cl, &msg);
		if (cl->datagram.cursize) {
			SZ_Write (&msg, cl->datagram.data, cl->datagram.cursize);
			SZ_Clear (&cl->datagram);
		}
	}
	Netchan_Transmit (&cl->netchan, msg.cursize, msg.data);
}

static void
client_handler (connection_t *con, void *object)
{
	client_t   *cl = (client_t *) object;

	if (net_message->message->cursize < 11) {
		qtv_printf ("%s: Runt packet\n", NET_AdrToString (net_from));
		return;
	}
	if (Netchan_Process (&cl->netchan)) {
		// this is a valid, sequenced packet, so process it
		//svs.stats.packets++;
		cl->send_message = true;
		//if (cl->state != cs_zombie)
			client_parse_message (cl);
	}
}

static void
client_connect (connection_t *con, void *object)
{
	challenge_t *ch = (challenge_t *) object;
	client_t   *cl;
	const char *str;
	info_t     *userinfo;
	int         version, qport, challenge, seq;
	int         i;

	MSG_BeginReading (net_message);
	seq = MSG_ReadLong (net_message);
	if (seq != -1) {
		qtv_printf ("unexpected connected packet\n");
		return;
	}
	str = MSG_ReadString (net_message);
	COM_TokenizeString (str, qtv_args);
	cmd_args = qtv_args;
	if (strcmp (Cmd_Argv (0), "connect")) {
		qtv_printf ("unexpected connected packet\n");
		return;
	}
	version = atoi (Cmd_Argv (1));
	if (version != PROTOCOL_VERSION) {
		Netchan_OutOfBandPrint (net_from, "%c\nServer is version %s.\n",
								A2C_PRINT, QW_VERSION);
		qtv_printf ("* rejected connect from version %i\n", version);
		return;
	}
	qport = atoi (Cmd_Argv (2));
	challenge = atoi (Cmd_Argv (3));
	if (!(con = Connection_Find (&net_from))
		|| (ch = con->object)->challenge != challenge) {
		Netchan_OutOfBandPrint (net_from, "%c\nBad challenge.\n",
								A2C_PRINT);
		return;
	}
	free (con->object);
	userinfo = Info_ParseString (Cmd_Argv (4), 0, 0);
	if (!userinfo) {
		Netchan_OutOfBandPrint (net_from, "%c\nInvalid userinfo string.\n",
								A2C_PRINT);
		return;
	}

	cl = calloc (1, sizeof (client_t));
	client_count++;
	Netchan_Setup (&cl->netchan, con->address, qport, NC_QPORT_READ);
	cl->clnext = clients;
	clients = cl;
	cl->userinfo = userinfo;
	cl->name = Info_ValueForKey (userinfo, "name");
	cl->backbuf.name = cl->name;
	cl->backbuf.netchan = &cl->netchan;
	cl->con = con;
	con->object = cl;
	con->handler = client_handler;

	cl->datagram.allowoverflow = true;
	cl->datagram.maxsize = sizeof (cl->datagram_buf);
	cl->datagram.data = cl->datagram_buf;

	for (i = 0; i < UPDATE_BACKUP; i++)
		cl->frames[i].entities.entities = cl->packet_entities[i];

	qtv_printf ("client %s (%s) connected\n",
				Info_ValueForKey (userinfo, "name"),
				NET_AdrToString (con->address));

	Netchan_OutOfBandPrint (net_from, "%c", S2C_CONNECTION);
}

void
Client_NewConnection (void)
{
	challenge_t *ch;
	connection_t *con;

	if ((con = Connection_Find (&net_from))) {
		if (con->handler == client_handler)
			return;
		ch = con->object;
	} else {
		ch = malloc (sizeof (challenge_t));
	}
	ch->challenge = (rand () << 16) ^ rand ();
	ch->time = realtime;
	if (!con)
		con = Connection_Add (&net_from, ch, 0);
	Netchan_OutOfBandPrint (net_from, "%c%i QF", S2C_CHALLENGE, ch->challenge);
	con->handler = client_connect;
}

static const char *
ucmds_getkey (const void *_a, void *unused)
{
	ucmd_t *a = (ucmd_t*)_a;
	return a->name;
}

void
Client_Init (void)
{
	size_t      i;

	ucmd_table = Hash_NewTable (251, ucmds_getkey, 0, 0, 0);
	for (i = 0; i < sizeof (ucmds) / sizeof (ucmds[0]); i++)
		Hash_Add (ucmd_table, &ucmds[i]);
}

void
Client_New (client_t *cl)
{
	server_t   *sv = cl->server;

	MSG_WriteByte (&cl->netchan.message, svc_serverdata);
	MSG_WriteLong (&cl->netchan.message, PROTOCOL_VERSION);
	MSG_WriteLong (&cl->netchan.message, sv->spawncount);
	MSG_WriteString (&cl->netchan.message, sv->gamedir);
	MSG_WriteByte (&cl->netchan.message, 0x80 | 31);
	MSG_WriteString (&cl->netchan.message, sv->message);
	MSG_WriteFloat (&cl->netchan.message, sv->movevars.gravity);
	MSG_WriteFloat (&cl->netchan.message, sv->movevars.stopspeed);
	MSG_WriteFloat (&cl->netchan.message, sv->movevars.maxspeed);
	MSG_WriteFloat (&cl->netchan.message, sv->movevars.spectatormaxspeed);
	MSG_WriteFloat (&cl->netchan.message, sv->movevars.accelerate);
	MSG_WriteFloat (&cl->netchan.message, sv->movevars.airaccelerate);
	MSG_WriteFloat (&cl->netchan.message, sv->movevars.wateraccelerate);
	MSG_WriteFloat (&cl->netchan.message, sv->movevars.friction);
	MSG_WriteFloat (&cl->netchan.message, sv->movevars.waterfriction);
	MSG_WriteFloat (&cl->netchan.message, sv->movevars.entgravity);
	MSG_WriteByte (&cl->netchan.message, svc_cdtrack);
	MSG_WriteByte (&cl->netchan.message, sv->cdtrack);
	MSG_WriteByte (&cl->netchan.message, svc_stufftext);
	MSG_WriteString (&cl->netchan.message,
					 va (0, "fullserverinfo \"%s\"\n",
						 Info_MakeString (sv->info, 0)));
}

static void
delete_client (client_t *cl)
{
	if (cl->server)
		Server_Disconnect (cl);
	Connection_Del (cl->con);
	Info_Destroy (cl->userinfo);
	client_count--;
	free (cl);
}

void
Client_Frame (void)
{
	client_t  **c;

	for (c = &clients; *c; ) {
		client_t   *cl = *c;

		if (realtime - cl->netchan.last_received > 60) {
			qtv_printf ("client %s timed out\n", (*c)->name);
			client_drop (cl);
		}
		if (cl->netchan.message.overflowed) {
			qtv_printf ("client %s overflowed\n", cl->name);
			client_drop (cl);
		}
		if (cl->drop) {
			qtv_printf ("client %s dropped\n", cl->name);
			*c = cl->clnext;
			delete_client (cl);
			continue;
		}
		if (cl->send_message) {
			Client_SendMessages (cl);
			cl->send_message = false;
		}
		c = &(*c)->clnext;
	}
}
