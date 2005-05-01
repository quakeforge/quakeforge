/*
	sv_recorder.c

	Interface for recording server state (server side demos and qtv)

	Copyright (C) 2005 #AUTHOR#

	Author: Bill Currie
	Date: 2005/5/1

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

#include "QF/sizebuf.h"

#include "bothdefs.h"
#include "server.h"
#include "sv_demo.h"
#include "sv_progs.h"
#include "sv_recorder.h"

typedef struct dbuffer_s {
	byte       *data;
	int         start, end, last;
	int         maxsize;
} dbuffer_t;

typedef struct header_s {
	byte        type;
	byte        full;
	int         to;
	int         size;
	byte        data[1];
} header_t;

typedef struct demobuf_s {
	sizebuf_t   sz;
	int         bufsize;
	header_t   *h;
} demobuf_t;

typedef struct demo_frame_s {
	double      time;
	demobuf_t   buf;
} demo_frame_t;

#define DEMO_FRAMES 64
#define DEMO_FRAMES_MASK (DEMO_FRAMES - 1)

typedef struct rec_s {
	demobuf_t  *dbuf;
	dbuffer_t   dbuffer;
	sizebuf_t   datagram;

	int         lastto;
	int         lasttype;
	double      time, pingtime;

	delta_t     delta;
	int         stats[MAX_CLIENTS][MAX_CL_STATS];	// ouch!
	demo_frame_t frames[DEMO_FRAMES];
	int         forceFrame;

	int         parsecount;
	int         lastwritten;
} rec_t;

struct recorder_s {
	recorder_t *next;
	void (*writer)(sizebuf_t *);
	int (*frame)(void);
};

static rec_t    rec;
static recorder_t *free_recorders;
static recorder_t recorders_list[3];

static byte     buffer[20 * MAX_MSGLEN];
static byte     datagram_data[MAX_DATAGRAM];
static byte     msg_buffer[MAX_DATAGRAM];

#define MIN_DEMO_MEMORY 0x100000
#define USECACHE (sv_demoUseCache->int_val && svs.demomemsize)
#define MAXSIZE (rec.dbuffer.end < rec.dbuffer.last ? \
				 rec.dbuffer.start - rec.dbuffer.end : \
				 rec.dbuffer.maxsize - rec.dbuffer.end)

#define HEADER  ((int) &((header_t *) 0)->data)

static entity_state_t entities[UPDATE_MASK + 1][MAX_DEMO_PACKET_ENTITIES];
static plent_state_t players[UPDATE_MASK + 1][MAX_CLIENTS];

static void
dbuffer_init (dbuffer_t *dbuffer, byte *buf, size_t size)
{
	dbuffer->data = buf;
	dbuffer->maxsize = size;
	dbuffer->start = 0;
	dbuffer->end = 0;
	dbuffer->last = 0;
}

static void
set_msgbuf (demobuf_t *prev, demobuf_t *cur)
{
	// fix the maxsize of previous msg buffer,
	// we won't be able to write there anymore
	if (prev != NULL)
		prev->sz.maxsize = prev->bufsize;

	rec.dbuf = cur;
	memset (rec.dbuf, 0, sizeof (*rec.dbuf));

	rec.dbuf->sz.data = rec.dbuffer.data + rec.dbuffer.end;
	rec.dbuf->sz.maxsize = MAXSIZE;
}

static void
write_msg (sizebuf_t *msg, int type, int to, float time, sizebuf_t *dst)
{
	int         msec;
	static double prevtime;

	msec = (time - prevtime) * 1000;
	prevtime += msec * 0.001;
	if (msec > 255)
		msec = 255;
	if (msec < 2)
		msec = 0;

	MSG_WriteByte (dst, msec);

	if (rec.lasttype != type || rec.lastto != to) {
		rec.lasttype = type;
		rec.lastto = to;
		switch (rec.lasttype) {
			case dem_all:
				MSG_WriteByte (dst, dem_all);
				break;
			case dem_multiple:
				MSG_WriteByte (dst, dem_multiple);
				MSG_WriteLong (dst, rec.lastto);
				break;
			case dem_single:
			case dem_stats:
				MSG_WriteByte (dst, rec.lasttype | (rec.lastto << 3));
				break;
			default:
				//XXX SV_Stop (0);
				Con_Printf ("bad demo message type:%d", type);
				return;
		}
	} else {
		MSG_WriteByte (dst, dem_read);
	}

	MSG_WriteLong (dst, msg->cursize);
	SZ_Write (dst, msg->data, msg->cursize);
}

static void
write_to_msg (int type, int to, float time, sizebuf_t *dst)
{
	int         pos = 0, oldm, oldd;
	header_t   *p;
	int         size;
	sizebuf_t   msg;

	p = (header_t *) rec.dbuf->sz.data;
	rec.dbuf->h = NULL;

	oldm = rec.dbuf->bufsize;
	oldd = rec.dbuffer.start;
	while (pos < rec.dbuf->bufsize) {
		size = p->size;
		pos += HEADER + size;

		// no type means we are writing to disk everything
		if (!type || (p->type == type && p->to == to)) {
			if (size) {
				msg.data = p->data;
				msg.cursize = size;

				write_msg (&msg, p->type, p->to, time, dst);
			}
			// data is written so it need to be cleard from demobuf
			if (rec.dbuf->sz.data != (byte *) p)
				memmove (rec.dbuf->sz.data + size + HEADER,
						 rec.dbuf->sz.data, (byte *) p - rec.dbuf->sz.data);

			rec.dbuf->bufsize -= size + HEADER;
			rec.dbuf->sz.data += size + HEADER;
			pos -= size + HEADER;
			rec.dbuf->sz.maxsize -= size + HEADER;
			rec.dbuffer.start += size + HEADER;
		}
		// move along
		p = (header_t *) (p->data + size);
	}

	if (rec.dbuffer.start == rec.dbuffer.last) {
		if (rec.dbuffer.start == rec.dbuffer.end) {
			rec.dbuffer.end = 0;		// rec.dbuffer is empty
			rec.dbuf->sz.data = rec.dbuffer.data;
		}
		// go back to begining of the buffer
		rec.dbuffer.last = rec.dbuffer.end;
		rec.dbuffer.start = 0;
	}
}

/*
	set_buf

	Sets position in the buf for writing to specific client
*/

