/*
	qtypes.h

	(description)

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

#ifndef _QTYPES_H
#define _QTYPES_H

#include <stdio.h>
#include <sys/types.h>

#include "qdefs.h"
#include "compat.h"

#define MAX_QPATH	64

#ifndef _DEF_BYTE_
# define _DEF_BYTE_
typedef unsigned char byte;
#endif

// KJB Undefined true and false defined in SciTech's DEBUG.H header
#undef true
#undef false
typedef	enum	{false, true} qboolean;

// From mathlib...
typedef float	vec_t;
typedef vec_t	vec3_t[3];
typedef vec_t	vec5_t[5];
typedef	int	fixed4_t;
typedef	int	fixed8_t;
typedef	int	fixed16_t;


typedef	int	func_t;
typedef	int	string_t;
typedef	byte	pixel_t;

/*
typedef	enum	{key_game, key_console, key_message, key_menu} keydest_t;
typedef	enum	{ ALIAS_SINGLE=0, ALIAS_GROUP } aliasframetype_t;
typedef	enum	{ ALIAS_SKIN_SINGLE=0, ALIAS_SKIN_GROUP } aliasskintype_t;
typedef	enum	{ev_void, ev_string, ev_float, ev_vector, ev_entity, ev_field, ev_function, ev_pointer} etype_t;
typedef	void	(*builtin_t) (void);
typedef	enum	{touchessolid, drawnode, nodrawnode} solidstate_t;
typedef	enum	{ ST_SYNC=0, ST_RAND } synctype_t;
typedef	enum	{ SPR_SINGLE=0, SPR_GROUP } spriteframetype_t;
typedef	enum	{MS_WINDOWED, MS_FULLSCREEN, MS_FULLDIB, MS_UNINIT} modestate_t;
typedef	enum	{mod_brush, mod_sprite, mod_alias} modtype_t;
*/

#endif // _QTYPES_H
