/*
	sys_developer.h

	Developer flags

	Copyright (C) 2021      Bill Currie <bill@taniwha.org>

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

#ifndef SYS_DEVELOPER
#define SYS_DEVELOPER(developer)
#endif

SYS_DEVELOPER (warn)
SYS_DEVELOPER (vid)
SYS_DEVELOPER (input)
SYS_DEVELOPER (fs_nf)
SYS_DEVELOPER (fs_f)
SYS_DEVELOPER (fs)
SYS_DEVELOPER (net)
SYS_DEVELOPER (rua_resolve)
SYS_DEVELOPER (rua_obj)
SYS_DEVELOPER (rua_msg)
SYS_DEVELOPER (snd)
SYS_DEVELOPER (glt)
SYS_DEVELOPER (glsl)
SYS_DEVELOPER (skin)
SYS_DEVELOPER (model)
SYS_DEVELOPER (vulkan)
SYS_DEVELOPER (vulkan_parse)

#undef SYS_DEVELOPER
