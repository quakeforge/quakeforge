/*
	winquake.h

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

#ifndef _WINQUAKE_H
#define _WINQUAKE_H

#ifdef _WIN32

#include <memoryapi.h>

#ifndef __GNUC__
# pragma warning( disable : 4229 )  /* mgraph gets this */
#endif

#define byte __hide_byte
#define shutdown __hide_shutdown

#include <windows.h>

#ifdef WINNT
# ifdef HAVE_DSOUND_H
#  include <dsound.h>
# endif
#else
# include <windows.h>
# ifdef HAVE_DSOUND_H
#  include <dsound.h>
# endif
#endif

#ifdef HAVE_DDRAW_H
# include <ddraw.h>
#endif

#ifdef HAVE_MGRAPH_H
# include <mgraph.h>
#endif

#undef byte
#undef shutdown
#undef LoadImage

#include "QF/qtypes.h"

#ifndef WM_MOUSEWHEEL
# define WM_MOUSEWHEEL                   0x020A
#endif

extern	HINSTANCE	global_hInstance;

typedef enum {MS_WINDOWED, MS_FULLSCREEN, MS_FULLDIB, MS_UNINIT} modestate_t;

extern modestate_t	modestate;

extern bool	WinNT;

extern bool	winsock_lib_initialized;

#undef E_POINTER

#endif /* _WIN32 */

#endif /* _WINQUAKE_H */
