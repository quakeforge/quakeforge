/*
	cl_main.c

	entity parsing and management

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/keys.h"
#include "QF/msg.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/skin.h"
#include "QF/va.h"

#include "chase.h"
#include "client.h"
#include "compat.h"
#include "host.h"
#include "host.h"
#include "r_cvar.h"
#include "r_dynamic.h"
#include "server.h"

byte       *vid_colormap;

// these two are not intended to be set directly
cvar_t     *cl_name;

cvar_t     *cl_writecfg;

cvar_t     *cl_shownet;
cvar_t     *cl_nolerp;
cvar_t     *cl_sbar;
cvar_t     *cl_sbar_separator;
cvar_t     *cl_hudswap;

cvar_t     *cl_cshift_bonus; 
cvar_t     *cl_cshift_contents; 
cvar_t     *cl_cshift_damage; 
cvar_t     *cl_cshift_powerup; 

cvar_t     *lookspring;

cvar_t     *m_pitch;
cvar_t     *m_yaw;
cvar_t     *m_forward;
cvar_t     *m_side;

cvar_t     *show_fps;
cvar_t     *show_time;

int         fps_count;

client_static_t cls;
client_state_t cl;

// FIXME: put these on hunk?
entity_t    cl_entities[MAX_EDICTS];
entity_state_t    cl_baselines[MAX_EDICTS];
entity_t    cl_static_entities[MAX_STATIC_ENTITIES];
entity_state_t    cl_static_entity_baselines[MAX_STATIC_ENTITIES];


void
CL_Sbar_f (cvar_t *var)
{   
	vid.recalc_refdef = true;
	r_lineadj = var->int_val ? sb_lines : 0;
}


void
CL_InitCvars (void)
{
	cl_cshift_bonus = Cvar_Get ("cl_cshift_bonus", "1", CVAR_ARCHIVE, NULL,
								"Show bonus flash on item pickup");
	cl_cshift_contents = Cvar_Get ("cl_cshift_content", "1", CVAR_ARCHIVE,
								   NULL, "Shift view colors for contents "
								   "(water, slime, etc)");
	cl_cshift_damage = Cvar_Get ("cl_cshift_damage", "1", CVAR_ARCHIVE, NULL, 
								 "Shift view colors on damage");
	cl_cshift_powerup = Cvar_Get ("cl_cshift_powerup", "1", CVAR_ARCHIVE, NULL,                             "Shift view colors for powerups");
	cl_warncmd = Cvar_Get ("cl_warncmd", "0", CVAR_NONE, NULL,
						   "inform when execing a command");
	cl_name = Cvar_Get ("_cl_name", "player", CVAR_ARCHIVE, NULL,
						"Player name");
	cl_color = Cvar_Get ("_cl_color", "0", CVAR_ARCHIVE, NULL,
						 "Player color");
	cl_anglespeedkey = Cvar_Get ("cl_anglespeedkey", "1.5", CVAR_NONE, NULL,
								 "turn `run' speed multiplier");
	cl_backspeed = Cvar_Get ("cl_backspeed", "200", CVAR_ARCHIVE, NULL,
							 "backward speed");
	cl_forwardspeed = Cvar_Get ("cl_forwardspeed", "200", CVAR_ARCHIVE, NULL,
								"forward speed");
	cl_movespeedkey = Cvar_Get ("cl_movespeedkey", "2.0", CVAR_NONE, NULL,
								"move `run' speed multiplier");
	cl_pitchspeed = Cvar_Get ("cl_pitchspeed", "150", CVAR_NONE, NULL,
							  "look up/down speed");
	cl_sidespeed = Cvar_Get ("cl_sidespeed", "350", CVAR_NONE, NULL,
							 "strafe speed");
	cl_upspeed = Cvar_Get ("cl_upspeed", "200", CVAR_NONE, NULL,
						   "swim/fly up/down speed");
	cl_yawspeed = Cvar_Get ("cl_yawspeed", "140", CVAR_NONE, NULL,
							"turning speed");
	cl_writecfg = Cvar_Get ("cl_writecfg", "1", CVAR_NONE, NULL,
							"write config files?");
	cl_shownet = Cvar_Get ("cl_shownet", "0", CVAR_NONE, NULL,
						   "show network packets. 0=off, 1=basic, 2=verbose");
	cl_nolerp = Cvar_Get ("cl_nolerp", "0", CVAR_NONE, NULL,
						  "linear motion interpolation");
	cl_sbar = Cvar_Get ("cl_sbar", "0", CVAR_ARCHIVE, CL_Sbar_f,
						"status bar mode");
	cl_hudswap = Cvar_Get ("cl_hudswap", "0", CVAR_ARCHIVE, NULL,
						   "new HUD on left side?");
	lookspring = Cvar_Get ("lookspring", "0", CVAR_ARCHIVE, NULL, "Snap view "
						   "to center when moving and no mlook/klook");
	m_pitch = Cvar_Get ("m_pitch", "0.022", CVAR_ARCHIVE, NULL,
						"mouse pitch (up/down) multipier");
	m_yaw =	Cvar_Get ("m_yaw", "0.022", CVAR_ARCHIVE, NULL,
					  "mouse yaw (left/right) multipiler");
	m_forward = Cvar_Get ("m_forward", "1", CVAR_ARCHIVE, NULL,
						  "mouse forward/back speed");
	m_side = Cvar_Get ("m_side", "0.8", CVAR_ARCHIVE, NULL,
					   "mouse strafe speed");
	show_fps = Cvar_Get ("show_fps", "0", CVAR_NONE, NULL,
						 "display realtime frames per second");
	show_time = Cvar_Get ("show_time", "0", CVAR_NONE, NULL,
						  "display the current time");
}


void
CL_ClearState (void)
{
	int         i;

	if (!sv.active)
		Host_ClearMemory ();

	// wipe the entire cl structure
	memset (&cl, 0, sizeof (cl));

	SZ_Clear (&cls.message);

	// clear other arrays   
	memset (cl_entities, 0, sizeof (cl_entities));
	memset (cl_baselines, 0, sizeof (cl_baselines));
	memset (r_lightstyle, 0, sizeof (r_lightstyle));

	CL_ClearTEnts ();

	R_ClearEfrags ();
	R_ClearDlights ();
	R_ClearFires ();

	for (i = 0; i < MAX_EDICTS; i++) {
		cl_entities[i].baseline = &cl_baselines[i];
		cl_entities[i].baseline->alpha = 255;
		cl_entities[i].baseline->scale = 16;
		cl_entities[i].baseline->glow_color = 254;
		cl_entities[i].baseline->glow_size = 0;
		cl_entities[i].baseline->colormod = 255;
	}
}


/*
	CL_StopCshifts

	Cleans the Cshifts, so your screen doesn't stay red after a timedemo :)
*/
void
CL_StopCshifts (void)
{
	int i;
	for (i = 0; i < NUM_CSHIFTS; i++)
		cl.cshifts[i].percent = 0;
	for (i = 0; i < MAX_CL_STATS; i++)
		cl.stats[i] = 0;
}


