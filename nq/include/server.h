/*
	server.h

	@description@

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

#ifndef __server_h
#define __server_h

#include <setjmp.h>

#include "QF/info.h"
#include "QF/model.h"
#include "QF/quakeio.h"
#include "QF/sizebuf.h"

#include "client.h"
#include "netmain.h"
#include "protocol.h"
#include "sv_progs.h"

extern progs_t sv_pr_state;

typedef struct
{
	int			maxclients;
	int			maxclientslimit;
	struct client_s	*clients;		// [maxclients]
	void		(*phys_client) (struct edict_s *ent, int num);
	int			serverflags;		// episode completion information
	qboolean	changelevel_issued;	// cleared when at SV_SpawnServer
} server_static_t;

//=============================================================================

typedef enum {ss_loading, ss_active} server_state_t;

typedef struct
{
	qboolean	active;				// false if only a net client

	qboolean	paused;
	qboolean	loadgame;			// handle connections specially

	double		time;

	int			lastcheck;			// used by PF_checkclient
	double		lastchecktime;

	char		name[64];			// map name
	char		modelname[64];		// maps/<name>.bsp, for model_precache[0]
	struct model_s 	*worldmodel;
	const char	*model_precache[MAX_MODELS];	// NULL terminated
	struct model_s	*models[MAX_MODELS];
	const char	*sound_precache[MAX_SOUNDS];	// NULL terminated
	const char	*lightstyles[MAX_LIGHTSTYLES];
	int			num_edicts;
	int			max_edicts;
	edict_t		*edicts;			// can NOT be array indexed, because
									// edict_t is variable sized, but can
									// be used to reference the world ent
	server_state_t	state;			// some actions are valid only during load

	sizebuf_t	datagram;
	byte		datagram_buf[MAX_DATAGRAM];

	sizebuf_t	reliable_datagram;	// copied to all clients at end of frame
	byte		reliable_datagram_buf[MAX_DATAGRAM];

	sizebuf_t	signon;
	byte		signon_buf[MAX_MSGLEN-2];

	unsigned    protocol;
} server_t;


#define	NUM_PING_TIMES		16
#define	NUM_SPAWN_PARMS		16

typedef struct client_s
{
	qboolean		active;				// false = client is free
	qboolean		spawned;			// false = don't send datagrams
	qboolean		dropasap;			// has been told to go to another level
	qboolean		privileged;			// can execute any host command
	qboolean		sendsignon;			// valid only before spawned

	double			last_message;		// reliable messages must be sent
										// periodically

	struct qsocket_s *netconnection;	// communications handle

	usercmd_t		cmd;				// movement
	vec3_t			wishdir;			// intended motion calced from cmd

	sizebuf_t		message;			// can be added to at any time,
										// copied and clear once per frame
	byte			msgbuf[MAX_MSGLEN];
	edict_t			*edict;				// EDICT_NUM(clientnum+1)
	char			name[32];			// for printing to other people
	int				colors;

	float			ping_times[NUM_PING_TIMES];
	int				num_pings;			// ping_times[num_pings%NUM_PING_TIMES]

// spawn parms are carried from level to level
	float			spawn_parms[NUM_SPAWN_PARMS];

// client known data for deltas
	int				old_frags;
} client_t;


//=============================================================================

// edict->movetype values
#define	MOVETYPE_NONE			0		// never moves
#define	MOVETYPE_ANGLENOCLIP	1
#define	MOVETYPE_ANGLECLIP		2
#define	MOVETYPE_WALK			3		// gravity
#define	MOVETYPE_STEP			4		// gravity, special edge handling
#define	MOVETYPE_FLY			5
#define	MOVETYPE_TOSS			6		// gravity
#define	MOVETYPE_PUSH			7		// no clip to world, push and crush
#define	MOVETYPE_NOCLIP			8
#define	MOVETYPE_FLYMISSILE		9		// extra size to monsters
#define	MOVETYPE_BOUNCE			10

// edict->solid values
#define	SOLID_NOT				0		// no interaction with other objects
#define	SOLID_TRIGGER			1		// touch on edge, but not blocking
#define	SOLID_BBOX				2		// touch on edge, block
#define	SOLID_SLIDEBOX			3		// touch on edge, but not an onground
#define	SOLID_BSP				4		// bsp clip, touch on edge, block

// edict->deadflag values
#define	DEAD_NO					0
#define	DEAD_DYING				1
#define	DEAD_DEAD				2

#define	DAMAGE_NO				0
#define	DAMAGE_YES				1
#define	DAMAGE_AIM				2

// edict->flags
#define	FL_FLY					1
#define	FL_SWIM					2
//#define	FL_GLIMPSE				4
#define	FL_CONVEYOR				4
#define	FL_CLIENT				8
#define	FL_INWATER				16
#define	FL_MONSTER				32
#define	FL_GODMODE				64
#define	FL_NOTARGET				128
#define	FL_ITEM					256
#define	FL_ONGROUND				512
#define	FL_PARTIALGROUND		1024	// not all corners are valid
#define	FL_WATERJUMP			2048	// player jumping out of water
#define	FL_JUMPRELEASED			4096	// for jump debouncing
#define FL_FINALIZED			8192
#define FL_FINDABLE_NONSOLID	16384

#define TL_ANY_SOLID		0
#define TL_BSP_ONLY			1
// 2 used internally (MOVE_MISSILE)
#define TL_TRIGGERS			3   // scan for triggers
#define TL_EVERYTHING		4   // scan for anything

// entity effects

#define	EF_BRIGHTFIELD			1
#define	EF_MUZZLEFLASH 			2
#define	EF_BRIGHTLIGHT 			4
#define	EF_DIMLIGHT 			8

#define	SPAWNFLAG_NOT_EASY			256
#define	SPAWNFLAG_NOT_MEDIUM		512
#define	SPAWNFLAG_NOT_HARD			1024
#define	SPAWNFLAG_NOT_DEATHMATCH	2048


//============================================================================

extern	struct cvar_s	*teamplay;
extern	struct cvar_s	*skill;
extern	struct cvar_s	*deathmatch;
extern	struct cvar_s	*coop;
extern	struct cvar_s	*fraglimit;
extern	struct cvar_s	*timelimit;

extern	struct cvar_s	*sv_rollangle;
extern	struct cvar_s	*sv_rollspeed;

extern	struct cvar_s	*sv_maxvelocity;
extern	struct cvar_s	*sv_gravity;
extern	struct cvar_s	*sv_jump_any;
extern	struct cvar_s	*sv_nostep;
extern	struct cvar_s	*sv_friction;
extern	struct cvar_s	*sv_edgefriction;
extern	struct cvar_s	*sv_stopspeed;
extern	struct cvar_s	*sv_maxspeed;
extern	struct cvar_s	*sv_accelerate;
extern	struct cvar_s	*sv_idealpitchscale;
extern	struct cvar_s	*sv_aim;
extern	struct cvar_s	*sv_friction;
extern	struct cvar_s	*sv_stopspeed;

extern	struct cvar_s	*max_edicts;

extern	server_static_t	svs;				// persistant server info
extern	server_t		sv;					// local server

extern	client_t	*host_client;

extern	jmp_buf 	host_abortserver;

extern	double		host_time;
extern	double		sv_frametime;

extern	edict_t		*sv_player;

//===========================================================

void SV_Init (void);
void SV_PR_Cmds_Init (void);

void SV_StartParticle (const vec3_t org, const vec3_t dir, int color,
					   int count);
void SV_StartSound (edict_t *entity, int channel, const char *sample,
					int volume, float attenuation);

void SV_DropClient (qboolean crash);

void SV_SendClientMessages (void);
void SV_ClearDatagram (void);

int SV_ModelIndex (const char *name) __attribute__((pure));

void SV_SetIdealPitch (void);

void SV_AddUpdates (void);

void SV_ClientThink (void);
void SV_AddClientToServer (struct qsocket_s	*ret);

void SV_ClientPrintf (const char *fmt, ...) __attribute__((format(PRINTF,1,2)));
void SV_BroadcastPrintf (const char *fmt, ...) __attribute__((format(PRINTF,1,2)));

struct trace_s SV_PushEntity (edict_t *ent, vec3_t push);
int SV_EntCanSupportJump (edict_t *ent) __attribute__((pure));
int SV_FlyMove (edict_t *ent, float time, struct trace_s *steptrace);
void SV_CheckVelocity (edict_t *ent);
qboolean SV_RunThink (edict_t *ent);
void SV_AddGravity (edict_t *ent);
void SV_FinishGravity (edict_t *ent, vec3_t move);
void SV_Physics_Toss (edict_t *ent);
void SV_Physics_Client (edict_t *ent, int num);
void SV_Physics (void);
void SV_ProgStartFrame (void);
void SV_RunNewmis (void);

qboolean SV_CheckBottom (edict_t *ent);
qboolean SV_movestep (edict_t *ent, const vec3_t move, qboolean relink);

void SV_WriteClientdataToMessage (edict_t *ent, sizebuf_t *msg);

void SV_MoveToGoal (progs_t *pr);

void SV_CheckForNewClients (void);
void SV_RunClients (void);
void SV_SaveSpawnparms (void);
void SV_SpawnServer (const char *server);

void SV_LoadProgs (void);
void SV_Progs_Init (void);
void SV_Progs_Init_Cvars (void);

void Cvar_Info (struct cvar_s *var);

//FIXME location
#define		STOP_EPSILON	0.1

extern struct clip_hull_s *pf_hull_list[];

#endif // __server_h
