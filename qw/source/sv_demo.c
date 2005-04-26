/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((unused)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif
#ifdef HAVE_UNISTD_H
# include "unistd.h"
#endif
#include <sys/time.h>
#include <time.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/info.h"
#include "QF/msg.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "pmove.h"
#include "server.h"
#include "sv_demo.h"
#include "sv_progs.h"

demo_t      demo;

#define MIN_DEMO_MEMORY 0x100000
#define USACACHE (sv_demoUseCache->int_val && svs.demomemsize)
#define DWRITE(a,b,d) dwrite((QFile *) d, a, b)
#define MAXSIZE (demo.dbuffer.end < demo.dbuffer.last ? \
				 demo.dbuffer.start - demo.dbuffer.end : \
				 demo.dbuffer.maxsize - demo.dbuffer.end)

static int  demo_max_size;
static int  demo_size;
cvar_t     *sv_demoUseCache;
cvar_t     *sv_demoCacheSize;
cvar_t     *sv_demoMaxDirSize;
cvar_t     *sv_demoDir;
cvar_t     *sv_demofps;
cvar_t     *sv_demoPings;
cvar_t     *sv_demoNoVis;
cvar_t     *sv_demoMaxSize;
cvar_t     *sv_demoPrefix;
cvar_t     *sv_demoSuffix;
cvar_t     *sv_onrecordfinish;
cvar_t     *sv_ondemoremove;
cvar_t     *sv_demotxt;
cvar_t     *serverdemo;

int         (*dwrite) (QFile * file, const void *buf, int count);

#define HEADER  ((int) &((header_t *) 0)->data)

entity_state_t demo_entities[UPDATE_MASK + 1][MAX_DEMO_PACKET_ENTITIES];

/*
	SV_WriteDemoMessage

	Dumps the current net message, prefixed by the length and view angles
*/
static void
SV_WriteDemoMessage (sizebuf_t *msg, int type, int to, float time)
{
	int         len, i, msec;
	byte        c;
	static double prevtime;

	msec = (time - prevtime) * 1000;
	prevtime += msec * 0.001;
	if (msec > 255)
		msec = 255;
	if (msec < 2)
		msec = 0;

	c = msec;
	demo.size += DWRITE (&c, sizeof (c), demo.dest);

	if (demo.lasttype != type || demo.lastto != to) {
		demo.lasttype = type;
		demo.lastto = to;
		switch (demo.lasttype) {
			case dem_all:
				c = dem_all;
				demo.size += DWRITE (&c, sizeof (c), demo.dest);
				break;
			case dem_multiple:
				c = dem_multiple;
				demo.size += DWRITE (&c, sizeof (c), demo.dest);

				i = LittleLong (demo.lastto);
				demo.size += DWRITE (&i, sizeof (i), demo.dest);
				break;
			case dem_single:
			case dem_stats:
				c = demo.lasttype + (demo.lastto << 3);
				demo.size += DWRITE (&c, sizeof (c), demo.dest);
				break;
			default:
				SV_Stop (0);
				Con_Printf ("bad demo message type:%d", type);
				return;
		}
	} else {
		c = dem_read;
		demo.size += DWRITE (&c, sizeof (c), demo.dest);
	}


	len = LittleLong (msg->cursize);
	demo.size += DWRITE (&len, 4, demo.dest);
	demo.size += DWRITE (msg->data, msg->cursize, demo.dest);

	if (demo.disk)
		Qflush (demo.file);
	else if (demo.size - demo_size > demo_max_size) {
		demo_size = demo.size;
		demo.mfile -= 0x80000;
		Qwrite (demo.file, svs.demomem, 0x80000);
		Qflush (demo.file);
		memmove (svs.demomem, svs.demomem + 0x80000, demo.size - 0x80000);
	}
}

/*
	DemoWriteToDisk

	Writes to disk a message meant for specifc client
	or all messages if type == 0
	Message is cleared from demobuf after that
*/