/*
	CL_Disconnect

	Sends a disconnect message to the server
	This is also called on Host_Error, so it shouldn't cause any errors
*/
void
CL_Disconnect (void)
{
	// stop sounds (especially looping!)
	S_StopAllSounds (true);

	// Clean the Cshifts
	CL_StopCshifts ();

	// bring the console down and fade the colors back to normal
//	SCR_BringDownConsole ();

	// if running a local server, shut it down
	if (cls.demoplayback)
		CL_StopPlayback ();
	else if (cls.state == ca_connected) {
		if (cls.demorecording)
			CL_Stop_f ();

		Con_DPrintf ("Sending clc_disconnect\n");
		SZ_Clear (&cls.message);
		MSG_WriteByte (&cls.message, clc_disconnect);
		NET_SendUnreliableMessage (cls.netcon, &cls.message);
		SZ_Clear (&cls.message);
		NET_Close (cls.netcon);

		cls.state = ca_disconnected;
		if (sv.active)
			Host_ShutdownServer (false);
	}

	cls.demoplayback = cls.timedemo = false;
	cls.signon = 0;
}


void
CL_Disconnect_f (void)
{
	CL_Disconnect ();
	if (sv.active)
		Host_ShutdownServer (false);
}


/*
	CL_EstablishConnection

	Host should be either "local" or a net address to be passed on
*/
void
CL_EstablishConnection (char *host)
{
	if (cls.state == ca_dedicated)
		return;

	if (cls.demoplayback)
		return;

	CL_Disconnect ();

	cls.netcon = NET_Connect (host);
	if (!cls.netcon)
		Host_Error ("CL_Connect: connect failed\n");
	Con_DPrintf ("CL_EstablishConnection: connected to %s\n", host);

	cls.demonum = -1;					// not in the demo loop now
	cls.state = ca_connected;
	cls.signon = 0;						// need all the signon messages
										// before playing
	key_dest = key_game;
}


