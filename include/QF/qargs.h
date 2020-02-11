/*
	qargs.h

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

#ifndef __qargs_h
#define __qargs_h

/** \addtogroup misc
*/
///@{

#include "QF/qtypes.h"

extern int com_argc;
extern const char **com_argv;
extern const char *com_cmdline;
extern struct cvar_s *fs_globalcfg;
extern struct cvar_s *fs_usercfg;

int COM_CheckParm (const char *parm) __attribute__((pure));
void COM_AddParm (const char *parm);

void COM_Init (void);
void COM_Init_Cvars (void);
void COM_InitArgv (int argc, const char **argv);
void COM_ParseConfig (void);

///@}

#endif // __qargs_h
