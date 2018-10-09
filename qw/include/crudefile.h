/*
	crudefile.h

	(description)

	Copyright (C) 2001 Adam Olsen

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

#ifndef _CRUDEFILE_H
#define _CRUDEFILE_H

void CF_Init (void);
void CF_CloseAllFiles (void);
int CF_Open (const char *path, const char *mode);
void CF_Close (int desc);
const char * CF_Read (int desc);
int CF_Write (int desc, const char *buf);
int CF_EOF (int desc);
int CF_Quota (void) __attribute__((pure));

#endif // _CRUDEFILE_H