/*
	CL_SignonReply

	An svc_signonnum has been received, perform a client side setup
*/
void
CL_SignonReply (void)
{
	char        str[8192];

	Con_DPrintf ("CL_SignonReply: %i\n", cls.signon);

	switch (cls.signon) {
	case 1:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, "prespawn");
		break;

	case 2:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, va ("name \"%s\"\n", cl_name->string));
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message,
						 va ("color %i %i\n", (cl_color->int_val) >> 4,
							 (cl_color->int_val) & 15));
		MSG_WriteByte (&cls.message, clc_stringcmd);
		snprintf (str, sizeof (str), "spawn %s", cls.spawnparms);
		MSG_WriteString (&cls.message, str);
		break;

	case 3:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, "begin");
		Cache_Report ();				// print remaining memory
		break;

	case 4:
//		SCR_EndLoadingPlaque ();		// allow normal screen updates
		break;
	}
}


/*
	CL_NextDemo

	Called to play the next demo in the demo loop
*/
void
CL_NextDemo (void)
{
	char        str[1024];

	if (cls.demonum == -1)
		return;							// don't play demos

	if (!cls.demos[cls.demonum][0] || cls.demonum == MAX_DEMOS) {
		cls.demonum = 0;
		if (!cls.demos[cls.demonum][0]) {
			Con_Printf ("No demos listed with startdemos\n");
			cls.demonum = -1;
			return;
		}
	}

	snprintf (str, sizeof (str), "playdemo %s\n", cls.demos[cls.demonum]);
	Cbuf_InsertText (str);
	cls.demonum++;
}


void
CL_PrintEntities_f (void)
{
	entity_t   *ent;
	int         i;

	for (i = 0, ent = cl_entities; i < cl.num_entities; i++, ent++) {
		Con_Printf ("%3i:", i);
		if (!ent->model) {
			Con_Printf ("EMPTY\n");
			continue;
		}
		Con_Printf ("%s:%2i  (%5.1f,%5.1f,%5.1f) [%5.1f %5.1f %5.1f]\n",
					ent->model->name, ent->frame, ent->origin[0],
					ent->origin[1], ent->origin[2], ent->angles[0],
					ent->angles[1], ent->angles[2]);
	}
}


/*
	SetPal

	Debugging tool, just flashes the screen
*/
void
SetPal (int i)
{
#if 0
	static int  old;
	byte        pal[768];
	int         c;

	if (i == old)
		return;
	old = i;

	if (i == 0)
		VID_SetPalette (vid_basepal);
	else if (i == 1) {
		for (c = 0; c < 768; c += 3) {
			pal[c] = 0;
			pal[c + 1] = 255;
			pal[c + 2] = 0;
		}
		VID_SetPalette (pal);
	} else {
		for (c = 0; c < 768; c += 3) {
			pal[c] = 0;
			pal[c + 1] = 0;
			pal[c + 2] = 255;
		}
		VID_SetPalette (pal);
	}
#endif
}


void
CL_NewDlight (int key, float x, float y, float z, float radius, float time,
			  int type)
{
	dlight_t   *dl;

	dl = R_AllocDlight (key);
	dl->origin[0] = x;
	dl->origin[1] = y;
	dl->origin[2] = z;
	dl->radius = radius;
	dl->die = cl.time + time;
	switch (type) {
	default:
	case 0:
		dl->color[0] = 0.4;
		dl->color[1] = 0.2;
		dl->color[2] = 0.05;
		break;
	case 1:						// blue
		dl->color[0] = 0.05;
		dl->color[1] = 0.05;
		dl->color[2] = 0.5;
		break;
	case 2:						// red
		dl->color[0] = 0.5;
		dl->color[1] = 0.05;
		dl->color[2] = 0.05;
		break;
	case 3:						// purple
		dl->color[0] = 0.5;
		dl->color[1] = 0.05;
		dl->color[2] = 0.5;
		break;
	}
}


/*
	CL_LerpPoint

	Determines the fraction between the last two messages that the objects
	should be put at.
*/
float
CL_LerpPoint (void)
{
	float       f, frac;

	f = cl.mtime[0] - cl.mtime[1];

	if (!f || cl_nolerp->int_val || cls.timedemo || sv.active) {
		cl.time = cl.mtime[0];
		return 1;
	}

	if (f > 0.1) {						// dropped packet, or start of demo
		cl.mtime[1] = cl.mtime[0] - 0.1;
		f = 0.1;
	}
	frac = (cl.time - cl.mtime[1]) / f;
//	Con_Printf ("frac: %f\n",frac);
	if (frac < 0) {
		if (frac < -0.01) {
			SetPal (1);
			cl.time = cl.mtime[1];
//			Con_Printf ("low frac\n");
		}
		frac = 0;
	} else if (frac > 1) {
		if (frac > 1.01) {
			SetPal (2);
			cl.time = cl.mtime[0];
//			Con_Printf ("high frac\n");
		}
		frac = 1;
	} else
		SetPal (0);

	return frac;
}


