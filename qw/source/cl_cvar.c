/*
	cl_cvar.c

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
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include "QF/cvar.h"
#include "QF/msg.h"
#include "QF/va.h"

#include "compat.h"

#include "qw/include/client.h"

void
Cvar_Info (cvar_t *var)
{
	if (var->flags & CVAR_USERINFO) {
		Info_SetValueForKey (cls.userinfo, var->name, var->string,
							 ((!strequal(var->name, "name"))
							  |(strequal(var->name, "team") << 1)));
		if (cls.state >= ca_connected && !cls.demoplayback) {
			MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
			MSG_WriteString (&cls.netchan.message,
							 va ("setinfo \"%s\" \"%s\"\n", var->name,
								 var->string));
		}
	}
}