static void
SV_DemoWriteToDisk (int type, int to, float time)
{
	int         pos = 0, oldm, oldd;
	header_t   *p;
	int         size;
	sizebuf_t   msg;

	p = (header_t *) demo.dbuf->sz.data;
	demo.dbuf->h = NULL;

	oldm = demo.dbuf->bufsize;
	oldd = demo.dbuffer.start;
	while (pos < demo.dbuf->bufsize) {
		size = p->size;
		pos += HEADER + size;

		// no type means we are writing to disk everything
		if (!type || (p->type == type && p->to == to)) {
			if (size) {
				msg.data = p->data;
				msg.cursize = size;

				SV_WriteDemoMessage (&msg, p->type, p->to, time);
			}
			// data is written so it need to be cleard from demobuf
			if (demo.dbuf->sz.data != (byte *) p)
				memmove (demo.dbuf->sz.data + size + HEADER,
						 demo.dbuf->sz.data, (byte *) p - demo.dbuf->sz.data);

			demo.dbuf->bufsize -= size + HEADER;
			demo.dbuf->sz.data += size + HEADER;
			pos -= size + HEADER;
			demo.dbuf->sz.maxsize -= size + HEADER;
			demo.dbuffer.start += size + HEADER;
		}
		// move along
		p = (header_t *) (p->data + size);
	}

	if (demo.dbuffer.start == demo.dbuffer.last) {
		if (demo.dbuffer.start == demo.dbuffer.end) {
			demo.dbuffer.end = 0;		// demo.dbuffer is empty
			demo.dbuf->sz.data = demo.dbuffer.data;
		}
		// go back to begining of the buffer
		demo.dbuffer.last = demo.dbuffer.end;
		demo.dbuffer.start = 0;
	}
}

/*
	SV_DemoWritePackets

	Interpolates to get exact players position for current frame
	and writes packets to the disk/memory
*/

static float
adjustangle (float current, float ideal, float fraction)
{
	float       move;

	move = ideal - current;
	if (ideal > current) {

		if (move >= 180)
			move = move - 360;
	} else {
		if (move <= -180)
			move = move + 360;
	}

	move *= fraction;

	return (current + move);
}

void
SV_DemoWritePackets (int num)
{
	demo_frame_t *frame, *nextframe;
	demo_client_t *cl, *nextcl = 0;
	int         i, j, flags;
	qboolean    valid;
	double      time, playertime, nexttime;
	float       f;
	vec3_t      origin, angles;
	sizebuf_t   msg;
	byte        msg_buf[MAX_MSGLEN];
	demoinfo_t *demoinfo;

	msg.data = msg_buf;
	msg.maxsize = sizeof (msg_buf);

	if (num > demo.parsecount - demo.lastwritten + 1)
		num = demo.parsecount - demo.lastwritten + 1;

	// 'num' frames to write
	for (; num; num--, demo.lastwritten++) {
		frame = &demo.frames[demo.lastwritten & DEMO_FRAMES_MASK];
		time = frame->time;
		nextframe = frame;
		msg.cursize = 0;

		demo.dbuf = &frame->buf;

		// find two frames
		// one before the exact time (time - msec) and one after,
		// then we can interpolate exact position for current frame
		for (i = 0, cl = frame->clients, demoinfo = demo.info; i < MAX_CLIENTS;
			 i++, cl++, demoinfo++) {
			if (cl->parsecount != demo.lastwritten)
				continue;				// not valid

			nexttime = playertime = time - cl->sec;

			for (j = demo.lastwritten + 1, valid = false;
				 nexttime < time && j < demo.parsecount; j++) {
				nextframe = &demo.frames[j & DEMO_FRAMES_MASK];
				nextcl = &nextframe->clients[i];

				if (nextcl->parsecount != j)
					break;				// disconnected?
				if (nextcl->fixangle)
					break;				// respawned, or walked into
										// teleport, do not interpolate!
				if (!(nextcl->flags & DF_DEAD) && (cl->flags & DF_DEAD))
					break;				// respawned, do not interpolate

				nexttime = nextframe->time - nextcl->sec;

				if (nexttime >= time) {
					// good, found what we were looking for
					valid = true;
					break;
				}
			}

			if (valid) {
				f = (time - nexttime) / (nexttime - playertime);
				for (j = 0; j < 3; j++) {
					angles[j] = adjustangle (cl->info.angles[j],
											 nextcl->info.angles[j], 1.0 + f);
					origin[j] = (nextcl->info.origin[j]
							     + f * (nextcl->info.origin[j]
									    - cl->info.origin[j]));
				}
			} else {
				VectorCopy (cl->info.origin, origin);
				VectorCopy (cl->info.angles, angles);
			}

			// now write it to buf
			flags = cl->flags;

			if (cl->fixangle) {
				demo.fixangletime[i] = cl->cmdtime;
			}

			for (j = 0; j < 3; j++)
				if (origin[j] != demoinfo->origin[j])
					flags |= DF_ORIGIN << j;

			if (cl->fixangle || demo.fixangletime[i] != cl->cmdtime) {
				for (j = 0; j < 3; j++)
					if (angles[j] != demoinfo->angles[j])
						flags |= DF_ANGLES << j;
			}

			if (cl->info.model != demoinfo->model)
				flags |= DF_MODEL;
			if (cl->info.effects != demoinfo->effects)
				flags |= DF_EFFECTS;
			if (cl->info.skinnum != demoinfo->skinnum)
				flags |= DF_SKINNUM;
			if (cl->info.weaponframe != demoinfo->weaponframe)
				flags |= DF_WEAPONFRAME;

			MSG_WriteByte (&msg, svc_playerinfo);
			MSG_WriteByte (&msg, i);
			MSG_WriteShort (&msg, flags);

			MSG_WriteByte (&msg, cl->frame);

			for (j = 0; j < 3; j++)
				if (flags & (DF_ORIGIN << j))
					MSG_WriteCoord (&msg, origin[j]);

			for (j = 0; j < 3; j++)
				if (flags & (DF_ANGLES << j))
					MSG_WriteAngle16 (&msg, angles[j]);

			if (flags & DF_MODEL)
				MSG_WriteByte (&msg, cl->info.model);

			if (flags & DF_SKINNUM)
				MSG_WriteByte (&msg, cl->info.skinnum);

			if (flags & DF_EFFECTS)
				MSG_WriteByte (&msg, cl->info.effects);

			if (flags & DF_WEAPONFRAME)
				MSG_WriteByte (&msg, cl->info.weaponframe);

			VectorCopy (cl->info.origin, demoinfo->origin);
			VectorCopy (cl->info.angles, demoinfo->angles);
			demoinfo->skinnum = cl->info.skinnum;
			demoinfo->effects = cl->info.effects;
			demoinfo->weaponframe = cl->info.weaponframe;
			demoinfo->model = cl->info.model;
		}

		// this goes first to reduce demo size a bit
		SV_DemoWriteToDisk (demo.lasttype, demo.lastto, time);
		SV_DemoWriteToDisk (0, 0, time);			// now goes the rest
		if (msg.cursize)
			SV_WriteDemoMessage (&msg, dem_all, 0, time);
	}

	if (demo.lastwritten > demo.parsecount)
		demo.lastwritten = demo.parsecount;

	demo.dbuf = &demo.frames[demo.parsecount & DEMO_FRAMES_MASK].buf;
	demo.dbuf->sz.maxsize = MAXSIZE + demo.dbuf->bufsize;
}