void
CL_RelinkEntities (void)
{
	entity_t  **_ent;
	entity_t   *ent;
	int         i, j;
	float       frac, f, d;
	vec3_t      delta;
	float       bobjrotate;
	dlight_t   *dl;
	
	r_player_entity = &cl_entities[cl.viewentity];

	// determine partial update time    
	frac = CL_LerpPoint ();

	// interpolate player info
	for (i = 0; i < 3; i++)
		cl.velocity[i] = cl.mvelocity[1][i] +
			frac * (cl.mvelocity[0][i] - cl.mvelocity[1][i]);

	if (cls.demoplayback) {
		// interpolate the angles   
		for (j = 0; j < 3; j++) {
			d = cl.mviewangles[0][j] - cl.mviewangles[1][j];
			if (d > 180)
				d -= 360;
			else if (d < -180)
				d += 360;
			cl.viewangles[j] = cl.mviewangles[1][j] + frac * d;
		}
	}

	bobjrotate = anglemod (100 * cl.time);

	// start on the entity after the world
	for (i = 1, ent = cl_entities + 1; i < cl.num_entities; i++, ent++) {
		if (!ent->model) {				// empty slot
			if (ent->forcelink)
				R_RemoveEfrags (ent);	// just became empty
			continue;
		}
		// if the object wasn't included in the last packet, remove it
		if (ent->msgtime != cl.mtime[0]) {
			ent->model = NULL;
			continue;
		}

		VectorCopy (ent->origin, ent->old_origin);

		if (ent->forcelink) {		// the entity was not updated in the
									// last message so move to the final spot
			VectorCopy (ent->msg_origins[0], ent->origin);
			VectorCopy (ent->msg_angles[0], ent->angles);
		} else {					// if the delta is large, assume a
									// teleport and don't lerp
			f = frac;
			for (j = 0; j < 3; j++) {
				delta[j] = ent->msg_origins[0][j] - ent->msg_origins[1][j];
				if (delta[j] > 100 || delta[j] < -100)
					f = 1;				// assume a teleportation, not a motion
			}

			// interpolate the origin and angles
			for (j = 0; j < 3; j++) {
				ent->origin[j] = ent->msg_origins[1][j] + f * delta[j];

				d = ent->msg_angles[0][j] - ent->msg_angles[1][j];
				if (d > 180)
					d -= 360;
				else if (d < -180)
					d += 360;
				ent->angles[j] = ent->msg_angles[1][j] + f * d;
			}

		}

		if (i <= cl.maxclients) {
			ent->skin = Skin_NewTempSkin ();
			if (ent->skin)
				CL_NewTranslation (i - 1, ent->skin);
		}

		// rotate binary objects locally
		if (ent->model->flags & EF_ROTATE)
			ent->angles[1] = bobjrotate;

		if (ent->effects & EF_BRIGHTFIELD)
			R_EntityParticles (ent);
#ifdef QUAKE2
		if (ent->effects & EF_DARKFIELD)
			R_DarkFieldParticles (ent);
#endif
		if (ent->effects & EF_MUZZLEFLASH) {
			vec3_t      fv, rv, uv;

			dl = R_AllocDlight (i);
			VectorCopy (ent->origin, dl->origin);
			dl->origin[2] += 16;
			AngleVectors (ent->angles, fv, rv, uv);

			VectorMA (dl->origin, 18, fv, dl->origin);
			dl->radius = 200 + (rand () & 31);
			dl->minlight = 32;
			dl->die = cl.time + 0.1;
			dl->color[0] = 0.2;
			dl->color[1] = 0.1;
			dl->color[2] = 0.05;
		}
		if ((ent->effects & (EF_BLUE | EF_RED)) == (EF_BLUE | EF_RED))
			CL_NewDlight (i, ent->origin[0], ent->origin[1], ent->origin[2],
						  200 + (rand () & 31), 0.1, 3);
		if (ent->effects & EF_BLUE)
			CL_NewDlight (i, ent->origin[0], ent->origin[1], ent->origin[2],
						  200 + (rand () & 31), 0.1, 1);
		if (ent->effects & EF_RED)
			CL_NewDlight (i, ent->origin[0], ent->origin[1], ent->origin[2],
						  200 + (rand () & 31), 0.1, 2);
		if (ent->effects & EF_BRIGHTLIGHT)
			CL_NewDlight (i, ent->origin[0], ent->origin[1],
						  ent->origin[2] + 16, 400 + (rand () & 31), 0.001, 0);
		if (ent->effects & EF_DIMLIGHT)
			CL_NewDlight (i, ent->origin[0], ent->origin[1], ent->origin[2],
						  200 + (rand () & 31), 0.001, 0);
#ifdef QUAKE2
		if (ent->effects & EF_DARKLIGHT) {
			dl = R_AllocDlight (i);
			VectorCopy (ent->origin, dl->origin);
			dl->radius = 200.0 + (rand () & 31);
			dl->die = cl.time + 0.001;
			dl->dark = true;
		}
		if (ent->effects & EF_LIGHT) {
			dl = R_AllocDlight (i);
			VectorCopy (ent->origin, dl->origin);
			dl->radius = 200;
			dl->die = cl.time + 0.001;
		}
#endif
		if (VectorDistance_fast(ent->msg_origins[1], ent->origin) > (256*256))
			VectorCopy (ent ->origin, ent->msg_origins[1]);
		if (ent->model->flags & EF_ROCKET) {
			dl = R_AllocDlight (i);
			VectorCopy (ent->origin, dl->origin);
			VectorCopy (r_firecolor->vec, dl->color);
			dl->radius = 200;
			dl->die = cl.time + 0.1;
			R_RocketTrail (0, ent);
		} else if (ent->model->flags & EF_GRENADE)
			R_RocketTrail (1, ent);
		else if (ent->model->flags & EF_GIB)
			R_RocketTrail (2, ent);
		else if (ent->model->flags & EF_ZOMGIB)
			R_RocketTrail (4, ent);
		else if (ent->model->flags & EF_TRACER)
			R_RocketTrail (3, ent);
		else if (ent->model->flags & EF_TRACER2)
			R_RocketTrail (5, ent);
		else if (ent->model->flags & EF_TRACER3)
			R_RocketTrail (6, ent);

		ent->forcelink = false;

		if (i == cl.viewentity && !chase_active->int_val) {
			continue;
		}
#ifdef QUAKE2
		if (ent->effects & EF_NODRAW)
			continue;
#endif
		if ((_ent = R_NewEntity ()))
			*_ent = ent;
	}
}


