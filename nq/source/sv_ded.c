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
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdarg.h>

#include "QF/cvar.h"
#include "QF/keys.h"
#include "QF/sys.h"

#include "client.h"
#include "host.h"

client_state_t cl;
client_static_t cls;

keydest_t   key_dest = key_game;

float       scr_centertime_off;

cvar_t     *cl_name;
cvar_t     *cl_writecfg;
cvar_t     *chase_active;

int         fps_count;

qboolean    scr_disabled_for_loading;

byte       *vid_colormap;

vec3_t      vpn, vright, vup;

qboolean    r_active;
qboolean    r_force_fullscreen;
double      r_frametime;
qboolean    r_inhibit_viewmodel;
vec3_t      r_origin;
qboolean    r_paused;
entity_t   *r_view_model;

void
CL_UpdateScreen (double realtime)
{
}

void
Cmd_ForwardToServer (void)
{
}

void
CDAudio_Init (void)
{
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
CL_InitCvars (void)
{
}

void
CL_NextDemo (void)
{
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
IN_Commands (void)
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
IN_SendKeyEvents (void)
{
}

void
IN_Shutdown (void)
{
}

void
Key_Init (void)
{
}

void
Key_Init_Cvars (void)
{
}

void
Key_WriteBindings (VFile *f)
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
S_Init (void)
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
S_Update (vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up)
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
