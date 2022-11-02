/*
	client.h

	Client definitions

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

#ifndef _CLIENT_H
#define _CLIENT_H

#include "QF/info.h"
#include "QF/quakefs.h"
#include "QF/vid.h"
#include "QF/zone.h"

#include "QF/plugin/vid_render.h"
#include "QF/scene/entity.h"

#include "client/chase.h"
#include "client/entities.h"
#include "client/input.h"
#include "client/state.h"
#include "client/view.h"

#include "netchan.h"
#include "qw/bothdefs.h"
#include "qw/protocol.h"


/*
  player_state_t is the information needed by a player entity
  to do move prediction and to generate a drawable entity
*/
typedef struct player_state_s {
	int         messagenum;		// all player's won't be updated each frame

	double      state_time;		// not the same as the packet time,
								// because player commands come asyncronously
	vec3_t      viewangles;		// only for demos, not from server

	plent_state_t pls;

	float       waterjumptime;
	int         onground;		// -1 = in air, else pmove entity number
	int         oldbuttons;
	int         oldonground;

} player_state_t;

typedef struct {
	// generated on client side
	usercmd_t   cmd;		// cmd that generated the frame
	double      senttime;	// time cmd was sent off
	int         delta_sequence;		// sequence number to delta from, -1 = full update

	// received from server
	double      receivedtime;	// time message was received, or -1
	player_state_t playerstate[MAX_CLIENTS];	// message received that
												// reflects performing the
												// usercmd
	packet_entities_t packet_entities;
	qboolean    invalid;		// true if the packet_entities delta was invalid
} frame_t;

#define	MAX_DEMOS		8
#define	MAX_DEMONAME	16

typedef enum {
	ca_disconnected, 	// full screen console with no connection
	ca_demostart,		// starting up a demo
	ca_connected,		// talking to a server
	ca_onserver,		// processing data lists, donwloading, etc
	ca_active,			// everything is in, so frames can be rendered
} cactive_t;

typedef enum {
	dl_none,
	dl_model,
	dl_sound,
	dl_skin,
	dl_single
} dltype_t;		// download type

/*
  the client_static_t structure is persistant through an arbitrary number
  of server connections
*/
typedef struct {
// connection information
	cactive_t   state;

// network stuff
	netchan_t   netchan;
	int         qport;
	int         challenge;
	float       latency;				// rolling average
	struct dstring_s *servername;		// name of server from original connect
	netadr_t    server_addr;			// address of server

// private userinfo for sending to masterless servers
	struct info_s	*userinfo;
	int         chat;

// file transfer from server
	QFile      *download;
	struct dstring_s *downloadtempname;
	struct dstring_s *downloadname;
	struct dstring_s *downloadurl;
	int         downloadnumber;
	dltype_t    downloadtype;
	int         downloadpercent;

// demo loop control
	int         demonum;		// -1 = don't play demos
	char        demos[MAX_DEMOS][MAX_DEMONAME];		// when not playing

	QFile      *demofile;
	qboolean    demorecording;
	int         demo_capture;
	qboolean    demoplayback;
	qboolean    demoplayback2;
	qboolean    findtrack;
	int         lastto;
	int         lasttype;
	int         prevtime;
	double      basetime;
	qboolean    timedemo;
	double      td_lastframe;		// to meter out one message a frame
	int         td_startframe;		// host_framecount at start
	double      td_starttime;		// realtime at second frame of timedemo
} client_static_t;

extern client_static_t	cls;

#define FPD_NO_MACROS		0x0001	// Many clients ignore this, and it isn't used, but let's honor it
#define FPD_NO_TIMERS		0x0002	// We never allow timers anyway
#define FPD_NO_STRIGGER		0x0004	// Don't have soundtrigger yet, but this disables it
#define FPD_HIDE_PERCENTE	0x0020	// Ditto
#define FPD_HIDE_POINT		0x0080	// Can ignore if we do visibility checking for point
#define FPD_NO_TEAMSKIN		0x0100	// Disable skin force
#define FPD_NO_TEAMCOLOR	0x0200	// Disable color force
#define FPD_HIDE_ITEM		0x0400	// No idea what this does
#define FPD_LIMIT_PITCH		0x4000	// Limit pitchspeed
#define FPD_LIMIT_YAW		0x8000	// Limit yawspeed