static int
memwrite (QFile *_mem, const void *buffer, int size)
{
	byte      **mem = (byte **) _mem;

	memcpy (*mem, buffer, size);
	*mem += size;

	return size;
}

/*
	DemoSetBuf

	Sets position in the buf for writing to specific client
*/

static void
DemoSetBuf (byte type, int to)
{
	header_t   *p;
	int         pos = 0;

	p = (header_t *) demo.dbuf->sz.data;

	while (pos < demo.dbuf->bufsize) {
		pos += HEADER + p->size;

		if (type == p->type && to == p->to && !p->full) {
			demo.dbuf->sz.cursize = pos;
			demo.dbuf->h = p;
			return;
		}

		p = (header_t *) (p->data + p->size);
	}
	// type && to not exist in the buf, so add it

	p->type = type;
	p->to = to;
	p->size = 0;
	p->full = 0;

	demo.dbuf->bufsize += HEADER;
	demo.dbuf->sz.cursize = demo.dbuf->bufsize;
	demo.dbuffer.end += HEADER;
	demo.dbuf->h = p;
}

static void
DemoMoveBuf (void)
{
	// set the last message mark to the previous frame (i/e begining of this
	// one)
	demo.dbuffer.last = demo.dbuffer.end - demo.dbuf->bufsize;

	// move buffer to the begining of demo buffer
	memmove (demo.dbuffer.data, demo.dbuf->sz.data, demo.dbuf->bufsize);
	demo.dbuf->sz.data = demo.dbuffer.data;
	demo.dbuffer.end = demo.dbuf->bufsize;
	demo.dbuf->h = NULL;				// it will be setup again
	demo.dbuf->sz.maxsize = MAXSIZE + demo.dbuf->bufsize;
}

void
DemoWrite_Begin (byte type, int to, int size)
{
	byte       *p;
	qboolean    move = false;

	// will it fit?
	while (demo.dbuf->bufsize + size + HEADER > demo.dbuf->sz.maxsize) {
		// if we reached the end of buffer move msgbuf to the begining
		if (!move && demo.dbuffer.end > demo.dbuffer.start)
			move = true;

		SV_DemoWritePackets (1);
		if (move && demo.dbuffer.start > demo.dbuf->bufsize + HEADER + size)
			DemoMoveBuf ();
	}

	if (demo.dbuf->h == NULL || demo.dbuf->h->type != type
		|| demo.dbuf->h->to != to || demo.dbuf->h->full) {
		DemoSetBuf (type, to);
	}

	if (demo.dbuf->h->size + size > MAX_MSGLEN) {
		demo.dbuf->h->full = 1;
		DemoSetBuf (type, to);
	}
	// we have to make room for new data
	if (demo.dbuf->sz.cursize != demo.dbuf->bufsize) {
		p = demo.dbuf->sz.data + demo.dbuf->sz.cursize;
		memmove (p + size, p, demo.dbuf->bufsize - demo.dbuf->sz.cursize);
	}

	demo.dbuf->bufsize += size;
	demo.dbuf->h->size += size;
	if ((demo.dbuffer.end += size) > demo.dbuffer.last)
		demo.dbuffer.last = demo.dbuffer.end;
}

