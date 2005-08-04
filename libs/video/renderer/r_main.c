/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2001 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

#include "QF/cvar.h"
#include "QF/qtypes.h"
#include "QF/render.h"

#include "r_cvar.h"

qboolean    r_inhibit_viewmodel;
qboolean    r_force_fullscreen;
qboolean    r_paused;
double      r_realtime;
double      r_frametime;
entity_t   *r_view_model;
entity_t   *r_player_entity;
float       r_time1;
int         r_lineadj;
qboolean    r_active;

#define U __attribute__ ((used))
static U void (*const r_progs_init)(struct progs_s *) = R_Progs_Init;
#undef U

byte        color_white[4] = { 255, 255, 255, 0 };	// alpha will be explicitly set
byte        color_black[4] = { 0, 0, 0, 0 };		// alpha will be explicitly set
