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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/msg.h"
#include "QF/screen.h"
#include "QF/va.h"

#include "cl_parse.h"
#include "cl_skin.h"
#include "client.h"
#include "compat.h"
#include "host.h"

cvar_t     *noskins; //XXX FIXME
cvar_t     *skin;
cvar_t     *topcolor;
cvar_t     *bottomcolor;


void
Skin_NextDownload (void)
{
	int         i;
	player_info_t *sc;

	if (cls.state == ca_disconnected)
		return;

	if (cls.downloadnumber == 0) {
		Con_Printf ("Checking skins...\n");
		CL_UpdateScreen (realtime);
	}
	cls.downloadtype = dl_skin;

	for (; cls.downloadnumber != MAX_CLIENTS; cls.downloadnumber++) {
		sc = &cl.players[cls.downloadnumber];
		if (!sc->name[0])
			continue;
		Skin_Find (sc);
		if (noskins->int_val) //XXX FIXME
			continue;
		if (!CL_CheckOrDownloadFile (va ("skins/%s.pcx", sc->skin->name)))
			return;						// started a download
	}

	cls.downloadtype = dl_none;

	// now load them in for real
	for (i = 0; i < MAX_CLIENTS; i++) {
		sc = &cl.players[i];
		if (!sc->name[0])
			continue;
		Skin_Find (sc);
		Skin_Cache (sc->skin);
		sc->skin = NULL;
	}

	if (cls.state != ca_active) {
		if (!cls.demoplayback) {
			// get next signon phase
			MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
			MSG_WriteString (&cls.netchan.message, va ("begin %i",
												   cl.servercount));
			Cache_Report ();				// print remaining memory
		}
		CL_SetState (ca_active);
	}
}

/*
	CL_Skins_f

	Refind all skins, downloading if needed.
*/
void
CL_Skins_f (void)
{
	Skin_Flush ();

	if (cls.state == ca_disconnected)
		return;

	cls.downloadnumber = 0;
	cls.downloadtype = dl_skin;
	Skin_NextDownload ();
}

/*
	CL_AllSkins_f

	Sets all skins to one specific one
*/
void
CL_AllSkins_f (void)
{
	if (Cmd_Argc () == 2) {
		strcpy (allskins, Cmd_Argv (1));
	} else if (Cmd_Argc () == 1) {
		Con_Printf ("clearing allskins\n");
		allskins[0] = 0;
	} else {
		Con_Printf ("Usage: allskins [name]\n");
		return;
	}
	CL_Skins_f ();
}

static void
CL_Color_f (void)
{
	// just for quake compatability...
	char        num[16];
	int         top, bottom;

	if (Cmd_Argc () == 1) {
		Con_Printf ("\"color\" is \"%s %s\"\n",
					Info_ValueForKey (cls.userinfo, "topcolor"),
					Info_ValueForKey (cls.userinfo, "bottomcolor"));
		Con_Printf ("color <0-13> [0-13]\n");
		return;
	}

	if (Cmd_Argc () == 2)
		top = bottom = atoi (Cmd_Argv (1));
	else {
		top = atoi (Cmd_Argv (1));
		bottom = atoi (Cmd_Argv (2));
	}

	top &= 15;
	if (top > 13)
		top = 13;
	bottom &= 15;
	if (bottom > 13)
		bottom = 13;

	snprintf (num, sizeof (num), "%i", top);
	Cvar_Set (topcolor, num);
	snprintf (num, sizeof (num), "%i", bottom);
	Cvar_Set (bottomcolor, num);
}

void
CL_Skin_Init (void)
{
	Skin_Init ();
	Cmd_AddCommand ("skins", CL_Skins_f, "Download all skins that are "
					"currently in use");
	Cmd_AddCommand ("allskins", CL_AllSkins_f, "Force all player skins to "
					"one skin");
	Cmd_AddCommand ("color", CL_Color_f, "The pant and shirt color (color "
					"shirt pants) Note that if only shirt color is given, "
					"pants will match");
}

static void
skin_f (cvar_t *var)
{
	char       *s = Hunk_TempAlloc (strlen (var->string) + 1);
	QFS_StripExtension (var->string, s);
	Cvar_Set (var, s);
	Cvar_Info (var);
}

void
CL_Skin_Init_Cvars (void)
{
	Skin_Init_Cvars ();
	noskins = Cvar_Get ("noskins", "0", CVAR_ARCHIVE, NULL, //XXX FIXME
						"set to 1 to not download new skins");
	skin = Cvar_Get ("skin", "", CVAR_ARCHIVE | CVAR_USERINFO, skin_f,
					 "Players skin");
	topcolor = Cvar_Get ("topcolor", "0", CVAR_ARCHIVE | CVAR_USERINFO,
						 Cvar_Info, "Players color on top");
	bottomcolor = Cvar_Get ("bottomcolor", "0", CVAR_ARCHIVE | CVAR_USERINFO,
							Cvar_Info, "Players color on bottom");
}

void
CL_NewTranslation (int slot, skin_t *skin)
{
	player_info_t *player;

	if (slot > MAX_CLIENTS)
		Host_Error ("CL_NewTranslation: slot > MAX_CLIENTS");

	player = &cl.players[slot];
	if (!player->name[0])
		return;

	if (player->skin && !strequal (player->skinname->value, player->skin->name))
		player->skin = NULL;

	if (player->_topcolor != player->topcolor ||
		player->_bottomcolor != player->bottomcolor || !player->skin) {
		player->_topcolor = player->topcolor;
		player->_bottomcolor = player->bottomcolor;
		Skin_Set_Translate (player->topcolor, player->bottomcolor,
							player->translations);
		if (!player->skin)
			Skin_Find (player);
		memcpy (skin, player->skin, sizeof (*skin));
		skin->texture = skin_textures + slot;		// FIXME
		skin->data.texels = Skin_Cache (player->skin);
				// FIXME: breaks cache ownership
		Skin_Do_Translation (player->skin, slot, skin);
	} else {
		memcpy (skin, player->skin, sizeof (*skin));
		skin->texture = skin_textures + slot;		// FIXME
		skin->data.texels = Skin_Cache (player->skin);
				// FIXME: breaks cache ownership
	}
}
