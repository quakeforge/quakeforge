/*
	cvar.c

	dynamic variable tracking

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

*/
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "QF/cvar.h"

#include "nq/include/server.h"

void
Cvar_Info (void *data, const cvar_t *cvar)
{
	if (cvar->flags & CVAR_SERVERINFO) {
		if (sv.active)
			SV_BroadcastPrintf ("\"%s\" changed to \"%s\"\n",
								cvar->name, Cvar_VarString (cvar));
	}
}
