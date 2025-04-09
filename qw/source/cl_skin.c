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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/msg.h"
#include "QF/screen.h"
#include "QF/skin.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "client/screen.h"
#include "client/world.h"
#include "compat.h"

#include "qw/include/cl_parse.h"
#include "qw/include/cl_skin.h"
#include "qw/include/client.h"
#include "qw/include/host.h"

int noskins;
static cvar_t noskins_cvar = {
	.name = "noskins",
	.description =
		"set to 1 to not download new skins",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &noskins },
};
char *skin;
static cvar_t skin_cvar = {
	.name = "skin",
	.description =
		"Players skin",
	.default_value = "",
	.flags = CVAR_ARCHIVE | CVAR_USERINFO,
	.value = { .type = 0, .value = &skin },
};
int topcolor;
static cvar_t topcolor_cvar = {
	.name = "topcolor",
	.description =
		"Players color on top",
	.default_value = "0",
	.flags = CVAR_ARCHIVE | CVAR_USERINFO,
	.value = { .type = &cexpr_int, .value = &topcolor },
};
int bottomcolor;
static cvar_t bottomcolor_cvar = {
	.name = "bottomcolor",
	.description =
		"Players color on bottom",
	.default_value = "0",
	.flags = CVAR_ARCHIVE | CVAR_USERINFO,
	.value = { .type = &cexpr_int, .value = &bottomcolor },
};


void
Skin_NextDownload (void)
{
	int         i;
	player_info_t *sc;

	if (cls.state == ca_disconnected)
		return;

	if (cls.downloadnumber == 0) {
		Sys_Printf ("Checking skins...\n");
		cl.viewstate.time = realtime;
		cl.viewstate.realtime = realtime;
		cl_realtime = realtime;
		cl_frametime = host_frametime;
		CL_UpdateScreen (&cl.viewstate);
	}
	cls.downloadtype = dl_skin;

	for (; cls.downloadnumber != MAX_CLIENTS; cls.downloadnumber++) {
		sc = &cl.players[cls.downloadnumber];
		if (!sc->name || !sc->name->value[0])
			continue;
		//XXX Skin_Find (sc);
		if (noskins) //XXX FIXME
			continue;
		//XXX if (!CL_CheckOrDownloadFile (va ("skins/%s.pcx",
		//									   sc->skin->name)))
			//XXX return;						// started a download
	}

	cls.downloadtype = dl_none;

	// now load them in for real
	for (i = 0; i < MAX_CLIENTS; i++) {
		sc = &cl.players[i];
		if (!sc->name || !sc->name->value[0])
			continue;
		//XXX Skin_Find (sc);
		//XXX Skin_Cache (sc->skin);
		//XXX sc->skin = NULL;
	}

	if (cls.state != ca_active) {
		if (!cls.demoplayback) {
			// get next signon phase
			MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
			MSG_WriteString (&cls.netchan.message,
							 va ("begin %i", cl.servercount));
			Cache_Report ();				// print remaining memory
		}
		CL_SetState (ca_active);
	}
}

/*
	CL_Skins_f

	Refind all skins, downloading if needed.
*/
static void
CL_Skins_f (void)
{
	//XXX Skin_Flush ();

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
static void
CL_AllSkins_f (void)
{
	if (Cmd_Argc () == 2) {
		//XXX strcpy (allskins, Cmd_Argv (1));
	} else if (Cmd_Argc () == 1) {
		Sys_Printf ("clearing allskins\n");
		//XXX allskins[0] = 0;
	} else {
		Sys_Printf ("Usage: allskins [name]\n");
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
		Sys_Printf ("%c\"color\" is \"%s %s\"\n", 3,
					Info_ValueForKey (cls.userinfo, "topcolor"),
					Info_ValueForKey (cls.userinfo, "bottomcolor"));
		Sys_Printf ("color <0-13> [0-13]\n");
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
	Cvar_Set ("topcolor", num);
	snprintf (num, sizeof (num), "%i", bottom);
	Cvar_Set ("bottomcolor", num);
}

static void
skin_f (void *data, const cvar_t *cvar)
{
	char       *s = Hunk_TempAlloc (0, strlen (skin) + 1);
	QFS_StripExtension (skin, s);
	free (skin);	// cvar allocated one FIXME do in validator?
	skin = strdup (s);
	Cvar_Info (0, cvar);
}

void
CL_Skin_Init (void)
{
	Cmd_AddCommand ("skins", CL_Skins_f, "Download all skins that are "
					"currently in use");
	Cmd_AddCommand ("allskins", CL_AllSkins_f, "Force all player skins to "
					"one skin");
	Cmd_AddCommand ("color", CL_Color_f, "The pant and shirt color (color "
					"shirt pants) Note that if only shirt color is given, "
					"pants will match");

	Cvar_Register (&noskins_cvar, 0, 0);
	Cvar_Register (&skin_cvar, skin_f, 0);
	Cvar_Register (&topcolor_cvar, Cvar_Info, &topcolor);
	Cvar_Register (&bottomcolor_cvar, Cvar_Info, &bottomcolor);
}
