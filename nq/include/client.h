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

#ifndef __client_h
#define __client_h

#include <stdio.h>

#include "QF/mathlib.h"
#include "QF/model.h"
#include "QF/quakefs.h"
#include "QF/sound.h"
#include "QF/render.h"

#include "client/chase.h"
#include "client/entities.h"
#include "client/input.h"
#include "client/state.h"
#include "client/view.h"

#include "game.h"
#include "netmain.h"
#include "protocol.h"


typedef struct usercmd_s {
	vec3_t	viewangles;

// intended velocities
	float	forwardmove;
	float	sidemove;
	float	upmove;
} usercmd_t;

// client_state_t should hold all pieces of the client state

typedef enum {
	so_none,
	so_prespawn,
	so_spawn,
	so_begin,
	so_active,
} signon_t;

#define	MAX_DEMOS		8
#define	MAX_DEMONAME	16

typedef enum {
	ca_dedicated, 		// a dedicated server with no ability to start a client
	ca_disconnected, 	// full screen console with no connection
	ca_connected,		// talking to a server
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
	signon_t    signon;

// network stuff
	struct qsocket_s *netcon;
	sizebuf_t   message;		// writing buffer to send to server

// demo loop control
	int         demonum;		// -1 = don't play demos
	char        demos[MAX_DEMOS][MAX_DEMONAME];		// when not playing

	QFile      *demofile;
	qboolean    demorecording;
	qboolean    demo_capture;
	qboolean    demoplayback;
	int         forcetrack;			// -1 = use normal cd track
	qboolean    timedemo;
	int         td_lastframe;		// to meter out one message a frame
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


/*
  the client_state_t structure is wiped completely at every server signon
*/
typedef struct client_state_s {
	qboolean    loading;

	int         movemessages;	// Since connecting to this server throw out
								// the first couple, so the player doesn't
								// accidentally do something the first frame
	usercmd_t   cmd;			// Last command sent to the server

// information for local display
	int         stats[MAX_CL_STATS];	// Health, etc
	float       item_gettime[32];	// cl.time of aquiring item, for blinking
	float       faceanimtime;			// Use anim frame if cl.time < this

// The client maintains its own idea of view angles, which are sent to the
// server each frame.  The server sets punchangle when the view is temporarily
// offset, and an angle reset commands at the start of each level and after
// teleporting.
	int         frameIndex;
	vec3_t      frameViewAngles[2];	// During demo playback viewangles is lerped
								// between these
	vec4f_t     frameVelocity[2];	// Update by server, used for lean+bob
								// (0 is newest)
	viewstate_t viewstate;
	movestate_t movestate;
	chasestate_t chasestate;

	qboolean    paused;			// Sent over by server
	float       crouch;			// Local amount for smoothing stepups
	qboolean    inwater;

	int         intermission;	// Don't change view angle, full screen, etc
	int         completed_time;	// Latched from time at intermission start

	double      mtime[2];		// The timestamp of last two messages
	double      time;			// Clients view of time, should be between
								// servertime and oldvertime to generate a
								// lerp point for other data
	double      oldtime;		// Previous cl.time, time-oldtime is used
								// to decay light values and smooth step ups

	double      last_ping_request;	// while showing scoreboard
	double      last_servermessage;	// (realtime) for net trouble icon

/* information that is static for the entire time connected to a server */

	struct sfx_s *sound_precache[MAX_SOUNDS];
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

// cvars
extern struct cvar_s	*cl_name;
extern struct cvar_s	*cl_color;

extern struct cvar_s	*cl_autofire;

extern struct cvar_s	*cl_shownet;
extern struct cvar_s	*cl_nolerp;

extern struct cvar_s	*cl_pitchdriftspeed;

extern struct cvar_s	*cl_name;
extern struct cvar_s	*cl_writecfg;

extern struct cvar_s	*cl_cshift_bonus;
extern struct cvar_s	*cl_cshift_contents;
extern struct cvar_s	*cl_cshift_damage;
extern struct cvar_s	*cl_cshift_powerup;

extern struct cvar_s	*noskins;

extern	client_state_t	cl;

extern entity_t *cl_entities[MAX_EDICTS];
extern double cl_msgtime[MAX_EDICTS];
extern struct set_s cl_forcelink;

extern int fps_count;

extern void (*write_angles) (sizebuf_t *sb, const vec3_t angles);

// cl_main
struct cbuf_s;
void CL_Init (struct cbuf_s *cbuf);
void CL_InitCvars (void);
void CL_ClearMemory (void);
int CL_ReadConfiguration (const char *cfg_name);

void CL_EstablishConnection (const char *host);
void CL_Signon1 (void);
void CL_Signon2 (void);
void CL_Signon3 (void);
void CL_Signon4 (void);

void CL_Disconnect (void);
void CL_Disconnect_f (void);
void CL_NextDemo (void);


// cl_input
void CL_Init_Input (struct cbuf_s *cbuf);
void CL_Init_Input_Cvars (void);
void CL_SendCmd (void);
void CL_SendMove (usercmd_t *cmd);

void CL_ClearState (void);

int  CL_ReadFromServer (void);
void CL_WriteToServer (usercmd_t *cmd);
void CL_BaseMove (usercmd_t *cmd);

// cl_demo.c
void CL_StopPlayback (void);
void CL_StopRecording (void);
void CL_Record (const char *argv1, int track);
int CL_GetMessage (void);
void CL_Demo_Init (void);

extern struct cvar_s *demo_gzip;
extern struct cvar_s *demo_speed;

// cl_parse.c
struct skin_s;
void CL_ParseServerMessage (void);
void CL_NewTranslation (int slot, struct skin_s *skin);


// cl_tent
void CL_SignonReply (void);
void CL_RelinkEntities (void);
void CL_ClearEnts (void);
entity_t *CL_GetEntity (int num);

extern	double			realtime;

extern qboolean recording;

void Cvar_Info (struct cvar_s *var);

void CL_UpdateScreen (double realtime);

void CL_SetState (cactive_t state);

void CL_Cmd_ForwardToServer (void);

#endif // __client_h
