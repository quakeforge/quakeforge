/*
	quakefs.h

	quake virtual filesystem definitions

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

#ifndef __quakefs_h
#define __quakefs_h

#include "qtypes.h"
#include "quakeio.h"
#include "cvar.h"

//============================================================================

#define	MAX_OSPATH	128		// max length of a filesystem pathname

extern cvar_t *fs_userpath;
extern cvar_t *fs_sharepath;
extern cvar_t *fs_skinbase;

extern int com_filesize;
struct cache_user_s;

extern char	com_gamedir[MAX_OSPATH];
extern char	gamedirfile[MAX_OSPATH];

void COM_WriteFile (char *filename, void *data, int len);
void COM_WriteBuffers (const char *filename, int count, ...);

int _COM_FOpenFile (char *filename, QFile **gzfile, char *foundname, int zip);
int COM_FOpenFile (char *filename, QFile **gzfile);
void COM_CloseFile (QFile *h);
int COM_filelength (QFile *f);
void COM_FileBase (char *in, char *out);
void COM_DefaultExtension (char *path, char *extension);
char *COM_SkipPath (char *pathname);
void COM_StripExtension (char *in, char *out);
int COM_NextFilename (char *filename, const char *prefix, const char *ext);


byte *COM_LoadStackFile (char *path, void *buffer, int bufsize);
byte *COM_LoadTempFile (char *path);
byte *COM_LoadHunkFile (char *path);
void COM_LoadCacheFile (char *path, struct cache_user_s *cu);
void COM_CreatePath (char *path);
void COM_Gamedir (char *dir);
void COM_Filesystem_Init (void);
void COM_Filesystem_Init_Cvars (void);
void COM_Path_f (void);
void COM_Maplist_f (void);

#endif // __quakefs_h
