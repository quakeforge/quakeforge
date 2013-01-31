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

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/cbuf.h"
#include "QF/cdaudio.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/input.h"
#include "QF/joystick.h"
#include "QF/keys.h"
#include "QF/msg.h"
#include "QF/qfplist.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/skin.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/plugin/console.h"
#include "QF/plugin/vid_render.h"

#include "chase.h"
#include "cl_skin.h"
#include "client.h"
#include "clview.h"
#include "compat.h"
#include "host.h"
#include "host.h"
#include "server.h"
#include "sbar.h"

CLIENT_PLUGIN_PROTOS
static plugin_list_t client_plugin_list[] = {
		CLIENT_PLUGIN_LIST
};

// these two are not intended to be set directly
cvar_t     *cl_name;
cvar_t     *cl_color;

cvar_t     *cl_writecfg;

cvar_t     *cl_shownet;
cvar_t     *cl_nolerp;

cvar_t     *cl_cshift_bonus;
cvar_t     *cl_cshift_contents;
cvar_t     *cl_cshift_damage;
cvar_t     *cl_cshift_powerup;

cvar_t     *lookspring;

cvar_t     *m_pitch;
cvar_t     *m_yaw;
cvar_t     *m_forward;
cvar_t     *m_side;

cvar_t     *hud_fps;
cvar_t     *hud_time;

int         fps_count;

client_static_t cls;
client_state_t cl;

/*
	CL_WriteConfiguration

	Writes key bindings and archived cvars to config.cfg
*/
static void
CL_WriteConfiguration (void)
{
	QFile      *f;

	// dedicated servers initialize the host but don't parse and set the
	// config.cfg cvars
	if (host_initialized && !isDedicated && cl_writecfg->int_val) {
		char       *path = va ("%s/config.cfg", qfs_gamedir->dir.def);
		f = QFS_WOpen (path, 0);
		if (!f) {
			Sys_Printf ("Couldn't write config.cfg.\n");
			return;
		}

		Key_WriteBindings (f);
		Cvar_WriteVariables (f);
		Joy_WriteBindings (f);

		Qclose (f);
	}
}

void
CL_Shutdown (void)
{
	CL_WriteConfiguration ();
	CDAudio_Shutdown ();
	S_Shutdown ();
	IN_Shutdown ();
	VID_Shutdown ();
}

void
CL_InitCvars (void)
{
	VID_Init_Cvars ();
	IN_Init_Cvars ();
	Mod_Init_Cvars ();
	S_Init_Cvars ();

	CL_Demo_Init ();
	Chase_Init_Cvars ();
	V_Init_Cvars ();

	cl_cshift_bonus = Cvar_Get ("cl_cshift_bonus", "1", CVAR_ARCHIVE, NULL,
								"Show bonus flash on item pickup");
	cl_cshift_contents = Cvar_Get ("cl_cshift_content", "1", CVAR_ARCHIVE,
								   NULL, "Shift view colors for contents "
								   "(water, slime, etc)");
	cl_cshift_damage = Cvar_Get ("cl_cshift_damage", "1", CVAR_ARCHIVE, NULL,
								 "Shift view colors on damage");
	cl_cshift_powerup = Cvar_Get ("cl_cshift_powerup", "1", CVAR_ARCHIVE, NULL,                             "Shift view colors for powerups");
	cl_name = Cvar_Get ("_cl_name", "player", CVAR_ARCHIVE, NULL,
						"Player name");
	cl_color = Cvar_Get ("_cl_color", "0", CVAR_ARCHIVE, NULL, "Player color");
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
	hud_fps = Cvar_Get ("hud_fps", "0", CVAR_ARCHIVE, NULL,
						"display realtime frames per second");
	Cvar_MakeAlias ("show_fps", hud_fps);
	hud_time = Cvar_Get ("hud_time", "0", CVAR_ARCHIVE, NULL,
						 "display the current time");
}

