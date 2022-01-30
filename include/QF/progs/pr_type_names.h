/*
	pr_type_names.h

	Progs type names

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2022      Bill Currie <bill@taniwha.org>

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

#ifndef EV_TYPE
#define EV_TYPE(event)
#endif

EV_TYPE(void)
EV_TYPE(string)
EV_TYPE(float)
EV_TYPE(vector)
EV_TYPE(entity)
EV_TYPE(field)
EV_TYPE(func)
EV_TYPE(ptr)			// end of v6 types
EV_TYPE(quaternion)
EV_TYPE(int)
EV_TYPE(uint)
EV_TYPE(short)			// value is embedded in the opcode
EV_TYPE(double)
EV_TYPE(long)
EV_TYPE(ulong)
EV_TYPE(ushort)			// value is embedded in the opcode

#undef EV_TYPE
