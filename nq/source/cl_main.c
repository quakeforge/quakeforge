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
#include "QF/gib.h"
#include "QF/input.h"
#include "QF/image.h"
#include "QF/joystick.h"
#include "QF/keys.h"
#include "QF/msg.h"
#include "QF/png.h"
#include "QF/plist.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/skin.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/plugin/console.h"
#include "QF/plugin/vid_render.h"
#include "QF/scene/entity.h"
#include "QF/scene/scene.h"

#include "compat.h"
#include "sbar.h"

#include "client/chase.h"
#include "client/particles.h"
#include "client/temp_entities.h"
#include "client/world.h"

#include "nq/include/cl_skin.h"
#include "nq/include/client.h"
#include "nq/include/host.h"
#include "nq/include/server.h"

CLIENT_PLUGIN_PROTOS
static plugin_list_t client_plugin_list[] = {
		CLIENT_PLUGIN_LIST
};

// these two are not intended to be set directly
char *cl_name;
static cvar_t cl_name_cvar = {
	.name = "_cl_name",
	.description =
		"Player name",
	.default_value = "player",
	.flags = CVAR_ARCHIVE,
	.value = { .type = 0, .value = &cl_name },
};
int cl_color;
static cvar_t cl_color_cvar = {
	.name = "_cl_color",
	.description =
		"Player color",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &cl_color },
};

int cl_writecfg;
static cvar_t cl_writecfg_cvar = {
	.name = "cl_writecfg",
	.description =
		"write config files?",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &cl_writecfg },
};

int cl_shownet;
static cvar_t cl_shownet_cvar = {
	.name = "cl_shownet",
	.description =
		"show network packets. 0=off, 1=basic, 2=verbose",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &cl_shownet },
};
int cl_nolerp;
static cvar_t cl_nolerp_cvar = {
	.name = "cl_nolerp",
	.description =
		"linear motion interpolation",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &cl_nolerp },
};

int hud_fps;
static cvar_t hud_fps_cvar = {
	.name = "hud_fps",
	.description =
		"display realtime frames per second",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &hud_fps },
};
int hud_time;
static cvar_t hud_time_cvar = {
	.name = "hud_time",
	.description =
		"display the current time",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &hud_time },
};

static int r_ambient;
static cvar_t r_ambient_cvar = {
	.name = "r_ambient",
	.description =
		"Determines the ambient lighting for a level",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &r_ambient },
};
static int r_drawflat;
static cvar_t r_drawflat_cvar = {
	.name = "r_drawflat",
	.description =
		"Toggles the drawing of textures",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &r_drawflat },
};

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
	// dedicated servers initialize the host but don't parse and set the
	// config.cfg cvars
	if (host_initialized && !isDedicated && cl_writecfg) {
		plitem_t   *config = PL_NewDictionary (0);
		Cvar_SaveConfig (config);
		IN_SaveConfig (config);

		const char *path = va (0, "%s/quakeforge.cfg", qfs_gamedir->dir.def);
		QFile      *f = QFS_WOpen (path, 0);

		if (!f) {
			Sys_Printf ("Couldn't write quakeforge.cfg.\n");
		} else {
			char       *cfg = PL_WritePropertyList (config);
			Qputs (f, cfg);
			free (cfg);
			Qclose (f);
		}
		PL_Free (config);
	}
}

int
CL_ReadConfiguration (const char *cfg_name)
{
	QFile      *cfg_file = QFS_FOpenFile (cfg_name);
	if (!cfg_file) {
		return 0;
	}
	size_t      len = Qfilesize (cfg_file);
	char       *cfg = malloc (len + 1);
	Qread (cfg_file, cfg, len);
	cfg[len] = 0;
	Qclose (cfg_file);

	plitem_t   *config = PL_GetPropertyList (cfg, 0);
	free (cfg);

	if (!config) {
		return 0;
	}

	Cvar_LoadConfig (config);
	IN_LoadConfig (config);

	PL_Free (config);
	return 1;
}

static void
CL_Shutdown (void *data)
{
	CL_WriteConfiguration ();
}