/*
	SV_Stop

	stop recording a demo
*/
void
SV_Stop (int reason)
{
	if (!sv.demorecording) {
		Con_Printf ("Not recording a demo.\n");
		return;
	}

	if (reason == 2) {
		// stop and remove
		if (demo.disk)
			Qclose (demo.file);

		QFS_Remove (demo.name->str);
		QFS_Remove (demo.text->str);

		demo.file = NULL;
		sv.demorecording = false;

		SV_BroadcastPrintf (PRINT_CHAT,
							"Server recording canceled, demo removed\n");

		Cvar_Set (serverdemo, "");

		return;
	}
	// write a disconnect message to the demo file

	// clearup to be sure message will fit
	demo.dbuf->sz.cursize = 0;
	demo.dbuf->h = NULL;
	demo.dbuf->bufsize = 0;
	DemoWrite_Begin (dem_all, 0, 2 + strlen ("EndOfDemo"));
	MSG_WriteByte (&demo.dbuf->sz, svc_disconnect);
	MSG_WriteString (&demo.dbuf->sz, "EndOfDemo");

	SV_DemoWritePackets (demo.parsecount - demo.lastwritten + 1);
	// finish up
	if (!demo.disk) {
		Qwrite (demo.file, svs.demomem, demo.size - demo_size);
		Qflush (demo.file);
	}

	Qclose (demo.file);

	demo.file = NULL;
	sv.demorecording = false;
	if (!reason)
		SV_BroadcastPrintf (PRINT_CHAT, "Server recording completed\n");
	else
		SV_BroadcastPrintf (PRINT_CHAT, "Server recording stoped\n"
							"Max demo size exceeded\n");
/*
	if (sv_onrecordfinish->string[0]) {
		extern redirect_t sv_redirected;
		int         old = sv_redirected;
		char        path[MAX_OSPATH];
		char       *p;

		if ((p = strstr (sv_onrecordfinish->string, " ")) != NULL)
			*p = 0;						// strip parameters

		strcpy (path, demo.name->str);
		strcpy (path + strlen (demo.name->str) - 3, "txt");

		sv_redirected = RD_NONE;		// onrecord script is called always
										// from the console
		Cmd_TokenizeString (va ("script %s \"%s\" \"%s\" \"%s\" %s",
								sv_onrecordfinish->string, demo.path->str,
								serverdemo->string,
								path, p != NULL ? p + 1 : ""));
		if (p)
			*p = ' ';
		SV_Script_f ();

		sv_redirected = old;
	}
*/
	Cvar_Set (serverdemo, "");
}

static void
SV_Stop_f (void)
{
	SV_Stop (0);
}

/*
	SV_Cancel_f

	Stops recording, and removes the demo
*/
static void
SV_Cancel_f (void)
{
	SV_Stop (2);
}

void
SV_DemoPings (void)
{
	client_t   *client;
	int         j;

	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++) {
		if (client->state != cs_spawned && client->state != cs_server)
			continue;

		DemoWrite_Begin (dem_all, 0, 7);
		MSG_WriteByte (&demo.dbuf->sz, svc_updateping);
		MSG_WriteByte (&demo.dbuf->sz, j);
		MSG_WriteShort (&demo.dbuf->sz, SV_CalcPing (client));
		MSG_WriteByte (&demo.dbuf->sz, svc_updatepl);
		MSG_WriteByte (&demo.dbuf->sz, j);
		MSG_WriteByte (&demo.dbuf->sz, client->lossage);
	}
}

static void
DemoBuffer_Init (dbuffer_t *dbuffer, byte *buf, size_t size)
{
	dbuffer->data = buf;
	dbuffer->maxsize = size;
	dbuffer->start = 0;
	dbuffer->end = 0;
	dbuffer->last = 0;
}

/*
	Demo_SetMsgBuf

	Sets the frame message buffer
*/

void
DemoSetMsgBuf (demobuf_t *prev, demobuf_t *cur)
{
	// fix the maxsize of previous msg buffer,
	// we won't be able to write there anymore
	if (prev != NULL)
		prev->sz.maxsize = prev->bufsize;

	demo.dbuf = cur;
	memset (demo.dbuf, 0, sizeof (*demo.dbuf));

	demo.dbuf->sz.data = demo.dbuffer.data + demo.dbuffer.end;
	demo.dbuf->sz.maxsize = MAXSIZE;
}

