/*
	server.h

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
// server.h

#ifndef _SERVER_H
#define _SERVER_H

#include <stdarg.h>

#include "QF/info.h"
#include "QF/model.h"
#include "QF/quakeio.h"
#include "QF/sizebuf.h"

#include "world.h"

#include "host.h"
#include "netchan.h"
#include "qw/bothdefs.h"
#include "qw/msg_backbuf.h"
#include "qw/protocol.h"

#define	QW_SERVER

#define	MAX_MASTERS	32				// max recipients for heartbeat packets

#define	MAX_SIGNON_BUFFERS	8

typedef enum {
	ss_dead,			// no map loaded
	ss_loading,			// spawning level edicts
	ss_active			// actively running
} server_state_t;
// some qc commands are valid only before the server has finished
// initializing (precache commands, static sounds / objects, etc)

typedef struct {
	qboolean	active;				// false when server is going down
	server_state_t	state;			// precache commands are valid only during load

	double		time;

	int			lastcheck;			// used by PF_checkclient
	double		lastchecktime;		// for monster ai

	qboolean	paused;				// are we paused?

	//check player/eyes models for hacks
	unsigned int	model_player_checksum;
	unsigned int	eyes_player_checksum;

	char		*name;			// map name
	char		modelname[MAX_QPATH];		// maps/<name>.bsp, for model_precache[0]
	struct model_s 	*worldmodel;
	const char	*model_precache[MAX_MODELS];	// NULL terminated
	const char	*sound_precache[MAX_SOUNDS];	// NULL terminated
	const char	*lightstyles[MAX_LIGHTSTYLES];
	struct model_s		*models[MAX_MODELS];

	unsigned    num_edicts;			// increases towards MAX_EDICTS
	struct edict_s		*edicts;			// can NOT be array indexed, because
									// struct edict_s is variable sized, but can
									// be used to reference the world ent

	struct set_s *pvs, *phs;			// fully expanded and decompressed

	//antilag
	float       lagentsfrac;
	laggedentinfo_t *lagents;
	unsigned    maxlagents;

	// added to every client's unreliable buffer each frame, then cleared
	sizebuf_t	datagram;
	byte		datagram_buf[MAX_DATAGRAM];

	// added to every client's reliable buffer each frame, then cleared
	sizebuf_t	reliable_datagram;
	byte		reliable_datagram_buf[MAX_MSGLEN];

	// the multicast buffer is used to send a message to a set of clients
	sizebuf_t	multicast;
	byte		multicast_buf[MAX_MSGLEN];

	// the master buffer is used for building log packets
	sizebuf_t	master;
	byte		master_buf[MAX_DATAGRAM];

	// the signon buffer will be sent to each client as they connect
	// includes the entity baselines, the static entities, etc
	// large levels will have >MAX_DATAGRAM sized signons, so
	// multiple signon messages are kept
	sizebuf_t	signon;
	int			num_signon_buffers;
	int         max_signon_buffers;	// grows;
	int        *signon_buffer_size;
	byte      (*signon_buffers)[MAX_DATAGRAM];

	// demo stuff
	int         recording_demo;
	struct recorder_s *recorders;
} server_t;

#define	NUM_SPAWN_PARMS			16

typedef enum {
	cs_free,		// can be reused for a new connection
	cs_server,		// client is grabbed by the server for its own purposes
	cs_zombie,		// client has been disconnected, but don't reuse
					// connection for a couple seconds
	cs_connected,	// has been assigned to a client_t, but not in game yet
	cs_spawned		// client is fully in game
} sv_client_state_t;

typedef struct {
	// received from client

	// reply
	double				senttime;
	float				ping_time;
	vec3_t              playerpositions[MAX_CLIENTS];
	qboolean            playerpresent[MAX_CLIENTS];
	packet_entities_t	entities;
	packet_players_t	players;
} client_frame_t;

typedef enum {
	dt_tp_normal,
	dt_tp_demo,
	dt_tp_qtv,
} delta_type_t;

typedef enum {
	dt_pvs_normal,
	dt_pvs_fat,
	dt_pvs_none,
} delta_pvs_t;

typedef struct {
	delta_type_t type;
	delta_pvs_t pvs;
	int         delta_sequence;
	int         cur_frame;
	int			out_frame;
	int			in_frame;
	struct client_s *client;
	client_frame_t frames[UPDATE_BACKUP];	// updates can be deltad from here
} delta_t;

#define MAX_BACK_BUFFERS	8
#define MAX_STUFFTEXT		512
#define MAX_NAME			32

typedef enum {
	ft_ban,
	ft_mute,		// mute penalty save over disconnect
	ft_cuff,		// cuff penatly save over disconnect
} filtertype_t;

typedef struct client_s {
	sv_client_state_t	state;
	int				ping;				// fake ping for server clients
	qboolean		prespawned;
	qboolean		spawned;

	int				spectator;			// non-interactive

	qboolean		sendinfo;			// at end of frame, send info to all
										// this prevents malicious multiple
										// broadcasts
	float			lastnametime;		// time of last name change
	int				lastnamecount;		// time of last name change
	qboolean		drop;				// lose this guy next opportunity
	int				lossage;			// loss percentage

	int				userid;				// identifying number
	struct info_s   *userinfo;			// infostring

	usercmd_t		lastcmd;			// for filling in big drops and partial predictions
	double			localtime;			// of last message
	int				oldbuttons;
	int				oldonground;

	float			maxspeed;			// localized maxspeed
	float			entgravity;			// localized ent gravity

	struct edict_s			*edict;		// EDICT_NUM(clientnum+1)
	char			name[MAX_NAME];		// for printing to other people
										// extracted from userinfo
	int				messagelevel;		// for filtering printed messages

	// the datagram is written to after every frame, but cleared only
	// when it is sent out to the client.  overflow is tolerated.
	sizebuf_t		datagram;
	byte			datagram_buf[MAX_DATAGRAM];

	// back buffers for client reliable data
	backbuf_t       backbuf;

	char			stufftext_buf[MAX_STUFFTEXT];

	double			connection_started;	// or time of disconnect for zombies
	qboolean		send_message;		// set on frames a datagram arived on

	//antilag stuff
	laggedentinfo_t laggedents[MAX_CLIENTS];
	unsigned        laggedents_count;
	float           laggedents_frac;

// spawn parms are carried from level to level
	float			spawn_parms[NUM_SPAWN_PARMS];

// client known data for deltas
	int				old_frags;

	int				stats[MAX_CL_STATS];

	delta_t			delta;

	QFile			*download;			// file being downloaded
	int				downloadsize;		// total bytes
	int				downloadcount;		// bytes sent

	int				spec_track;			// entnum of player tracking

	double			whensaid[10];       // JACK: For floodprots
 	int				whensaidhead;       // Head value for floodprots
 	double			lockedtill;

	QFile			*upload;
	struct dstring_s *uploadfn;
	netadr_t		snap_from;
	qboolean		remote_snap;

//===== NETWORK ============
	int				chokecount;
	netchan_t		netchan;
	int				msecs, msec_cheating;
	double			last_check;
	double			cuff_time;
	float			stdver;
} client_t;

// a client can leave the server in one of four ways:
// dropping properly by quiting or disconnecting
// timing out if no valid messages are received for timeout->value seconds
// getting kicked off by the server operator
// a program error, like an overflowed reliable buffer

extern qboolean rcon_from_user;			// current command is a from a user

//============================================================================


#define	STATFRAMES	100
typedef struct {
	double	active;
	double	idle;
	double  demo;
	int		count;
	int		packets;

	double	latched_active;
	double	latched_idle;
	double  latched_demo;
	int		latched_packets;
} svstats_t;

// MAX_CHALLENGES is made large to prevent a denial
// of service attack that could cycle all of them
// out before legitimate users connected
#define	MAX_CHALLENGES	1024

typedef struct {
	netadr_t	adr;
	int			challenge;
	int			time;
} challenge_t;

typedef struct {
	int			spawncount;			// number of servers spawned since start,
									// used to check late spawns
	client_t	clients[MAX_CLIENTS];
	unsigned    maxclients;
	unsigned    num_clients;
	int			serverflags;		// episode completion information
	void		(*phys_client) (struct edict_s *ent, int num);

	double		last_heartbeat;
	int			heartbeat_sequence;
	svstats_t	stats;

	info_t		*info;

	// log messages are used so that fraglog processes can get stats
	int			logsequence;	// the message currently being filled
	double		logtime;		// time of last swap
	sizebuf_t	log[2];
	byte		log_buf[2][MAX_DATAGRAM];

	challenge_t	challenges[MAX_CHALLENGES];	// to prevent invalid IPs from connecting

	// demo stuff
	byte        *demomem;
	int         demomemsize;
} server_static_t;

//=============================================================================
// DoSflood protection
//=============================================================================
typedef struct {
	netadr_t	adr;
	double		issued;
	int			floodcount;
	int			cmdcount;
	double		firstseen;
} flood_t;

typedef enum {
	FLOOD_PING,
	FLOOD_LOG,
	FLOOD_CONNECT,
	FLOOD_STATUS,
	FLOOD_RCON,
	FLOOD_BAN
} flood_enum_t;

#define DOSFLOODCMDS 6
#define DOSFLOODIP 64   // remember latest 64 IP's for each cmd.

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
#define MOVETYPE_PPUSH                  13               // no clip to world, push and crush

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
#define	FL_FLY					(1<<0)
#define	FL_SWIM					(1<<1)
#define	FL_GLIMPSE				(1<<2)
#define	FL_CLIENT				(1<<3)
#define	FL_INWATER				(1<<4)
#define	FL_MONSTER				(1<<5)
#define	FL_GODMODE				(1<<6)
#define	FL_NOTARGET				(1<<7)
#define	FL_ITEM					(1<<8)
#define	FL_ONGROUND				(1<<9)
#define	FL_PARTIALGROUND		(1<<10)	// not all corners are valid
#define	FL_WATERJUMP			(1<<11)	// player jumping out of water
// 4096 used by quakec
#define FL_FINALIZED			(1<<13)
#define FL_FINDABLE_NONSOLID	(1<<14)
#define FLQW_LAGGEDMOVE			(1<<16)

// entity effects

//define	EF_BRIGHTFIELD			1
//define	EF_MUZZLEFLASH 			2
#define	EF_BRIGHTLIGHT 			4
#define	EF_DIMLIGHT 			8


#define	SPAWNFLAG_NOT_EASY			256
#define	SPAWNFLAG_NOT_MEDIUM		512
#define	SPAWNFLAG_NOT_HARD			1024
#define	SPAWNFLAG_NOT_DEATHMATCH	2048

#define	MULTICAST_ALL			0
#define	MULTICAST_PHS			1
#define	MULTICAST_PVS			2

#define	MULTICAST_ALL_R			3
#define	MULTICAST_PHS_R			4
#define	MULTICAST_PVS_R			5

//============================================================================
// FIXME: declare exported variables in their own relevant .h

extern	struct cvar_s	*sv_hide_version_info;
extern	struct cvar_s	*sv_highchars;

extern	struct cvar_s	*sv_mintic, *sv_maxtic;
extern	struct cvar_s	*sv_maxspeed;

extern	struct cvar_s	*sv_timeout;

extern	netadr_t	master_adr[MAX_MASTERS];	// address of the master server

extern	struct cvar_s	*spawn;
extern	struct cvar_s	*teamplay;
extern	struct cvar_s	*deathmatch;
extern	struct cvar_s	*fraglimit;
extern	struct cvar_s	*timelimit;

extern	server_static_t	svs;				// persistant server info
extern	server_t		sv;					// local server

extern	client_t	*host_client;

extern	struct edict_s		*sv_player;

extern	char		localmodels[MAX_MODELS][5];	// inline model names for precache

extern	struct info_s	*localinfo;

extern	size_t		host_hunklevel;
extern	QFile		*sv_logfile;
extern	QFile		*sv_fraglogfile;

extern	double		sv_frametime;
extern	double		realtime;

extern	const char *client_info_filters[];

extern struct cbuf_s *sv_cbuf;
extern struct cbuf_args_s *sv_args;

//===========================================================
// FIXME: declare exported functions in their own relevant .h

void SV_Init (void);
void SV_Sbar_Init (void);
void SV_Progs_Init (void);
void SV_Progs_Init_Cvars (void);
void SV_PR_Cmds_Init (void);
void SV_LoadProgs (void);

void Con_Printf (const char *fmt, ...) __attribute__((format(PRINTF,1,2)));
void Con_DPrintf (const char *fmt, ...) __attribute__((format(PRINTF,1,2)));

extern struct clip_hull_s *pf_hull_list[];

//
// sv_main.c
//

void SV_OutOfBand (netadr_t adr, unsigned length, byte *data);
void SV_OutOfBandPrint (netadr_t adr, const char *format, ...)
		__attribute__ ((format (PRINTF,2,3)));


client_t *SV_AllocClient (int spectator, int server);

void SV_SavePenaltyFilter (client_t *cl, filtertype_t type, double pentime);
double SV_RestorePenaltyFilter (client_t *cl, filtertype_t type);

void SV_Shutdown (void *data);
void SV_Frame (float time);
void SV_FinalMessage (const char *message);
void SV_DropClient (client_t *drop);
int SV_CalcPing (client_t *cl) __attribute__((pure));
void SV_FullClientUpdate (client_t *client, sizebuf_t *buf);
void SV_FullClientUpdateToClient (client_t *client, backbuf_t *backbuf);

int SV_ModelIndex (const char *name) __attribute__((pure));

qboolean SV_CheckBottom (struct edict_s *ent);
qboolean SV_movestep (struct edict_s *ent, const vec3_t move,
					  qboolean relink);

void SV_WriteClientdataToMessage (client_t *client, sizebuf_t *msg);

struct progs_s;
void SV_MoveToGoal (struct progs_s *pr, void *data);

void SV_SaveSpawnparms (void);

void SV_Physics_Client (struct edict_s	*ent);

void SV_PreRunCmd (void);
void SV_RunCmd (usercmd_t *ucmd, qboolean inside);
void SV_PostRunCmd (void);
void SV_SetupUserCommands (void);
void SV_ExecuteUserCommand (const char *s);
void SV_InitOperatorCommands (void);
void SV_GIB_Init (void);

void SV_SendServerinfo (client_t *client);
void SV_ExtractFromUserinfo (client_t *cl);


void Master_Heartbeat (void);
void Master_Packet (void);

//
// sv_init.c
//
void SV_SpawnServer (const char *server);
void SV_FlushSignon (void);


//
// sv_phys.c
//
void SV_ProgStartFrame (void);
void SV_Physics (void);
void SV_CheckVelocity (struct edict_s *ent);
void SV_AddGravity (struct edict_s *ent);
void SV_FinishGravity (struct edict_s *ent, vec3_t move);
qboolean SV_RunThink (struct edict_s *ent);
void SV_Physics_Toss (struct edict_s *ent);
void SV_RunNewmis (void);
void SV_SetMoveVars(void);
struct trace_s;
int SV_FlyMove (struct edict_s *ent, float time, struct trace_s *steptrace);
struct trace_s SV_PushEntity (struct edict_s *ent, vec3_t push,
							  unsigned traceflags);
int SV_EntCanSupportJump (struct edict_s *ent) __attribute__((pure));

//
// sv_send.c
//
void SV_Print (const char *fmt, va_list args) __attribute__((format(PRINTF, 1, 0)));
void SV_Printf (const char *fmt, ...) __attribute__((format(PRINTF,1,2)));
void SV_SendClientMessages (void);
void SV_GetStats (struct edict_s *ent, int spectator, int stats[]);

void SV_Multicast (const vec3_t origin, int to);
void SV_StartSound (struct edict_s *entity, int channel, const char *sample,
					int volume, float attenuation);
void SV_ClientPrintf (int recorder, client_t *cl, int level, const char *fmt, ...) __attribute__((format(PRINTF,4,5)));
void SV_BroadcastPrintf (int level, const char *fmt, ...) __attribute__((format(PRINTF,2,3)));
void SV_BroadcastCommand (const char *fmt, ...) __attribute__((format(PRINTF,1,2)));
void SV_SendMessagesToAll (void);
void SV_FindModelNumbers (void);

//
// sv_user.c
//

#define UCMD_NO_REDIRECT 1
#define UCMD_OVERRIDABLE 2

void SV_WriteWorldVars (netchan_t *netchan);
void SV_WriteSoundlist (netchan_t *netchan, int n);
void SV_WriteModellist (netchan_t *netchan, int n);
void SV_WriteSpawn1 (backbuf_t *backbuf, int n);
void SV_WriteSpawn2 (backbuf_t *backbuf);
void SV_ExecuteClientMessage (client_t *cl);
void SV_UserInit (void);
void SV_TogglePause (const char *msg);
void *SV_AddUserCommand (const char *name, void (*func) (void *userdata),
				         int flags,
				         void *userdata,
				         void (*on_free) (void *userdata));
int SV_RemoveUserCommand (void *cmd);
void SV_Spawn (client_t *client);
void SV_SetUserinfo (client_t *client, const char *key, const char *value);
extern int (*ucmd_unknown)(void);

//
// svonly.c
//
typedef enum {
	RD_NONE,
	RD_CLIENT,
	RD_PACKET,
	RD_MOD,
} redirect_t;

void SV_BeginRedirect (redirect_t rd);
void SV_EndRedirect (void);
extern redirect_t sv_redirected;
extern struct dstring_s outputbuf;

//
// sv_ccmds.c
//
void SV_Status_f (void);
const char *SV_Current_Map (void) __attribute__((pure));
void SV_SetLocalinfo (const char *key, const char *value);


//
// sv_ents.c
//
void SV_WriteEntitiesToClient (delta_t *delta, sizebuf_t *msg);

//
// sv_nchan.c
//

void Cvar_Info (struct cvar_s *var);

extern struct cvar_s *sv_antilag;
extern struct cvar_s *sv_antilag_frac;
extern struct cvar_s *sv_timecheck_fuzz;
extern struct cvar_s *sv_timecheck_decay;
extern struct cvar_s *sv_maxrate;
extern struct cvar_s *sv_timestamps;
extern struct cvar_s *sv_timefmt;
extern struct cvar_s *sv_phs;
extern struct cvar_s *sv_maxvelocity;
extern struct cvar_s *sv_gravity;
extern struct cvar_s *sv_jump_any;
extern struct cvar_s *sv_aim;
extern struct cvar_s *sv_stopspeed;
extern struct cvar_s *sv_spectatormaxspeed;
extern struct cvar_s *sv_accelerate;
extern struct cvar_s *sv_airaccelerate;
extern struct cvar_s *sv_wateraccelerate;
extern struct cvar_s *sv_friction;
extern struct cvar_s *sv_waterfriction;
extern struct cvar_s *pr_double_remove;
extern struct cvar_s *allow_download;
extern struct cvar_s *allow_download_skins;
extern struct cvar_s *allow_download_models;
extern struct cvar_s *allow_download_sounds;
extern struct cvar_s *allow_download_maps;

extern int fp_messages;
extern int fp_persecond;
extern int fp_secondsdead;
extern struct cvar_s *pausable;
extern qboolean nouse;

extern char fp_msg[255];

extern int sv_nailmodel, sv_supernailmodel, sv_playermodel;

extern int con_printf_no_log;

//FIXME location
#define STOP_EPSILON 0.1

#endif // _SERVER_H