static void
set_buf (byte type, int to)
{
	header_t   *p;
	int         pos = 0;

	p = (header_t *) rec.dbuf->sz.data;

	while (pos < rec.dbuf->bufsize) {
		pos += HEADER + p->size;

		if (type == p->type && to == p->to && !p->full) {
			rec.dbuf->sz.cursize = pos;
			rec.dbuf->h = p;
			return;
		}

		p = (header_t *) (p->data + p->size);
	}
	// type && to not exist in the buf, so add it

	p->type = type;
	p->to = to;
	p->size = 0;
	p->full = 0;

	rec.dbuf->bufsize += HEADER;
	rec.dbuf->sz.cursize = rec.dbuf->bufsize;
	rec.dbuffer.end += HEADER;
	rec.dbuf->h = p;
}

static void
move_buf (void)
{
	// set the last message mark to the previous frame (i/e begining of this
	// one)
	rec.dbuffer.last = rec.dbuffer.end - rec.dbuf->bufsize;

	// move buffer to the begining of demo buffer
	memmove (rec.dbuffer.data, rec.dbuf->sz.data, rec.dbuf->bufsize);
	rec.dbuf->sz.data = rec.dbuffer.data;
	rec.dbuffer.end = rec.dbuf->bufsize;
	rec.dbuf->h = NULL;				// it will be setup again
	rec.dbuf->sz.maxsize = MAXSIZE + rec.dbuf->bufsize;
}