/*
	CL_ReadFromServer

	Read all incoming data from the server
*/
int
CL_ReadFromServer (void)
{
	int         ret;

	cl.oldtime = cl.time;
	cl.time += host_frametime;

	do {
		ret = CL_GetMessage ();
		if (ret == -1)
			Host_Error ("CL_ReadFromServer: lost server connection");
		if (!ret)
			break;

		cl.last_received_message = realtime;
		CL_ParseServerMessage ();
	} while (ret && cls.state == ca_connected);

	if (cl_shownet->int_val)
		Con_Printf ("\n");

	R_ClearEnts ();
	Skin_ClearTempSkins ();

	CL_RelinkEntities ();
	CL_UpdateTEnts ();

	// bring the links up to date
	return 0;
}


void
CL_SendCmd (void)
{
	usercmd_t   cmd;

	if (cls.state != ca_connected)
		return;

	if (cls.signon == SIGNONS) {
		CL_BaseMove (&cmd);

		// send the unreliable message
		CL_SendMove (&cmd);

	}

	if (cls.demoplayback) {
		SZ_Clear (&cls.message);
		return;
	}
	// send the reliable message
	if (!cls.message.cursize)
		return;							// no message at all

	if (!NET_CanSendMessage (cls.netcon)) {
		Con_DPrintf ("CL_WriteToServer: can't send\n");
		return;
	}

	if (NET_SendMessage (cls.netcon, &cls.message) == -1)
		Host_Error ("CL_WriteToServer: lost server connection");

	SZ_Clear (&cls.message);
}


void
CL_Init (void)
{
	int i;

	SZ_Alloc (&cls.message, 1024);

	CL_InitInput ();
	CL_TEnts_Init ();

	Cmd_AddCommand ("entities", CL_PrintEntities_f, "No Description");
	Cmd_AddCommand ("disconnect", CL_Disconnect_f, "No Description");
	Cmd_AddCommand ("record", CL_Record_f, "No Description");
	Cmd_AddCommand ("stop", CL_Stop_f, "No Description");
	Cmd_AddCommand ("playdemo", CL_PlayDemo_f, "No Description");
	Cmd_AddCommand ("timedemo", CL_TimeDemo_f, "No Description");
	Cmd_AddCommand ("maplist", COM_Maplist_f, "No Description");

	for (i = 0; i < MAX_STATIC_ENTITIES; i++) {
		cl_static_entities[i].baseline = &cl_static_entity_baselines[i];
	}
}
