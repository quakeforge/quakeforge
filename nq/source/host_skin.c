/*
	skin.c

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((unused)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cmd.h"
#include "compat.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/msg.h"
#include "QF/screen.h"
#include "QF/skin.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "client.h"
#include "host.h"
#include "server.h"

cvar_t     *noskins; //XXX FIXME
cvar_t     *skin;
cvar_t     *topcolor;
cvar_t     *bottomcolor;
cvar_t     *cl_color;



static void
Host_Color_f (void)
{
	int         top, bottom;
	char        playercolor;

	if (Cmd_Argc () == 1) {
		Con_Printf ("\"color\" is \"%d %d\"\n", (cl_color->int_val) >> 4,
					(cl_color->int_val) & 0x0f);
		Con_Printf ("color <0-13> [0-13]\n");
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

	if (cmd_source == src_command) {
		Cvar_SetValue (cl_color, playercolor);
		if (cls.state == ca_connected)
			CL_Cmd_ForwardToServer ();
		return;
	}

	host_client->colors = playercolor;
	SVfloat (host_client->edict, team) = bottom + 1;

	// send notification to all clients
	MSG_WriteByte (&sv.reliable_datagram, svc_updatecolors);
	MSG_WriteByte (&sv.reliable_datagram, host_client - svs.clients);
	MSG_WriteByte (&sv.reliable_datagram, host_client->colors);
}


void
Host_Skin_Init (void)
{
	Skin_Init ();
	Cmd_AddCommand ("color", Host_Color_f, "The pant and shirt color (color "
					"shirt pants) Note that if only shirt color is given, "
					"pants will match");
}


void
Host_Skin_Init_Cvars (void)
{
	Skin_Init_Cvars ();
	cl_color = Cvar_Get ("_cl_color", "0", CVAR_ARCHIVE, NULL, "Player color");
}


void
CL_NewTranslation (int slot, skin_t *skin)
{
	scoreboard_t *player;
	int         top, bottom;
	byte       *dest;
	model_t    *model;
	cl_entity_state_t *state;
	int         skinnum;

	if (slot > cl.maxclients)
		Sys_Error ("Host_NewTranslation: slot > cl.maxclients");

	player = &cl.scores[slot];
	dest = player->translations;
	top = (player->colors & 0xf0) >> 4;
	bottom = player->colors & 15;
	state = &cl_baselines[1 + slot];
	model = state->ent->model;
	skinnum = state->ent->skinnum;

	memset (skin, 0, sizeof (*skin)); //XXX external skins not yet supported

	if (!model)
		return;

	skin->texture = skin_textures + slot; //FIXME
	skin->data.texels = 0; //FIXME

	if (player->colors == player->_colors
		&& model == state->model
		&& skinnum == state->skinnum)
		return;

	player->_colors = player->colors;
	state->model = model;
	state->skinnum = skinnum;

	Skin_Set_Translate (top, bottom, dest);
	Skin_Do_Translation_Model (model, skinnum, slot, skin);
}
