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

	$Id$
*/

#ifndef __client_h
#define __client_h

#include <stdio.h>

#include "QF/input.h"
#include "QF/mathlib.h"
#include "QF/model.h"
#include "QF/quakefs.h"
#include "QF/sound.h"
#include "QF/render.h"

#include "game.h"
#include "netmain.h"
#include "protocol.h"
#include "r_local.h"


typedef struct usercmd_s
{
	vec3_t	viewangles;

// intended velocities
	float	forwardmove;
	float	sidemove;
	float	upmove;
} usercmd_t;

typedef struct
{
	char	name[MAX_SCOREBOARDNAME];
	float	entertime;
	int		frags;
	int		colors;			// two 4 bit fields
	int		_colors;
	byte	translations[4*VID_GRADES*256];	// space for colormap32
} scoreboard_t;

#define	NAME_LENGTH	64


// client_state_t should hold all pieces of the client state

#define	SIGNONS		4			// signon messages to receive before connected

#define	MAX_DEMOS		8
#define	MAX_DEMONAME	16
#define	MAX_MAPSTRING	2048

typedef enum {
	ca_dedicated, 		// a dedicated server with no ability to start a client
	ca_disconnected, 	// full screen console with no connection
	ca_connected		// valid netcon, talking to a server
} cactive_t;

typedef enum {
	dl_none,
	dl_model,
	dl_sound,
	dl_skin,
	dl_single
} dltype_t;		// download type

// FIXME: A grotesque (temporary) hack.  They're not the same thing to QW.
#define ca_active ca_connected

/*
  the client_static_t structure is persistant through an arbitrary number
  of server connections
*/
typedef struct
{
	cactive_t	state;

// personalization data sent to server	
	char		mapstring[MAX_QPATH];
	char		spawnparms[MAX_MAPSTRING];	// to restart a level

// file transfer from server
	QFile	   *download;
	char	   *downloadtempname;
	char	   *downloadname;
	int			downloadnumber;
	dltype_t	downloadtype;
	int			downloadpercent;

// demo loop control
	int			demonum;		// -1 = don't play demos
	char		demos[MAX_DEMOS][MAX_DEMONAME];		// when not playing

// demo recording info must be here, because record is started before
// entering a map (and clearing client_state_t)
	qboolean	demorecording;
	qboolean	demoplayback;
	qboolean	timedemo;
	int			forcetrack;			// -1 = use normal cd track
	QFile		*demofile;
	int			td_lastframe;		// to meter out one message a frame
	int			td_startframe;		// host_framecount at start
	float		td_starttime;		// realtime at second frame of timedemo

// connection information
	int			signon;			// 0 to SIGNONS
	struct qsocket_s	*netcon;
	sizebuf_t	message;		// writing buffer to send to server
	
} client_static_t;

extern client_static_t	cls;

/*
  the client_state_t structure is wiped completely at every
  server signon
*/
typedef struct
{
	int			movemessages;	// Since connecting to this server throw out
								// the first couple, so the player doesn't
								// accidentally do something the first frame
	usercmd_t	cmd;			// Last command sent to the server

// information for local display
	int			stats[MAX_CL_STATS];	// Health, etc
	float		item_gettime[32];	// cl.time of aquiring item, for blinking
	float		faceanimtime;	// Use anim frame if cl.time < this

	cshift_t	cshifts[NUM_CSHIFTS];	// Color shifts for damage, powerups
	cshift_t	prev_cshifts[NUM_CSHIFTS];	// And content types

// The client maintains its own idea of view angles, which are sent to the
// server each frame.  The server sets punchangle when the view is temporarily
// offset, and an angle reset commands at the start of each level and after
// teleporting.
	vec3_t		mviewangles[2];	// During demo playback viewangles is lerped
								// between these
	vec3_t		viewangles;
	
	vec3_t		mvelocity[2];	// Update by server, used for lean+bob
								// (0 is newest)
	vec3_t		velocity;		// Lerped between mvelocity[0] and [1]

	vec3_t		punchangle;		// Temporary offset
	
// pitch drifting vars
	float		idealpitch;
	float		pitchvel;
	qboolean	nodrift;
	float		driftmove;
	double		laststop;

	float		viewheight;
	float		crouch;			// Local amount for smoothing stepups

	qboolean	paused;			// Send over by server
	qboolean	onground;
	qboolean	inwater;
	
	int			intermission;	// Don't change view angle, full screen, etc
	int			completed_time;	// Latched at intermission start
	
	double		mtime[2];		// The timestamp of last two messages	
	double		time;			// Clients view of time, should be between
								// servertime and oldservertime to generate a
								// lerp point for other data
	double		oldtime;		// Previous cl.time, time-oldtime is used
								// to decay light values and smooth step ups

	float		last_received_message;	// (realtime) for net trouble icon

/* information that is static for the entire time connected to a server */

	struct model_s		*model_precache[MAX_MODELS];
	struct sfx_s		*sound_precache[MAX_SOUNDS];

	struct plitem_s *edicts;
	struct plitem_s *worldspawn;

	char		levelname[40];	// for display on solo scoreboard
	int			viewentity;		// cl_entitites[cl.viewentity] = player
	int			maxclients;
	int			gametype;

// refresh related state
	struct model_s	*worldmodel;	// cl_entitites[0].model
	int			num_entities;	// held in cl_entities array
	entity_t	viewent;			// the gun model

	int			cdtrack, looptrack;	// cd audio

// frag scoreboard
	scoreboard_t	*scores;		// [cl.maxclients]

	unsigned	protocol;
} client_state_t;


