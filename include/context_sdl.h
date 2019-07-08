/*
	context_sdl.h

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

*/

#ifndef __context_sdl_h_
#define __context_sdl_h_

#include <SDL.h>

extern SDL_Surface *sdl_screen;

void VID_SDL_GammaCheck (void);
void SDL_Init_Cvars (void);

struct gl_ctx_s *SDL_GL_Context (void);
void SDL_GL_Init_Cvars (void);

struct sw_ctx_s *SDL_SW_Context (void);
void SDL_SW_Init_Cvars (void);

extern uint32_t sdl_flags;

#endif	// __context_sdl_h_