void
CL_ClearState (void)
{
	if (!sv.active)
		Host_ClearMemory ();

	if (cl.edicts)
		PL_Free (cl.edicts);

	if (cl.scores) {
		int         i;

		for (i = 0; i < cl.maxclients; i++)
			Info_Destroy (cl.scores[i].userinfo);
	}

	// wipe the entire cl structure
	memset (&cl, 0, sizeof (cl));
	cl.chase = 1;
	cl.watervis = 1;
	r_data->force_fullscreen = 0;
	r_data->lightstyle = cl.lightstyle;

	CL_Init_Entity (&cl.viewent);
	r_data->view_model = &cl.viewent;

	SZ_Clear (&cls.message);

	CL_ClearTEnts ();

	r_funcs->R_ClearState ();

	CL_ClearEnts ();
}

/*
	CL_StopCshifts

	Cleans the Cshifts, so your screen doesn't stay red after a timedemo :)
*/
static void
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
	S_StopAllSounds ();

	// Clean the Cshifts
	CL_StopCshifts ();

	// bring the console down and fade the colors back to normal
//	SCR_BringDownConsole ();

	// if running a local server, shut it down
	if (cls.demoplayback)
		CL_StopPlayback ();
	else if (cls.state >= ca_connected) {
		if (cls.demorecording)
			CL_StopRecording ();

		Sys_MaskPrintf (SYS_DEV, "Sending clc_disconnect\n");
		SZ_Clear (&cls.message);
		MSG_WriteByte (&cls.message, clc_disconnect);
		NET_SendUnreliableMessage (cls.netcon, &cls.message);
		SZ_Clear (&cls.message);
		NET_Close (cls.netcon);

		CL_SetState (ca_disconnected);
		if (sv.active)
			Host_ShutdownServer (false);
	}

	cl.worldmodel = NULL;
	cl.intermission = 0;
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
CL_EstablishConnection (const char *host)
{
	if (cls.state == ca_dedicated)
		return;

	if (cls.demoplayback)
		return;

	CL_Disconnect ();

	cls.netcon = NET_Connect (host);
	if (!cls.netcon)
		Host_Error ("CL_Connect: connect failed\n");
	Sys_MaskPrintf (SYS_DEV, "CL_EstablishConnection: connected to %s\n",
					host);

	cls.demonum = -1;					// not in the demo loop now
	CL_SetState (ca_connected);
	Key_SetKeyDest (key_game);
}

/*
	CL_SignonReply

	An svc_signonnum has been received, perform a client side setup
*/
void
CL_SignonReply (void)
{
	Sys_MaskPrintf (SYS_DEV, "CL_SignonReply: %i\n", cls.signon);

	switch (cls.signon) {
	case so_none:
		break;
	case so_prespawn:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, "prespawn");
		break;

	case so_spawn:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, va ("name \"%s\"\n", cl_name->string));
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message,
						 va ("color %i %i\n", (cl_color->int_val) >> 4,
							 (cl_color->int_val) & 15));
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, "spawn");
		break;

	case so_begin:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, "begin");
		Cache_Report ();				// print remaining memory
		break;

	case so_active:
		cl.loading = false;
		CL_SetState (ca_active);
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
	if (cls.demonum == -1)
		return;							// don't play demos

	cl.loading = true;
	CL_UpdateScreen(cl.time);

	if (!cls.demos[cls.demonum][0] || cls.demonum == MAX_DEMOS) {
		cls.demonum = 0;
		if (!cls.demos[cls.demonum][0]) {
			Sys_Printf ("No demos listed with startdemos\n");
			cls.demonum = -1;
			return;
		}
	}

	Cbuf_InsertText (host_cbuf, va ("playdemo %s\n", cls.demos[cls.demonum]));
	cls.demonum++;
}

