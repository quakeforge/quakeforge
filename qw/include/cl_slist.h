/*
	cl_slist.h

	Server listing address book interface

	Copyright (C) 1999,2000  Brian Koropoff

	Author: Brian Koropoff
	Date: 03 May 2000

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
#ifndef _CL_SLIST_H
#define _CL_SLIST_H

void SL_Init (void);

void MSL_ParseServerList(const char *msl_data);

int SL_CheckStatus (const char *cs_from, const char *cs_data);

void SL_CheckPing (const char *cp_from);

void SL_Shutdown (void);

#endif	// _CL_SLIST_H
