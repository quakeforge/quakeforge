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

#ifndef __sv_demo_h
#define __sv_demo_h

#include "QF/quakeio.h"
#include "QF/sizebuf.h"

#include "server.h"

typedef struct {
	byte       *data;
	int         start, end, last;
	int         maxsize;
} dbuffer_t;

typedef struct {
	byte        type;
	byte        full;
	int         to;
	int         size;
	byte        data[1];				// gcc doesn't allow [] (?)
} header_t;

typedef struct {
	vec3_t      origin;
	vec3_t      angles;
	int         weaponframe;
	int         skinnum;
	int         model;
	int         effects;
} demoinfo_t;

typedef struct {
	demoinfo_t  info;
	float       sec;
	int         parsecount;
	qboolean    fixangle;
	vec3_t      angle;
	float       cmdtime;
	int         flags;
	int         frame;
} demo_client_t;

typedef struct {
	sizebuf_t   sz;
	int         bufsize;
	header_t   *h;
} demobuf_t;

typedef struct {
	demo_client_t clients[MAX_CLIENTS];
	double      time;
	demobuf_t   buf;
} demo_frame_t;

#define DEMO_FRAMES 64
#define DEMO_FRAMES_MASK (DEMO_FRAMES - 1)

typedef struct {
	QFile      *file;

	demobuf_t  *dbuf;
	dbuffer_t   dbuffer;
	sizebuf_t   datagram;
	byte        datagram_data[MAX_DATAGRAM];
	int         lastto;
	int         lasttype;
	double      time, pingtime;
	int         stats[MAX_CLIENTS][MAX_CL_STATS];	// ouch!
	client_t    recorder;
	qboolean    fixangle[MAX_CLIENTS];
	float       fixangletime[MAX_CLIENTS];
	vec3_t      angles[MAX_CLIENTS];
	struct dstring_s *name;
	struct dstring_s *text;
	int         parsecount;
	int         lastwritten;
	demo_frame_t frames[DEMO_FRAMES];
	demoinfo_t  info[MAX_CLIENTS];
	int         size;
	qboolean    disk;
	void       *dest;
	byte       *mfile;
	byte        buffer[20 * MAX_MSGLEN];
	int         bufsize;
	int         forceFrame;
} demo_t;

extern demo_t      demo;
extern struct cvar_s *sv_demoUseCache;
extern struct cvar_s *sv_demoCacheSize;
extern struct cvar_s *sv_demoMaxDirSize;
extern struct cvar_s *sv_demoDir;
extern struct cvar_s *sv_demofps;
extern struct cvar_s *sv_demoPings;
extern struct cvar_s *sv_demoNoVis;
extern struct cvar_s *sv_demoMaxSize;
extern struct cvar_s *sv_demoPrefix;
extern struct cvar_s *sv_demoSuffix;
extern struct cvar_s *sv_onrecordfinish;
extern struct cvar_s *sv_ondemoremove;
extern struct cvar_s *sv_demotxt;
extern struct cvar_s *serverdemo;

void DemoWrite_Begin (byte type, int to, int size);
void SV_DemoWritePackets (int num);
void SV_Stop (int reason);
void DemoSetMsgBuf (demobuf_t *prev, demobuf_t *cur);
void Demo_Init (void);
void SV_DemoPings (void);
void SV_WriteDemoMessage (sizebuf_t *msg, int type, int to, float time);

#endif//__sv_demo_h
