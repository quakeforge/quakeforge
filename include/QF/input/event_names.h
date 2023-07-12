/*
	event_names.h

	Input event enum names

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

#ifndef IE_EVENT
#define IE_EVENT(event)
#endif

IE_EVENT (none)
IE_EVENT (gain_focus)
IE_EVENT (lose_focus)
IE_EVENT (app_gain_focus)
IE_EVENT (app_lose_focus)
IE_EVENT (app_window)
IE_EVENT (add_device)
IE_EVENT (remove_device)
IE_EVENT (mouse)
IE_EVENT (key)
IE_EVENT (axis)
IE_EVENT (button)

#undef IE_EVENT
