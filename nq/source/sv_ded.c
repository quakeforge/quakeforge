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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdarg.h>

#include "QF/cvar.h"
#include "QF/host.h"
#include "QF/keys.h"

#include "client.h"

int         m_return_state;
qboolean    m_return_onerror;
char        m_return_reason[32];
enum { m_none, m_main, m_singleplayer, m_load, m_save, m_multiplayer, m_setup,
	   m_net, m_options, m_video, m_keys, m_help, m_quit, m_serialconfig,
	   m_modemconfig, m_lanconfig, m_gameoptions, m_search, m_slist
} m_state;

keydest_t   key_dest;
client_static_t cls;
client_state_t cl;
vec3_t      vright, vup, vleft, vpn;
float       scr_centertime_off;


void
Con_Printf (char *fmt, ...)
{
	va_list     args;

	va_start (args, fmt);
	vprintf (fmt, args);
	va_end (args);
}

void
Con_DPrintf (char *fmt, ...)
{
	va_list     args;

	if (!developer->int_val)
		return;

	va_start (args, fmt);
	vprintf (fmt, args);
	va_end (args);
}

void
SCR_UpdateScreen (double realtime)
{
}

void
SCR_BeginLoadingPlaque (void)
{
}

void
SCR_EndLoadingPlaque (void)
{
}

void
Draw_BeginDisc (void)
{
}

void
Draw_EndDisc (void)
{
}