static qboolean
SV_InitRecord (void)
{
	if (!USACACHE) {
		dwrite = &Qwrite;
		demo.dest = demo.file;
		demo.disk = true;
	} else {
		dwrite = &memwrite;
		demo.mfile = svs.demomem;
		demo.dest = &demo.mfile;
	}

	demo_size = 0;

	return true;
}

/*
	SV_WriteRecordDemoMessage

	Dumps the current net message, prefixed by the length and view angles
*/

static void
SV_WriteRecordDemoMessage (sizebuf_t *msg)
{
	int         len;
	byte        c;

	c = 0;
	demo.size += DWRITE (&c, sizeof (c), demo.dest);

	c = dem_read;
	demo.size += DWRITE (&c, sizeof (c), demo.dest);

	len = LittleLong (msg->cursize);
	demo.size += DWRITE (&len, 4, demo.dest);

	demo.size += DWRITE (msg->data, msg->cursize, demo.dest);

	if (demo.disk)
		Qflush (demo.file);
}

static void
SV_WriteSetDemoMessage (void)
{
	int         len;
	byte        c;

	c = 0;
	demo.size += DWRITE (&c, sizeof (c), demo.dest);

	c = dem_set;
	demo.size += DWRITE (&c, sizeof (c), demo.dest);


	len = LittleLong (0);
	demo.size += DWRITE (&len, 4, demo.dest);
	len = LittleLong (0);
	demo.size += DWRITE (&len, 4, demo.dest);

	if (demo.disk)
		Qflush (demo.file);
}

static const char *
SV_PrintTeams (void)
{
	const char *teams[MAX_CLIENTS];
	char       *p;
	int         i, j, numcl = 0, numt = 0;
	client_t   *clients[MAX_CLIENTS];
	const char *team;
	static dstring_t *buffer;

	if (!buffer)
		buffer = dstring_new ();

	// count teams and players
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (svs.clients[i].state != cs_spawned
			&& svs.clients[i].state != cs_server)
			continue;
		if (svs.clients[i].spectator)
			continue;

		team = Info_ValueForKey (svs.clients[i].userinfo, "team");
		clients[numcl++] = &svs.clients[i];

		for (j = 0; j < numt; j++)
			if (!strcmp (team, teams[j]))
				break;
		if (j != numt)
			continue;

		teams[numt++] = team;
	}

	// create output

	if (numcl == 2) {					// duel
		dsprintf (buffer, "team1 %s\nteam2 %s\n", clients[0]->name,
				  clients[1]->name);
	} else if (!teamplay->int_val) {	// ffa
		dsprintf (buffer, "players:\n");
		for (i = 0; i < numcl; i++)
			dasprintf (buffer, "  %s\n", clients[i]->name);
	} else {							// teamplay
		for (j = 0; j < numt; j++) {
			dasprintf (buffer, "team %s:\n", teams[j]);
			for (i = 0; i < numcl; i++) {
				team = Info_ValueForKey (svs.clients[i].userinfo, "team");
				if (!strcmp (team, teams[j]))
					dasprintf (buffer, "  %s\n", clients[i]->name);
			}
		}
	}

	if (!numcl)
		return "\n";
	for (p = buffer->str; *p; p++)
		*p = sys_char_map[(byte) * p];
	return buffer->str;
}

static int
make_info_string_filter (const char *key)
{
	return *key == '_' || !Info_FilterForKey (key, client_info_filters);
}