void
CL_ClearMemory (void)
{
	VID_ClearMemory ();
	SCR_SetFullscreen (0);

	cls.signon = 0;
	SZ_Clear (&cls.message);

	if (cl.viewstate.weapon_entity) {
		Scene_DestroyEntity (cl_world.scene, cl.viewstate.weapon_entity);
	}
	if (cl.players) {
		int         i;

		for (i = 0; i < cl.maxclients; i++)
			Info_Destroy (cl.players[i].userinfo);
	}
	// wipe the entire cl structure
	__auto_type cam = cl.viewstate.camera_transform;
	memset (&cl, 0, sizeof (cl));
	cl.viewstate.camera_transform = cam;

	CL_ClearTEnts ();

	SCR_NewScene (0);

	CL_ClearEnts ();
}

void
CL_InitCvars (void)
{
	VID_Init_Cvars ();
	IN_Init_Cvars ();
	Mod_Init_Cvars ();
	S_Init_Cvars ();

	CL_Demo_Init ();
	CL_Init_Input_Cvars ();
	Chase_Init_Cvars ();
	V_Init_Cvars ();

	Cvar_Register (&cl_name_cvar, 0, 0);
	Cvar_Register (&cl_color_cvar, 0, 0);
	Cvar_Register (&cl_writecfg_cvar, 0, 0);
	Cvar_Register (&cl_shownet_cvar, 0, 0);
	Cvar_Register (&cl_nolerp_cvar, 0, 0);
	Cvar_Register (&hud_fps_cvar, 0, 0);
	Cvar_MakeAlias ("show_fps", &hud_fps_cvar);
	Cvar_Register (&hud_time_cvar, 0, 0);

	//FIXME not hooked up (don't do anything), but should not work in
	//multi-player
	Cvar_Register (&r_ambient_cvar, 0, 0);
	Cvar_Register (&r_drawflat_cvar, 0, 0);
}

void
CL_ClearState (void)
{
	CL_ClearMemory ();
	if (!sv.active)
		Host_ClearMemory ();

	cl.viewstate.player_origin = (vec4f_t) {0, 0, 0, 1};
	cl.viewstate.chase = 1;
	cl.viewstate.chasestate = &cl.chasestate;
	cl.chasestate.viewstate = &cl.viewstate;
	cl.watervis = 1;
	SCR_SetFullscreen (0);
	r_data->lightstyle = cl.lightstyle;

	cl.viewstate.weapon_entity = Scene_CreateEntity (cl_world.scene);
	CL_Init_Entity (cl.viewstate.weapon_entity);
	r_data->view_model = cl.viewstate.weapon_entity;

	CL_TEnts_Precache ();
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
		cl.viewstate.cshifts[i].percent = 0;
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

	// if running a local server, shut it down
	if (cls.demoplayback)
		CL_StopPlayback ();
	else if (cls.state >= ca_connected) {
		if (cls.demorecording)
			CL_StopRecording ();

		Sys_MaskPrintf (SYS_dev, "Sending clc_disconnect\n");
		SZ_Clear (&cls.message);
		MSG_WriteByte (&cls.message, clc_disconnect);
		NET_SendUnreliableMessage (cls.netcon, &cls.message);
		SZ_Clear (&cls.message);
		NET_Close (cls.netcon);

		CL_SetState (ca_disconnected);
		if (sv.active)
			Host_ShutdownServer (false);
	}

	cl_world.scene->worldmodel = NULL;
	cl.intermission = 0;
	cl.viewstate.intermission = 0;
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
	if (net_is_dedicated)
		return;

	if (cls.demoplayback)
		return;

	CL_Disconnect ();

	cls.netcon = NET_Connect (host);
	if (!cls.netcon)
		Host_Error ("CL_Connect: connect failed\n");
	Sys_MaskPrintf (SYS_dev, "CL_EstablishConnection: connected to %s\n",
					host);

	cls.demonum = -1;					// not in the demo loop now
	CL_SetState (ca_connected);
}

