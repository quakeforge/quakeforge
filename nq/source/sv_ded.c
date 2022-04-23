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

#include "QF/cdaudio.h"
#include "QF/cvar.h"
#include "QF/keys.h"
#include "QF/plugin.h"
#include "QF/screen.h"

#include "QF/plugin/vid_render.h"

#include "client/world.h"

#include "nq/include/host.h"
#include "nq/include/server.h"

client_state_t cl;
client_static_t cls;

worldscene_t cl_world;

char *cl_name;
int cl_writecfg;
float demo_speed;
int chase_active;

int         fps_count;
int         viewentity;

vid_render_data_t *r_data;
vid_render_funcs_t *r_funcs;

void
GIB_Key_Init (void)
{
}

void
CL_SetState (cactive_t state)
{
}

void
CL_UpdateScreen (double realtime)
{
}

void
CL_ClearMemory (void)
{
}

void
CL_Cmd_ForwardToServer (void)
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
CL_Init (struct cbuf_s *cbuf)
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

__attribute__((const)) int
CL_ReadConfiguration (const char *cfg_name)
{
	return 0;
}

__attribute__((const)) int
CL_ReadFromServer (void)
{
	return 0;
}

void
CL_SendCmd (void)
{
}

void
CL_StopPlayback (void)
{
}

void
IN_ProcessEvents (void)
{
}

void
S_Update (struct transform_s *ere, const byte *ambient_sound_level)
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
__attribute__((const)) plugin_t *
console_client_PluginInfo (void)
{
	return 0;
}

void
R_DecayLights (double frametime)
{
}