static void
SV_Record (char *name)
{
	sizebuf_t   buf;
	char        buf_data[MAX_MSGLEN];
	int         n, i;
	const char *info;

	client_t   *player;
	const char *gamedir, *s;

	{
		// save over memset
		dstring_t  *tn = demo.name, *tt = demo.text;
		memset (&demo, 0, sizeof (demo));
		dstring_clearstr (demo.name = tn);
		dstring_clearstr (demo.text = tt);
	}
	for (i = 0; i < UPDATE_BACKUP; i++)
		demo.recorder.frames[i].entities.entities = demo_entities[i];

	DemoBuffer_Init (&demo.dbuffer, demo.buffer, sizeof (demo.buffer));
	DemoSetMsgBuf (NULL, &demo.frames[0].buf);

	demo.datagram.maxsize = sizeof (demo.datagram_data);
	demo.datagram.data = demo.datagram_data;

	demo.file = QFS_Open (name, "wb");
	if (!demo.file) {
		Con_Printf ("ERROR: couldn't open %s\n", name);
		return;
	}

	SV_InitRecord ();

	s = name + strlen (name);
	while (s > name && *s != '/')
		s--;
	dstring_copystr (demo.name, s + (*s == '/'));

	SV_BroadcastPrintf (PRINT_CHAT, "Server started recording (%s):\n%s\n",
						demo.disk ? "disk" : "memory", demo.name->str);
	Cvar_Set (serverdemo, demo.name->str);

	dstring_copystr (demo.text, name);
	strcpy (demo.text->str + strlen (demo.text->str) - 3, "txt");

	if (sv_demotxt->int_val) {
		QFile      *f;

		f = QFS_Open (demo.text->str, "w+t");
		if (f != NULL) {
			char        date[20];
			time_t      tim;

			time (&tim);
			strftime (date, sizeof (date), "%Y-%m-%d-%H-%M", localtime (&tim));
			Qprintf (f, "date %s\nmap %s\nteamplay %d\ndeathmatch %d\n"
					 "timelimit %d\n%s",
					 date, sv.name, teamplay->int_val,
					 deathmatch->int_val, timelimit->int_val, 
					 SV_PrintTeams ());
			Qclose (f);
		}
	} else
		QFS_Remove (demo.text->str);

	sv.demorecording = true;
	demo.pingtime = demo.time = sv.time;

/*-------------------------------------------------*/

	// serverdata
	// send the info about the new client to all connected clients
	memset (&buf, 0, sizeof (buf));
	buf.data = buf_data;
	buf.maxsize = sizeof (buf_data);

	// send the serverdata

	gamedir = Info_ValueForKey (svs.info, "*gamedir");
	if (!gamedir[0])
		gamedir = "qw";

	MSG_WriteByte (&buf, svc_serverdata);
	MSG_WriteLong (&buf, PROTOCOL_VERSION);
	MSG_WriteLong (&buf, svs.spawncount);
	MSG_WriteString (&buf, gamedir);


	MSG_WriteFloat (&buf, sv.time);

	// send full levelname
	MSG_WriteString (&buf, PR_GetString (&sv_pr_state,
										 SVstring (sv.edicts, message)));

	// send the movevars
	MSG_WriteFloat (&buf, movevars.gravity);
	MSG_WriteFloat (&buf, movevars.stopspeed);
	MSG_WriteFloat (&buf, movevars.maxspeed);
	MSG_WriteFloat (&buf, movevars.spectatormaxspeed);
	MSG_WriteFloat (&buf, movevars.accelerate);
	MSG_WriteFloat (&buf, movevars.airaccelerate);
	MSG_WriteFloat (&buf, movevars.wateraccelerate);
	MSG_WriteFloat (&buf, movevars.friction);
	MSG_WriteFloat (&buf, movevars.waterfriction);
	MSG_WriteFloat (&buf, movevars.entgravity);

	// send music
	MSG_WriteByte (&buf, svc_cdtrack);
	MSG_WriteByte (&buf, 0);			// none in demos

	// send server info string
	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va ("fullserverinfo \"%s\"\n",
							   Info_MakeString (svs.info, 0)));

	// flush packet
	SV_WriteRecordDemoMessage (&buf);
	SZ_Clear (&buf);

	// soundlist
	MSG_WriteByte (&buf, svc_soundlist);
	MSG_WriteByte (&buf, 0);

	n = 0;
	s = sv.sound_precache[n + 1];
	while (s) {
		MSG_WriteString (&buf, s);
		if (buf.cursize > MAX_MSGLEN / 2) {
			MSG_WriteByte (&buf, 0);
			MSG_WriteByte (&buf, n);
			SV_WriteRecordDemoMessage (&buf);
			SZ_Clear (&buf);
			MSG_WriteByte (&buf, svc_soundlist);
			MSG_WriteByte (&buf, n + 1);
		}
		n++;
		s = sv.sound_precache[n + 1];
	}

	if (buf.cursize) {
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, 0);
		SV_WriteRecordDemoMessage (&buf);
		SZ_Clear (&buf);
	}
	// modellist
	MSG_WriteByte (&buf, svc_modellist);
	MSG_WriteByte (&buf, 0);

	n = 0;
	s = sv.model_precache[n + 1];
	while (s) {
		MSG_WriteString (&buf, s);
		if (buf.cursize > MAX_MSGLEN / 2) {
			MSG_WriteByte (&buf, 0);
			MSG_WriteByte (&buf, n);
			SV_WriteRecordDemoMessage (&buf);
			SZ_Clear (&buf);
			MSG_WriteByte (&buf, svc_modellist);
			MSG_WriteByte (&buf, n + 1);
		}
		n++;
		s = sv.model_precache[n + 1];
	}
	if (buf.cursize) {
		MSG_WriteByte (&buf, 0);
		MSG_WriteByte (&buf, 0);
		SV_WriteRecordDemoMessage (&buf);
		SZ_Clear (&buf);
	}
	// prespawn

	for (n = 0; n < sv.num_signon_buffers; n++) {
		SZ_Write (&buf, sv.signon_buffers[n], sv.signon_buffer_size[n]);

		if (buf.cursize > MAX_MSGLEN / 2) {
			SV_WriteRecordDemoMessage (&buf);
			SZ_Clear (&buf);
		}
	}

	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va ("cmd spawn %i 0\n", svs.spawncount));

	if (buf.cursize) {
		SV_WriteRecordDemoMessage (&buf);
		SZ_Clear (&buf);
	}
	// send current status of all other players

	for (i = 0; i < MAX_CLIENTS; i++) {
		player = svs.clients + i;

		MSG_WriteByte (&buf, svc_updatefrags);
		MSG_WriteByte (&buf, i);
		MSG_WriteShort (&buf, player->old_frags);

		MSG_WriteByte (&buf, svc_updateping);
		MSG_WriteByte (&buf, i);
		MSG_WriteShort (&buf, SV_CalcPing (player));

		MSG_WriteByte (&buf, svc_updatepl);
		MSG_WriteByte (&buf, i);
		MSG_WriteByte (&buf, player->lossage);

		MSG_WriteByte (&buf, svc_updateentertime);
		MSG_WriteByte (&buf, i);
		MSG_WriteFloat (&buf, realtime - player->connection_started);

		info = player->userinfo ? Info_MakeString (player->userinfo,
												   make_info_string_filter)
								: "";

		MSG_WriteByte (&buf, svc_updateuserinfo);
		MSG_WriteByte (&buf, i);
		MSG_WriteLong (&buf, player->userid);
		MSG_WriteString (&buf, info);

		if (buf.cursize > MAX_MSGLEN / 2) {
			SV_WriteRecordDemoMessage (&buf);
			SZ_Clear (&buf);
		}
	}

	// send all current light styles
	for (i = 0; i < MAX_LIGHTSTYLES; i++) {
		MSG_WriteByte (&buf, svc_lightstyle);
		MSG_WriteByte (&buf, (char) i);
		MSG_WriteString (&buf, sv.lightstyles[i]);
	}

	// get the client to check and download skins
	// when that is completed, a begin command will be issued
	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, va ("skins\n"));

	SV_WriteRecordDemoMessage (&buf);

	SV_WriteSetDemoMessage ();
	// done
}