/*
	CL_SignonReply

	An svc_signonnum has been received, perform a client side setup
*/
void
CL_SignonReply (void)
{
	Sys_MaskPrintf (SYS_dev, "CL_SignonReply: %i\n", cls.signon);

	switch (cls.signon) {
	case so_none:
		break;
	case so_prespawn:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, "prespawn");
		break;

	case so_spawn:
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message, va (0, "name \"%s\"\n",
										   cl_name));
		MSG_WriteByte (&cls.message, clc_stringcmd);
		MSG_WriteString (&cls.message,
						 va (0, "color %i %i\n", (cl_color) >> 4,
							 (cl_color) & 15));
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

	Cbuf_InsertText (host_cbuf, va (0, "playdemo %s\n",
									cls.demos[cls.demonum]));
	cls.demonum++;
}

static void
pointfile_f (void)
{
	CL_LoadPointFile (cl_world.scene->worldmodel);
}

static void
CL_PrintEntities_f (void)
{
	int         i;

	for (i = 0; i < cl.num_entities; i++) {
		entity_t   *ent = cl_entities[i];
		Sys_Printf ("%3i:", i);
		if (!ent || !ent->renderer.model) {
			Sys_Printf ("EMPTY\n");
			continue;
		}
		vec4f_t     org = Transform_GetWorldPosition (ent->transform);
		vec4f_t     rot = Transform_GetWorldRotation (ent->transform);
		Sys_Printf ("%s:%2i  "VEC4F_FMT" "VEC4F_FMT"\n",
					ent->renderer.model->path, ent->animation.frame,
					VEC4_EXP (org), VEC4_EXP (rot));
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
	TEntContext_t tentCtx = {
		cl.viewstate.player_origin,
		cl.viewentity
	};

	cl.oldtime = cl.time;
	cl.time += host_frametime;
	cl.viewstate.frametime = host_frametime;
	cl.viewstate.time = cl.time;

	do {
		ret = CL_GetMessage ();
		if (ret == -1)
			Host_Error ("CL_ReadFromServer: lost server connection");
		if (!ret)
			break;

		CL_ParseServerMessage ();
	} while (ret && cls.state >= ca_connected);

	if (cl_shownet)
		Sys_Printf ("\n");

	CL_RelinkEntities ();
	CL_UpdateTEnts (cl.time, &tentCtx);

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
		Sys_MaskPrintf (SYS_dev, "CL_WriteToServer: can't send\n");
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
	cl.viewstate.active = cls.state == ca_active;
	cl.viewstate.drift_enabled = !cls.demoplayback;
	Sys_MaskPrintf (SYS_net, "CL_SetState: %d -> %d\n", old_state, state);
	if (old_state != state) {
		if (old_state == ca_active && state != ca_disconnected) {
			// leaving active state
			S_AmbientOff ();
			SCR_NewScene (0);
		}
		switch (state) {
			case ca_disconnected:
				CL_ClearState ();
				cls.signon = so_none;
				cl.loading = false;
				VID_SetCaption ("Disconnected");
				break;
			case ca_connected:
				cls.signon = so_none;		// need all the signon messages
											// before playing
				cl.loading = true;
				IN_ClearStates ();
				VID_SetCaption ("Connected");
				break;
			case ca_active:
				// entering active state
				cl.loading = false;
				IN_ClearStates ();
				VID_SetCaption ("");
				S_AmbientOn ();
				break;
		}
		CL_UpdateScreen (cl.time);
	}
	host_in_game = 0;
	Con_SetState (state == ca_active ? con_inactive : con_fullscreen);
	if (state != old_state && state == ca_active) {
		CL_Input_Activate (host_in_game = !cls.demoplayback);
	}
}

static void
write_capture (tex_t *tex, void *data)
{
	QFile      *file = QFS_Open (va (0, "%s/qfmv%06d.png",
									 qfs_gamedir->dir.shots,
									 cls.demo_capture++), "wb");
	if (file) {
		WritePNG (file, tex);
		Qclose (file);
	}
	free (tex);
}

void
CL_PreFrame (void)
{
	IN_ProcessEvents ();

	GIB_Thread_Execute ();
	cmd_source = src_command;
	Cbuf_Execute_Stack (host_cbuf);

	CL_SendCmd ();
}

void
CL_Frame (void)
{
	static double time1 = 0, time2 = 0, time3 = 0;
	int         pass1, pass2, pass3;

	// fetch results from server
	if (cls.state >= ca_connected)
		CL_ReadFromServer ();

	// update video
	if (host_speeds)
		time1 = Sys_DoubleTime ();

	r_data->inhibit_viewmodel = (chase_active
								 || (cl.stats[STAT_ITEMS] & IT_INVISIBILITY)
								 || cl.stats[STAT_HEALTH] <= 0);
	r_data->frametime = host_frametime;

	CL_UpdateScreen (cl.time);

	if (host_speeds)
		time2 = Sys_DoubleTime ();

	// update audio
	if (cls.state == ca_active) {
		mleaf_t    *l;
		byte       *asl = 0;
		vec4f_t     origin;

		origin = Transform_GetWorldPosition (cl.viewstate.camera_transform);
		l = Mod_PointInLeaf (origin, cl_world.scene->worldmodel);
		if (l)
			asl = l->ambient_sound_level;
		S_Update (cl.viewstate.camera_transform, asl);
		R_DecayLights (host_frametime);
	} else
		S_Update (0, 0);

	CDAudio_Update ();

	if (host_speeds) {
		pass1 = (time1 - time3) * 1000;
		time3 = Sys_DoubleTime ();
		pass2 = (time2 - time1) * 1000;
		pass3 = (time3 - time2) * 1000;
		Sys_Printf ("%3i tot %3i server %3i gfx %3i snd\n",
					pass1 + pass2 + pass3, pass1, pass2, pass3);
	}

	if (cls.demo_capture) {
		r_funcs->capture_screen (write_capture, 0);
	}
	fps_count++;
}

static void
Force_CenterView_f (void)
{
	cl.viewstate.player_angles[PITCH] = 0;
}

void
CL_Init (cbuf_t *cbuf)
{
	byte       *basepal, *colormap;

	basepal = (byte *) QFS_LoadHunkFile (QFS_FOpenFile ("gfx/palette.lmp"));
	if (!basepal)
		Sys_Error ("Couldn't load gfx/palette.lmp");
	colormap = (byte *) QFS_LoadHunkFile (QFS_FOpenFile ("gfx/colormap.lmp"));
	if (!colormap)
		Sys_Error ("Couldn't load gfx/colormap.lmp");

	Host_OnServerSpawn (CL_ClearMemory);

	W_LoadWadFile ("gfx.wad");
	VID_Init (basepal, colormap);
	IN_Init ();
	GIB_Key_Init ();
	R_Init ();
	r_data->lightstyle = cl.lightstyle;
	S_Init (&cl.viewentity, &host_frametime);

	PI_RegisterPlugins (client_plugin_list);
	Con_Init ("client");

	CDAudio_Init ();

	Sbar_Init ();

	CL_Init_Input (cbuf);
	CL_Particles_Init ();
	CL_TEnts_Init ();
	CL_World_Init ();
	CL_ClearState ();

	V_Init (&cl.viewstate);

	Cmd_AddCommand ("pointfile", pointfile_f,
					"Load a pointfile to determine map leaks.");
	Cmd_AddCommand ("entities", CL_PrintEntities_f, "No Description");
	Cmd_AddCommand ("disconnect", CL_Disconnect_f, "No Description");
	Cmd_AddCommand ("maplist", Con_Maplist_f, "List available maps");
	Cmd_AddCommand ("skyboxlist", Con_Skyboxlist_f, "List skyboxes available");
	Cmd_AddCommand ("demolist", Con_Demolist_DEM_f, "List available demos");
	Cmd_AddCommand ("force_centerview", Force_CenterView_f, "force the view "
					"to be level");

	Sys_RegisterShutdown (CL_Shutdown, 0);

	SZ_Alloc (&cls.message, 1024);
	CL_SetState (ca_disconnected);
}