#define FPD_DEFAULT			(FPD_HIDE_PERCENTE | FPD_NO_TEAMSKIN)

// These limits prevent a usable RJ script, requiring > 0.1 sec of turning time.
#define FPD_MAXPITCH		1000
#define FPD_MAXYAW			2000

// Default fbskins value. This should really be different for different gamedirs, but eh
#define FBSKINS_DEFAULT		0.0

/*
  the client_state_t structure is wiped completely at every server signon
*/
typedef struct client_state_s {
	qboolean    loading;

	int         movemessages;	// Since connecting to this server throw out
								// the first couple, so the player doesn't
								// accidentally do something the first frame
	// sentcmds[cl.netchan.outgoing_sequence & UPDATE_MASK] = cmd
	frame_t     frames[UPDATE_BACKUP];
	int         link_sequence;
	int         prev_sequence;

// information for local display
	int         stats[MAX_CL_STATS];	// Health, etc
	float       item_gettime[32];	// cl.time of aquiring item, for blinking
	float       faceanimtime;			// Use anim frame if cl.time < this

// the client simulates or interpolates movement to get these values
	double      time;			// this is the time value that the client
								// is rendering at.  always <= realtime
// the client maintains its own idea of view angles, which are sent to the
// server each frame.  And reset only at level change and teleport times
	viewstate_t viewstate;
	movestate_t movestate;
	chasestate_t chasestate;

	qboolean    paused;			// Sent over by server
	float       crouch;			// Local amount for smoothing stepups
	qboolean    inwater;

	int         intermission;	// Don't change view angle, full screen, etc
	int         completed_time;	// Latched from time at intermission start

	int         servercount;	// server identification for prespawns
	struct info_s *serverinfo;
	int         parsecount;		// server message counter
	int         validsequence;	// this is the sequence number of the last good
								// packetentity_t we got.  If this is 0, we
								// can't render a frame yet

	double      last_ping_request;	// while showing scoreboard
	double      last_servermessage;	// (realtime) for net trouble icon

/* information that is static for the entire time connected to a server */

	char        model_name[MAX_MODELS][MAX_QPATH];
	char        sound_name[MAX_SOUNDS][MAX_QPATH];

	struct sfx_s *sound_precache[MAX_SOUNDS];
	int         nummodels;
	int         numsounds;

	char        levelname[40];	// for display on solo scoreboard
	int         spectator;
	int         playernum;
	int         viewentity;		// cl_entitites[cl.viewentity] = player
	unsigned    protocol;
	float       stdver;
	int         gametype;
	int         maxclients;
	// serverinfo mirrors
	int         sv_cshifts;
	int         no_pogo_stick;
	int         teamplay;
	int         watervis;
	int         fpd;
	int         fbskins;

// refresh related state
	int         num_entities;	// held in cl_entities array

	int         cdtrack;		// cd audio

// all player information
	player_info_t *players;

	lightstyle_t lightstyle[MAX_LIGHTSTYLES];
} client_state_t;


/*
  cvars
*/
extern int cl_netgraph;
extern int cl_netgraph_height;
extern float cl_netgraph_alpha;
extern int cl_netgraph_box;

extern int cl_draw_locs;
extern int cl_shownet;

extern char *cl_name;

extern int cl_model_crcs;

extern float rate;

extern int hud_ping;
extern int hud_pl;

extern char *skin;

extern float cl_fb_players;

extern int hud_scoreboard_uid;

extern	client_state_t	cl;

extern entity_t cl_entities[512];
extern byte cl_entity_valid[2][512];

extern	qboolean	nomaster;
extern char	*server_version;	// version of server we connected to

extern	double		realtime;
extern	int			fps_count;

extern struct cbuf_s *cl_cbuf;
extern struct cbuf_s *cl_stbuf;

struct cvar_s;
void Cvar_Info (void *data, const struct cvar_s *cvar);

void CL_NetGraph_Init (void);
void CL_NetGraph_Init_Cvars (void);

extern struct view_s cl_screen_view;
void CL_Init_Screen (void);
void CL_UpdateScreen (double realtime);

void CL_SetState (cactive_t state);

void CL_Cmd_ForwardToServer (void);
void CL_Cmd_Init (void);

void CL_RSShot_f (void);

#define RSSHOT_WIDTH 320
#define RSSHOT_HEIGHT 200

extern struct dstring_s *centerprint;

#endif // _CLIENT_H