/*
	SV_CleanName

	Cleans the demo name, removes restricted chars, makes name lowercase
*/

static char *
SV_CleanName (const unsigned char *name)
{
	static char *text;
	static size_t text_len;
	char       *out, c;

	if (text_len < strlen (name)) {
		text_len = (strlen (name) + 1023) & ~1023;
		text = realloc (text, text_len);
	}

	out = text;
	do {
		c = sys_char_map[*name++];
		if (c != '_')
			*out++ = c;
	} while (c);

	return text;
}

/*
	SV_Record_f

	record <demoname>
*/
static void
SV_Record_f (void)
{
	dstring_t  *name = dstring_newstr ();

	if (Cmd_Argc () != 2) {
		Con_Printf ("record <demoname>\n");
		return;
	}

	if (sv.state != ss_active) {
		Con_Printf ("Not active yet.\n");
		return;
	}

	if (sv.demorecording)
		SV_Stop (0);

	dsprintf (name, "%s/%s/%s%s%s", qfs_gamedir->dir.def, sv_demoDir->string,
			  sv_demoPrefix->string, SV_CleanName (Cmd_Argv (1)),
			  sv_demoSuffix->string);

	// open the demo file
	name->size += 4;
	dstring_adjust (name);
	QFS_DefaultExtension (name->str, ".mvd");

	SV_Record (name->str);

	dstring_delete (name);
}

/*
	SV_EasyRecord_f

	easyrecord [demoname]
*/

static int
Dem_CountPlayers (void)
{
	int         i, count;

	count = 0;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (svs.clients[i].name[0] && !svs.clients[i].spectator)
			count++;
	}

	return count;
}

static const char *
Dem_Team (int num)
{
	int         i;
	static const char *lastteam[2];
	qboolean    first = true;
	client_t   *client;
	static int  index = 0;
	const char *team;

	index = 1 - index;

	for (i = 0, client = svs.clients; num && i < MAX_CLIENTS; i++, client++) {
		if (!client->name[0] || client->spectator)
			continue;

		team = Info_ValueForKey (svs.clients[i].userinfo, "team");
		if (first || strcmp (lastteam[index], team)) {
			first = false;
			num--;
			lastteam[index] = team;
		}
	}

	if (num)
		return "";

	return lastteam[index];
}

static const char *
Dem_PlayerName (int num)
{
	int         i;
	client_t   *client;

	for (i = 0, client = svs.clients; i < MAX_CLIENTS; i++, client++) {
		if (!client->name[0] || client->spectator)
			continue;

		if (!--num)
			return client->name;
	}

	return "";
}

