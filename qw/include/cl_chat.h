/*
	cl_ignore.h

	Client-side ignore list management

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

#ifndef _CL_IGNORE_H
#define _CL_IGNORE_H

typedef struct ignore_s {
	int slot, uid;
	const char *lastname;
} ignore_t;

bool CL_Chat_Allow_Message (const char *str);
void CL_Chat_User_Disconnected (int uid);
void CL_Chat_Check_Name (const char *name, int slot);
void CL_Chat_Flush_Ignores (void);
void CL_Chat_Init (void);

#endif