static void
clear_rec (void)
{
	int         i;

	memset (&rec, 0, sizeof (rec));

	rec.delta.pvs = dt_pvs_fat;
	for (i = 0; i < UPDATE_BACKUP; i++) {
		rec.delta.frames[i].entities.entities = entities[i];
		rec.delta.frames[i].players.players = players[i];
	}
	dbuffer_init (&rec.dbuffer, buffer, sizeof (buffer));
	set_msgbuf (NULL, &rec.frames[0].buf);

	rec.datagram.maxsize = sizeof (datagram_data);
	rec.datagram.data = datagram_data;
}

void
SVR_Init (void)
{
	recorder_t *r;
	unsigned    i;

	for (i = 0, r = recorders_list;
		 i < (sizeof (recorders_list) / sizeof (recorders_list[0])) - 1;
		 i++, r++)
		r->next = r + 1;
	r->next = 0;
	free_recorders = recorders_list;
}

recorder_t *
SVR_AddUser (void (*writer)(sizebuf_t *), int (*frame)(void))
{
	recorder_t *r;

	if (!free_recorders)
		return 0;

	if (!sv.recorders) {
		clear_rec ();
		rec.pingtime = sv.time;
	}

	r = free_recorders;
	free_recorders = r->next;

	r->next = sv.recorders;
	sv.recorders = r;

	r->writer = writer;
	r->frame = frame;

	return r;
}

void
SVR_RemoveUser (recorder_t *r)
{
	recorder_t **_r;

	for (_r = &sv.recorders; *_r; _r = &(*_r)->next) {
		if (*_r == r) {
			*_r = (*_r)->next;
			r->next = free_recorders;
			free_recorders = r;
		}
	}
}

static void
write_datagram (void)
{
	sizebuf_t  *dbuf;
	sizebuf_t   msg;

	memset (&msg, 0, sizeof (msg));
	msg.data = msg_buffer;
	msg.maxsize = sizeof (msg_buffer);
	msg.allowoverflow = true;

	if (!rec.delta.delta_sequence)
		rec.delta.delta_sequence = -1;
	rec.delta.cur_frame = (rec.delta.delta_sequence + 1) & UPDATE_MASK;
	rec.delta.out_frame = rec.delta.cur_frame;
	SV_WriteEntitiesToClient (&rec.delta, &msg);
	dbuf = SVR_WriteBegin (dem_all, 0, msg.cursize);
	SZ_Write (dbuf, msg.data, msg.cursize);
	// copy the accumulated multicast datagram
	// for this client out to the message
	if (rec.datagram.cursize) {
		dbuf = SVR_WriteBegin (dem_all, 0, rec.datagram.cursize);
		SZ_Write (dbuf, rec.datagram.data, rec.datagram.cursize);
		SZ_Clear (&rec.datagram);
	}

	rec.delta.delta_sequence++;
	rec.delta.delta_sequence &= UPDATE_MASK;
	rec.frames[rec.parsecount & DEMO_FRAMES_MASK].time = rec.time = sv.time;
}

void
SVR_WritePacket (void)
{
	demo_frame_t *frame;
	double      time;
	sizebuf_t   msg;
	recorder_t *r;

	write_datagram ();

	memset (&msg, 0, sizeof (msg));
	msg.data = msg_buffer;
	msg.maxsize = sizeof (msg_buffer);
	msg.allowoverflow = true;

	frame = &rec.frames[rec.lastwritten & DEMO_FRAMES_MASK];
	time = frame->time;

	rec.dbuf = &frame->buf;

	write_to_msg (0, 0, time, &msg);

	for (r = sv.recorders; r; r = r->next)
		r->writer (&msg);

	rec.dbuf = &rec.frames[rec.parsecount & DEMO_FRAMES_MASK].buf;
	rec.dbuf->sz.maxsize = MAXSIZE + rec.dbuf->bufsize;

	rec.parsecount++;
	set_msgbuf (rec.dbuf, &rec.frames[rec.parsecount & DEMO_FRAMES_MASK].buf);
}

