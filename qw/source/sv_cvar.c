/*
	sv_cvar.c

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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/cvar.h"
#include "server.h"

void        SV_SendServerInfoChange (const char *key, const char *value);

extern cvar_t *sv_highchars;

/*

	Cvar_Info

	Sets a given cvar (key,value) into svs.info (serverinfo)
	high char filtering is performed according to sv_highchars.value

*/

void
Cvar_Info (cvar_t *var)
{
	if (var->flags & CVAR_SERVERINFO) {
		unsigned char info[1024], *p;
		const unsigned char *c;

		if (!sv_highchars || !sv_highchars->int_val) {
			for (p = info, c = var->string;
				 *c && (p - info < sizeof (info) - 1);) {
				if ((*c & 0x7f) >= 32)
					*p++ = *c & 0x7f;
				c++;
			}
			*p = 0;
			Info_SetValueForKey (svs.info, var->name, info,
								 MAX_SERVERINFO_STRING);
		} else
			Info_SetValueForKey (svs.info, var->name, var->string,
								 MAX_SERVERINFO_STRING);

		SV_SendServerInfoChange (var->name, var->string);
//      SV_BroadcastCommand ("fullserverinfo \"%s\"\n", svs.info);
	}
}
