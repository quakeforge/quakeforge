/*
	sv_ded.c

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#include <stdio.h>
#include <stdarg.h>

#include "QF/cdaudio.h"
#include "QF/cmd.h"
#include "QF/csqc.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/keys.h"
#include "QF/msg.h"
#include "QF/plugin.h"
#include "QF/screen.h"
#include "QF/sys.h"

#include "chase.h"
#include "client.h"
#include "compat.h"
#include "host.h"
#include "r_dynamic.h"
#include "sbar.h"
#include "server.h"
#include "clview.h"	//FIXME

client_state_t cl;
client_static_t cls;

keydest_t   key_dest = key_game;

cvar_t     *cl_name;
cvar_t     *cl_writecfg;
cvar_t     *demo_speed;
cvar_t     *chase_active;

int         fps_count;
int         viewentity;

byte       *vid_colormap;

vec3_t      vpn, vright, vup;

qboolean    r_active;
qboolean    r_force_fullscreen;
double      r_frametime;
qboolean    r_inhibit_viewmodel;
vec3_t      r_origin;
qboolean    r_paused;
entity_t   *r_view_model;

cvar_t *cl_rollangle;
cvar_t *cl_rollspeed;

void
CL_UpdateScreen (double realtime)
{
}

void
CL_Cmd_ForwardToServer (void)
{
}

int
CDAudio_Init (void)
{
	return 0;
}

void
CDAudio_Shutdown (void)
{
}

void
CDAudio_Update (void)
{
}

void
CL_Disconnect (void)
{
}

void
CL_Disconnect_f (void)
{
}

void
CL_EstablishConnection (const char *host)
{
}

void
CL_Init (void)
{
}

void
CL_Init_Entity (entity_t *ent)
{
}

static void
color_f (void)
{
	int         top, bottom;
	char        playercolor;

	if (Cmd_Argc () <= 1) {
		Sys_Printf ("\"color\" is \"%d %d\"\n", (host_client->colors) >> 4,
					(host_client->colors) & 0x0f);
		Sys_Printf ("color <0-13> [0-13]\n");
		return;
	}

	if (Cmd_Argc () == 2)
		top = bottom = atoi (Cmd_Argv (1));
	else {
		top = atoi (Cmd_Argv (1));
		bottom = atoi (Cmd_Argv (2));
	}

	top = min (top & 15, 13);
	bottom = min (bottom & 15, 13);

	playercolor = top * 16 + bottom;

	host_client->colors = playercolor;
	SVfloat (host_client->edict, team) = bottom + 1;

	// send notification to all clients
	MSG_WriteByte (&sv.reliable_datagram, svc_updatecolors);
	MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
	MSG_WriteByte (&sv.reliable_datagram, host_client->colors);
}

void
CL_InitCvars (void)
{
	cl_name = Cvar_Get ("_cl_name", "player", CVAR_ARCHIVE, NULL, "");
	cl_writecfg = Cvar_Get ("cl_writecfg", "1", CVAR_NONE, NULL, "");
	cl_rollangle = Cvar_Get ("cl_rollangle", "2.0", CVAR_NONE, NULL,
							 "How much your screen tilts when strafing");
	cl_rollspeed = Cvar_Get ("cl_rollspeed", "200", CVAR_NONE, NULL,
							 "How quickly you straighten out after strafing");

	Cmd_AddCommand ("color", color_f, "The pant and shirt color (color shirt "
					"pants) Note that if only shirt color is given, pants "
					"will match");

}

void
CL_NextDemo (void)
{
}

void
CL_Demo_Init (void)
{
	demo_speed = Cvar_Get ("demo_speed", "1", CVAR_NONE, NULL, "");
}

int
CL_ReadFromServer (void)
{
	return 0;
}

void
CL_SendCmd (void)
{
}

void
CL_SetState (cactive_t state)
{
}

void
CL_StopPlayback (void)
{
}

void
Chase_Init_Cvars (void)
{
	chase_active = Cvar_Get ("chase_active", "0", CVAR_NONE, NULL, "None");
}

void
D_FlushCaches (void)
{
}

void
Draw_Init (void)
{
}

void
Host_Skin_Init (void)
{
}

void
Host_Skin_Init_Cvars (void)
{
}

void
IN_Init (void)
{
}

void
IN_Init_Cvars (void)
{
}

void
IN_ProcessEvents (void)
{
}

void
IN_Shutdown (void)
{
}

void
Key_Init (struct cbuf_s *cb)
{
}

void
Key_Init_Cvars (void)
{
}

void
Key_WriteBindings (QFile *f)
{
}

void
R_DecayLights (double frametime)
{
}

void
R_Init (void)
{
}

void
R_Init_Cvars (void)
{
}

void
R_Particles_Init_Cvars (void)
{
}

void
SCR_Init (void)
{
}

void
S_Init (struct model_s **worldmodel, int *viewentity, double *host_frametime)
{
}

void
S_Init_Cvars (void)
{
}

void
S_Shutdown (void)
{
}

void
S_Update (const vec3_t origin, const vec3_t v_forward, const vec3_t v_right,
		  const vec3_t v_up)
{
}

void
Sbar_Init (void)
{
}

void
VID_Init (unsigned char *palette)
{
}

void
VID_Init_Cvars (void)
{
}

void
VID_Shutdown (void)
{
}

void
V_Init (void)
{
}

void
V_Init_Cvars (void)
{
}

void
S_BlockSound (void)
{
}

void
S_UnblockSound (void)
{
}

plugin_t *console_client_PluginInfo (void);
plugin_t *
console_client_PluginInfo (void)
{
	return 0;
}   
