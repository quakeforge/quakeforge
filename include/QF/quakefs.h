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

#include "QF/qtypes.h"
#include "QF/quakeio.h"

//============================================================================

#define	MAX_OSPATH	128		// max length of a filesystem pathname

typedef struct searchpath_s {
	char        filename[MAX_OSPATH];
	struct pack_s *pack;	// only one of filename / pack will be used
	struct searchpath_s *next;
} searchpath_t;

extern searchpath_t *com_searchpaths;

extern struct cvar_s *fs_userpath;
extern struct cvar_s *fs_sharepath;
extern struct cvar_s *fs_basegame;
extern struct cvar_s *fs_skinbase;

extern int file_from_pak;
extern int com_filesize;
struct cache_user_s;

extern char	com_gamedir[MAX_OSPATH];
extern char	gamedirfile[MAX_OSPATH];

char *COM_CompressPath (const char *pth);
void COM_WriteFile (const char *filename, void *data, int len);
void COM_WriteBuffers (const char *filename, int count, ...);

int _COM_FOpenFile (const char *filename, QFile **gzfile, char *foundname, int zip);
int COM_FOpenFile (const char *filename, QFile **gzfile);
void COM_CloseFile (QFile *h);
void COM_FileBase (const char *in, char *out);
void COM_DefaultExtension (char *path, char *extension);
const char *COM_SkipPath (const char *pathname);
void COM_StripExtension (const char *in, char *out);
int COM_NextFilename (char *filename, const char *prefix, const char *ext);
const char *COM_FileExtension (const char *in);



byte *COM_LoadFile (const char *path, int usehunk);
byte *COM_LoadStackFile (const char *path, void *buffer, int bufsize);
byte *COM_LoadTempFile (const char *path);
byte *COM_LoadHunkFile (const char *path);
void COM_LoadCacheFile (const char *path, struct cache_user_s *cu);
void COM_CreatePath (const char *path);
void COM_Gamedir (const char *dir);
void COM_Filesystem_Init (void);
void COM_Filesystem_Init_Cvars (void);
void COM_Path_f (void);
void COM_CreateGameDirectory (const char *gamename);

#endif // __quakefs_h
