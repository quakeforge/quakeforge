/*
	info.h

	(server|local)info definitions and prototypes

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

#ifndef _INFO_H
#define _INFO_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h> // for size_t. sys/types.h SHOULD be used, but can't :(bc)
#include <QF/qtypes.h>

#define	MAX_INFO_STRING			512
#define	MAX_SERVERINFO_STRING	512
#define	MAX_LOCALINFO_STRING	32768

char *Info_ValueForKey (char *s, char *key);
void Info_RemoveKey (char *s, char *key);
void Info_RemovePrefixedKeys (char *start, char prefix);
void Info_SetValueForKey (char *s, char *key, char *value, size_t maxsize);
void Info_SetValueForStarKey (char *s, char *key, char *value, size_t maxsize);
void Info_Print (char *s);
qboolean Info_Validate (char *s);

#endif	// _INFO_H
