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

*/

#ifndef _INFO_H
#define _INFO_H

/** \defgroup info Info Keys
	\ingroup utils
*/
///@{

#include <stdlib.h> // for size_t. sys/types.h SHOULD be used, but can't :(bc)
#include <QF/qtypes.h>

#define	MAX_INFO_STRING			512
#define	MAX_SERVERINFO_STRING	512
#define	MAX_LOCALINFO_STRING	32768

typedef struct info_s info_t;

typedef struct info_key_s {
	const char			*key;
	const char			*value;
} info_key_t;

qboolean Info_FilterForKey (const char *key, const char **filter_list) __attribute__((pure));

void Info_Print (info_t *info);
int Info_CurrentSize (info_t *info) __attribute__((pure));
info_key_t *Info_Key (info_t *info, const char *key);
info_key_t **Info_KeyList (info_t *info);
void Info_RemoveKey (info_t *info, const char *key);
int Info_SetValueForKey (info_t *info, const char *key, const char *value, int flags);
int Info_SetValueForStarKey (info_t *info, const char *key, const char *value, int flags);
const char *Info_ValueForKey (info_t *info, const char *key);

info_t *Info_ParseString (const char *s, int maxsize, int flags);
void Info_Destroy (info_t *info);
char *Info_MakeString (info_t *info, int (*filter)(const char *));
void Info_AddKeys (info_t *info, info_t *keys);

///@}

#endif	// _INFO_H
