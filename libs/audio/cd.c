/*
	cd_plugin.c

	cd plugin wrapper

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

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

#include "QF/cdaudio.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/plugin.h"
#include "QF/qtypes.h"

cvar_t                *cd_plugin;
plugin_t              *cdmodule = NULL;


int
CDAudio_Init (void)
{
	cd_plugin = Cvar_Get ("cd_plugin", "null", CVAR_ARCHIVE, NULL,
						  "CD Plugin to use");
	cdmodule = PI_LoadPlugin ("cd", cd_plugin->string);
	if (!cdmodule) {
		Con_Printf ("Loading of cd module: %s failed!\n", cd_plugin->string);
		return -1;
	} else {
		cdmodule->functions->general->p_Init ();
		return 0; // FIXME: Assumes success
	}
}


void
CDAudio_Pause (void)
{
	if (cdmodule)
		cdmodule->functions->cd->pCDAudio_Pause ();
}


void
CDAudio_Play (byte track, qboolean looping)
{
	if (cdmodule)
		cdmodule->functions->cd->pCDAudio_Play (track, looping);
}


void
CDAudio_Resume (void)
{
	if (cdmodule)
		cdmodule->functions->cd->pCDAudio_Resume ();
}


void
CDAudio_Shutdown (void)
{
	if (cdmodule)
		cdmodule->functions->general->p_Shutdown ();
}


void
CDAudio_Update (void)
{
	if (cdmodule)
		cdmodule->functions->cd->pCDAudio_Update ();
}


static void
CD_f (void)
{
	if (cdmodule)
		cdmodule->functions->cd->pCD_f ();
}