static void
CL_PrintEntities_f (void)
{
	entity_t   *ent;
	int         i;

	for (i = 0, ent = cl_entities; i < cl.num_entities; i++, ent++) {
		Sys_Printf ("%3i:", i);
		if (!ent->model) {
			Sys_Printf ("EMPTY\n");
			continue;
		}
		Sys_Printf ("%s:%2i  (%5.1f,%5.1f,%5.1f) [%5.1f %5.1f %5.1f]\n",
					ent->model->name, ent->frame, VectorExpand (ent->origin),
					VectorExpand (ent->angles));
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
	} while (ret && cls.state >= ca_connected);

	if (cl_shownet->int_val)
		Sys_Printf ("\n");

	CL_RelinkEntities ();
	CL_UpdateTEnts ();

	// bring the links up to date
	return 0;
}

void
CL_SendCmd (void)
{
	usercmd_t   cmd;

	if (cls.state < ca_connected)
		return;

	if (cls.state == ca_active) {
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
		Sys_MaskPrintf (SYS_DEV, "CL_WriteToServer: can't send\n");
		return;
	}

	if (NET_SendMessage (cls.netcon, &cls.message) == -1)
		Host_Error ("CL_WriteToServer: lost server connection");

	SZ_Clear (&cls.message);
}

void
CL_SetState (cactive_t state)
{
	cactive_t   old_state = cls.state;
	cls.state = state;
	Sys_MaskPrintf (SYS_NET, "CL_SetState: %d -> %d\n", old_state, state);
	if (old_state != state) {
		if (old_state == ca_active) {
			// leaving active state
			Key_SetKeyDest (key_console);
			S_AmbientOff ();
		}
		switch (state) {
			case ca_dedicated:
				break;
			case ca_disconnected:
				cls.signon = so_none;
				cl.loading = false;
				VID_SetCaption ("Disconnected");
				break;
			case ca_connected:
				cls.signon = so_none;		// need all the signon messages
											// before playing
				cl.loading = true;
				Key_SetKeyDest (key_game);
				IN_ClearStates ();
				VID_SetCaption ("Connected");
				break;
			case ca_active:
				// entering active state
				cl.loading = false;
				Key_SetKeyDest (key_game);
				IN_ClearStates ();
				VID_SetCaption ("");
				S_AmbientOn ();
				break;
		}
		CL_UpdateScreen (cl.time);
	}
	if (con_module)
		con_module->data->console->force_commandline = (state < ca_connected);
}

static void
Force_CenterView_f (void)
{
	cl.viewangles[PITCH] = 0;
}

void
CL_Init (cbuf_t *cbuf)
{
	byte       *basepal, *colormap;

	basepal = (byte *) QFS_LoadHunkFile ("gfx/palette.lmp");
	if (!basepal)
		Sys_Error ("Couldn't load gfx/palette.lmp");
	colormap = (byte *) QFS_LoadHunkFile ("gfx/colormap.lmp");
	if (!colormap)
		Sys_Error ("Couldn't load gfx/colormap.lmp");

	W_LoadWadFile ("gfx.wad");
	VID_Init (basepal, colormap);
	IN_Init (cbuf);
	R_Init ();
	r_data->lightstyle = cl.lightstyle;

	S_Init (&cl.viewentity, &host_frametime);

	PI_RegisterPlugins (client_plugin_list);
	Con_Init ("client");

	CDAudio_Init ();

	Sbar_Init ();

	CL_Input_Init ();
	CL_TEnts_Init ();
	CL_ClearState ();

	V_Init ();

	Cmd_AddCommand ("entities", CL_PrintEntities_f, "No Description");
	Cmd_AddCommand ("disconnect", CL_Disconnect_f, "No Description");
	Cmd_AddCommand ("maplist", Con_Maplist_f, "List available maps");
	Cmd_AddCommand ("skyboxlist", Con_Skyboxlist_f, "List skyboxes available");
	Cmd_AddCommand ("demolist", Con_Demolist_DEM_f, "List available demos");
	Cmd_AddCommand ("force_centerview", Force_CenterView_f, "force the view "
					"to be level");

	SZ_Alloc (&cls.message, 1024);
	CL_SetState (ca_disconnected);
}