static void
SV_EasyRecord_f (void)
{
	dstring_t  *name = dstring_newstr ();
	dstring_t  *name2 = dstring_newstr ();
	int         i;

	if (Cmd_Argc () > 2) {
		Con_Printf ("easyrecord [demoname]\n");
		return;
	}

	if (sv.demorecording)
		SV_Stop (0);

	if (Cmd_Argc () == 2)
		dsprintf (name, "%s", Cmd_Argv (1));
	else {
		// guess game type and write demo name
		i = Dem_CountPlayers ();
		if (teamplay->int_val && i > 2) {
			// Teamplay
			dsprintf (name, "team_%s_vs_%s_%s",
					  Dem_Team (1), Dem_Team (2), sv.name);
		} else {
			if (i == 2) {
				// Duel
				dsprintf (name, "duel_%s_vs_%s_%s",
						  Dem_PlayerName (1), Dem_PlayerName (2), sv.name);
			} else {
				// FFA
				dsprintf (name, "ffa_%s (%d)", sv.name, i);
			}
		}
	}

	// Make sure the filename doesn't contain illegal characters
	dsprintf (name2, "%s/%s%s%s%s%s", 
			  qfs_gamedir->dir.def, sv_demoDir->string,
			  sv_demoDir->string[0] ? "/" : "",
			  sv_demoPrefix->string, SV_CleanName (name->str),
			  sv_demoSuffix->string);

	if (QFS_NextFilename (name, name2->str, ".mvd"))
		SV_Record (name->str);

	dstring_delete (name);
	dstring_delete (name2);
}

void
Demo_Init (void)
{
	int         p, size = MIN_DEMO_MEMORY;

	p = COM_CheckParm ("-democache");
	if (p) {
		if (p < com_argc - 1)
			size = atoi (com_argv[p + 1]) * 1024;
		else
			Sys_Error ("Memory_Init: you must specify a size in KB after "
					   "-democache");
	}

	if (size < MIN_DEMO_MEMORY) {
		Con_Printf ("Minimum memory size for demo cache is %dk\n",
					MIN_DEMO_MEMORY / 1024);
		size = MIN_DEMO_MEMORY;
	}

	demo.name = dstring_newstr ();
	demo.text = dstring_newstr ();

	svs.demomem = Hunk_AllocName (size, "demo");
	svs.demomemsize = size;
	demo_max_size = size - 0x80000;

	serverdemo = Cvar_Get ("serverdemo", "", CVAR_SERVERINFO, Cvar_Info,
						   "FIXME");
	sv_demofps = Cvar_Get ("sv_demofps", "20", CVAR_NONE, 0, "FIXME");
	sv_demoPings = Cvar_Get ("sv_demoPings", "3", CVAR_NONE, 0, "FIXME");
	sv_demoNoVis = Cvar_Get ("sv_demoNoVis", "1", CVAR_NONE, 0, "FIXME");
	sv_demoUseCache = Cvar_Get ("sv_demoUseCache", "0", CVAR_NONE, 0, "FIXME");
	sv_demoCacheSize = Cvar_Get ("sv_demoCacheSize", va ("%d", size / 1024),
								 CVAR_ROM, 0, "FIXME");
	sv_demoMaxSize = Cvar_Get ("sv_demoMaxSize", "20480", CVAR_NONE, 0,
							   "FIXME");
	sv_demoMaxDirSize = Cvar_Get ("sv_demoMaxDirSize", "102400", CVAR_NONE, 0,
								  "FIXME");
	sv_demoDir = Cvar_Get ("sv_demoDir", "demos", CVAR_NONE, 0, "FIXME");
	sv_demoPrefix = Cvar_Get ("sv_demoPrefix", "", CVAR_NONE, 0, "FIXME");
	sv_demoSuffix = Cvar_Get ("sv_demoSuffix", "", CVAR_NONE, 0, "FIXME");
	sv_onrecordfinish = Cvar_Get ("sv_onrecordfinish", "", CVAR_NONE, 0, "FIXME");
	sv_ondemoremove = Cvar_Get ("sv_ondemoremove", "", CVAR_NONE, 0, "FIXME");
	sv_demotxt = Cvar_Get ("sv_demotxt", "1", CVAR_NONE, 0, "FIXME");

	Cmd_AddCommand ("record", SV_Record_f, "FIXME");
	Cmd_AddCommand ("easyrecord", SV_EasyRecord_f, "FIXME");
	Cmd_AddCommand ("stop", SV_Stop_f, "FIXME");
	Cmd_AddCommand ("cancel", SV_Cancel_f, "FIXME");
}