typedef struct cl_entity_state_s {
	entity_t		*ent;
	entity_state_t	baseline;
	int				forcelink;
	vec3_t			msg_origins[2];
	vec3_t			msg_angles[2];
	double			msgtime;
	int				effects;
	int				colors;
	struct model_s	*model;
	int				skinnum;
	byte			alpha;
	byte			scale;
	byte			colormod;
	byte			glow_size;
	byte			glow_color;
} cl_entity_state_t;


// cvars
extern struct cvar_s	*cl_name;
extern struct cvar_s	*cl_color;

extern struct cvar_s	*cl_upspeed;
extern struct cvar_s	*cl_forwardspeed;
extern struct cvar_s	*cl_backspeed;
extern struct cvar_s	*cl_sidespeed;

extern struct cvar_s	*cl_movespeedkey;

extern struct cvar_s	*cl_yawspeed;
extern struct cvar_s	*cl_pitchspeed;

extern struct cvar_s	*cl_anglespeedkey;

extern struct cvar_s	*cl_autofire;

extern struct cvar_s	*cl_shownet;
extern struct cvar_s	*cl_nolerp;

extern struct cvar_s	*hud_sbar;

extern struct cvar_s	*cl_pitchdriftspeed;
extern struct cvar_s	*lookspring;

extern struct cvar_s	*m_pitch;
extern struct cvar_s	*m_yaw;
extern struct cvar_s	*m_forward;
extern struct cvar_s	*m_side;

extern struct cvar_s	*cl_name;
extern struct cvar_s	*cl_writecfg;

extern struct cvar_s	*cl_cshift_bonus;
extern struct cvar_s	*cl_cshift_contents;
extern struct cvar_s	*cl_cshift_damage;
extern struct cvar_s	*cl_cshift_powerup;

extern struct cvar_s	*noskins;

extern	client_state_t	cl;

// FIXME, allocate dynamically
extern	entity_t		cl_entities[MAX_EDICTS];
extern	cl_entity_state_t	cl_baselines[MAX_EDICTS];

extern int fps_count;

extern void (*write_angles) (sizebuf_t *sb, const vec3_t angles);

// cl_main
void CL_Init (void);
void CL_InitCvars (void);

void CL_EstablishConnection (const char *host);
void CL_Signon1 (void);
void CL_Signon2 (void);
void CL_Signon3 (void);
void CL_Signon4 (void);

void CL_Disconnect (void);
void CL_Disconnect_f (void);
void CL_NextDemo (void);


// cl_input
void CL_InitInput (void);
void CL_SendCmd (void);
void CL_SendMove (usercmd_t *cmd);

void CL_ParseParticleEffect (void);
void CL_ParseTEnt (void);
void CL_UpdateTEnts (void);

void CL_ClearState (void);

int  CL_ReadFromServer (void);
void CL_WriteToServer (usercmd_t *cmd);
void CL_BaseMove (usercmd_t *cmd);

float CL_KeyState (kbutton_t *key);

// cl_demo.c
void CL_StopPlayback (void);
void CL_StopRecording (void);
int CL_GetMessage (void);
void CL_Demo_Init (void);

extern struct cvar_s *demo_gzip;
extern struct cvar_s *demo_speed;

// cl_parse.c
struct skin_s;
void CL_ParseServerMessage (void);
void CL_NewTranslation (int slot, struct skin_s *skin);


// view
void V_StartPitchDrift (void);
void V_StopPitchDrift (void);

void V_RenderView (void);
void V_UpdatePalette (void);
void V_Register (void);
void V_ParseDamage (void);
void V_SetContentsColor (int contents);
void V_PrepBlend (void);

// cl_tent
void CL_TEnts_Init (void);
void CL_ClearEnts (void);
void CL_ClearTEnts (void);
void CL_Init_Entity (struct entity_s *ent);
void CL_ParseTEnt (void);
void CL_SignonReply (void);

extern kbutton_t   in_left, in_right, in_forward, in_back;
extern kbutton_t   in_lookup, in_lookdown, in_moveleft, in_moveright;
extern kbutton_t   in_use, in_jump, in_attack;
extern kbutton_t   in_up, in_down;

extern	double			realtime;

extern qboolean recording;

void Cvar_Info (struct cvar_s *var);

void CL_UpdateScreen (double realtime);

void CL_SetState (cactive_t state);

void CL_Cmd_ForwardToServer (void);

#endif // __client_h
