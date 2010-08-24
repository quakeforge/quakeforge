/*
	cl_screen.c

	master for refresh, status bar, console, chat, notify, etc

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

#include <time.h>

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/dstring.h"
#include "QF/image.h"
#include "QF/msg.h"
#include "QF/pcx.h"
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "cl_parse.h"
#include "client.h"
#include "compat.h"
#include "r_local.h"
#include "r_cvar.h"
#include "sbar.h"


static void
SCR_DrawNet (void)
{
	if (cls.netchan.outgoing_sequence - cls.netchan.incoming_acknowledged <
		UPDATE_BACKUP - 1)
		return;
	if (cls.demoplayback)
		return;

	Draw_Pic (scr_vrect.x + 64, scr_vrect.y, scr_net);
}

static SCR_Func scr_funcs_normal[] = {
	Draw_Crosshair,
	SCR_DrawRam,
	SCR_DrawNet,
	CL_NetGraph,
	SCR_DrawTurtle,
	SCR_DrawPause,
	Sbar_DrawCenterPrint,
	Sbar_Draw,
	Con_DrawConsole,
	0
};

static SCR_Func scr_funcs_intermission[] = {
	Sbar_IntermissionOverlay,
	Con_DrawConsole,
	0
};

static SCR_Func scr_funcs_finale[] = {
	Sbar_FinaleOverlay,
	Con_DrawConsole,
	0,
};

static SCR_Func *scr_funcs[] = {
	scr_funcs_normal,
	scr_funcs_intermission,
	scr_funcs_finale,
};

void
CL_UpdateScreen (double realtime)
{
	unsigned    index = cl.intermission;

	if (index >= sizeof (scr_funcs) / sizeof (scr_funcs[0]))
		index = 0;

	// don't allow cheats in multiplayer
	if (r_active) {
		if (cl.watervis)
			cl_wateralpha = r_wateralpha->value;
		else
			cl_wateralpha = 1.0;
	}

	V_PrepBlend ();
	SCR_UpdateScreen (realtime, scr_funcs[index]);
}

void
CL_RSShot_f (void)
{
	char        st[128];
	int         pcx_len;
	pcx_t      *pcx = 0;
	tex_t      *tex = 0;
	time_t      now;

	if (CL_IsUploading ())
		return;							// already one pending
	if (cls.state < ca_onserver)
		return;							// must be connected

	Sys_Printf ("Remote screen shot requested.\n");

	tex = SCR_ScreenShot (RSSHOT_WIDTH, RSSHOT_HEIGHT);

	if (tex) {
		time (&now);
		strcpy (st, ctime (&now));
		st[strlen (st) - 1] = 0;
		SCR_DrawStringToSnap (st, tex, tex->width - strlen (st) * 8,
							  tex->height - 1);

		strncpy (st, cls.servername->str, sizeof (st));
		st[sizeof (st) - 1] = 0;
		SCR_DrawStringToSnap (st, tex, tex->width - strlen (st) * 8,
							  tex->height - 11);

		strncpy (st, cl_name->string, sizeof (st));
		st[sizeof (st) - 1] = 0;
		SCR_DrawStringToSnap (st, tex, tex->width - strlen (st) * 8,
							  tex->height - 21);

		pcx = EncodePCX (tex->data, tex->width, tex->height, tex->width,
						 vid.basepal, true, &pcx_len);
		free (tex);
	}
	if (pcx) {
		CL_StartUpload ((void *)pcx, pcx_len);
		Sys_Printf ("Wrote %s\n", "rss.pcx");
		Sys_Printf ("Sending shot to server...\n");
	}
}