sizebuf_t *
SVR_WriteBegin (byte type, int to, int size)
{
	byte       *p;
	qboolean    move = false;

	// will it fit?
	while (rec.dbuf->bufsize + size + HEADER > rec.dbuf->sz.maxsize) {
		// if we reached the end of buffer move msgbuf to the begining
		if (!move && rec.dbuffer.end > rec.dbuffer.start)
			move = true;

		SVR_WritePacket ();
		if (move && rec.dbuffer.start > rec.dbuf->bufsize + HEADER + size)
			move_buf ();
	}

	if (rec.dbuf->h == NULL || rec.dbuf->h->type != type
		|| rec.dbuf->h->to != to || rec.dbuf->h->full) {
		set_buf (type, to);
	}

	if (rec.dbuf->h->size + size > MAX_MSGLEN) {
		rec.dbuf->h->full = 1;
		set_buf (type, to);
	}
	// we have to make room for new data
	if (rec.dbuf->sz.cursize != rec.dbuf->bufsize) {
		p = rec.dbuf->sz.data + rec.dbuf->sz.cursize;
		memmove (p + size, p, rec.dbuf->bufsize - rec.dbuf->sz.cursize);
	}

	rec.dbuf->bufsize += size;
	rec.dbuf->h->size += size;
	if ((rec.dbuffer.end += size) > rec.dbuffer.last)
		rec.dbuffer.last = rec.dbuffer.end;
	return &rec.dbuf->sz;
}

sizebuf_t *
SVR_Datagram (void)
{
	return &rec.datagram;
}

void
SVR_ForceFrame (void)
{
	rec.forceFrame = 1;
}

int
SVR_Frame (void)
{
	recorder_t *r;

	if (rec.forceFrame) {
		rec.forceFrame = 0;
		return 1;
	}
	for (r = sv.recorders; r; r = r->next)
		if (r->frame ())
			return 1;
	return 0;
}

static void
demo_pings (void)
{
	client_t   *client;
	int         j;
	sizebuf_t  *dbuf;

	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++) {
		if (client->state != cs_spawned && client->state != cs_server)
			continue;

		dbuf = SVR_WriteBegin (dem_all, 0, 7);
		MSG_WriteByte (dbuf, svc_updateping);
		MSG_WriteByte (dbuf, j);
		MSG_WriteShort (dbuf, SV_CalcPing (client));
		MSG_WriteByte (dbuf, svc_updatepl);
		MSG_WriteByte (dbuf, j);
		MSG_WriteByte (dbuf, client->lossage);
	}
}

void
SV_SendDemoMessage (void)
{
	int         i, j;
	client_t   *c;
	byte        buf[MAX_DATAGRAM];
	sizebuf_t	msg, *dbuf;
	int         stats[MAX_CL_STATS];

	if (sv_demoPings->value && sv.time - rec.pingtime > sv_demoPings->value) {
		demo_pings ();
		rec.pingtime = sv.time;
	}

	if (!SVR_Frame ())
		return;

	for (i = 0, c = svs.clients; i < MAX_CLIENTS; i++, c++) {
		if (c->state != cs_spawned && c->state != cs_server)
			continue;

		if (c->spectator)
			continue;

		SV_GetStats (c->edict, 0, stats);

		for (j = 0 ; j < MAX_CL_STATS ; j++)
			if (stats[j] != rec.stats[i][j]) {
				rec.stats[i][j] = stats[j];
				if (stats[j] >=0 && stats[j] <= 255) {
					dbuf = SVR_WriteBegin (dem_stats, i, 3);
					MSG_WriteByte (dbuf, svc_updatestat);
					MSG_WriteByte (dbuf, j);
					MSG_WriteByte (dbuf, stats[j]);
				} else {
					dbuf = SVR_WriteBegin (dem_stats, i, 6);
					MSG_WriteByte (dbuf, svc_updatestatlong);
					MSG_WriteByte (dbuf, j);
					MSG_WriteLong (dbuf, stats[j]);
				}
			}
	}

	// send over all the objects that are in the PVS
	// this will include clients, a packetentities, and
	// possibly a nails update
	msg.data = buf;
	msg.maxsize = sizeof (buf);
	msg.cursize = 0;
	msg.allowoverflow = true;
	msg.overflowed = false;

	SVR_WritePacket ();
}
