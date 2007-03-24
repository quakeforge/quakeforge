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

/** \defgroup quakefs Quake Filesystem
	\ingroup utils
*/
//@{

#include "QF/qtypes.h"
#include "QF/quakeio.h"

//============================================================================

#define	MAX_OSPATH	128		// max length of a filesystem pathname

#define MAX_GAMEDIR_CALLBACKS 128	// most QFS_GamedirCallback calls.

typedef struct filelist_s {
	char      **list;
	int         count;
	int         size;
} filelist_t;

typedef struct searchpath_s {
	char        filename[MAX_OSPATH];
	struct pack_s *pack;	// only one of filename / pack will be used
	struct searchpath_s *next;
} searchpath_t;

typedef struct gamedir_s {
	const char *name;
	const char *gamedir;
	const char *path;
	const char *gamecode;
	struct {
		const char *def;
		const char *skins;
		const char *progs;
		const char *sound;
		const char *maps;
	} dir;
} gamedir_t;

/** Function type of callback called on gamedir change.
	\param phase	0 = before Cache_Flush(), 1 = after Cache_Flush()
*/
typedef void gamedir_callback_t (int phase);

extern searchpath_t *qfs_searchpaths;
extern gamedir_t *qfs_gamedir;

extern struct cvar_s *fs_sharepath;
extern struct cvar_s *fs_userpath;

extern const char *qfs_userpath;

extern int file_from_pak;
extern int qfs_filesize;

struct cache_user_s;
struct dstring_s;

void QFS_Init (const char *game);

void QFS_Gamedir (const char *dir);

QFile *QFS_Open (const char *path, const char *mode);
QFile *QFS_WOpen (const char *path, int zip);
void QFS_WriteFile (const char *filename, const void *data, int len);
void QFS_WriteBuffers (const char *filename, int count, ...);

int _QFS_FOpenFile (const char *filename, QFile **gzfile,
					struct dstring_s *foundname, int zip);
int QFS_FOpenFile (const char *filename, QFile **gzfile);
byte *QFS_LoadFile (const char *path, int usehunk);
byte *QFS_LoadStackFile (const char *path, void *buffer, int bufsize);
byte *QFS_LoadTempFile (const char *path);
byte *QFS_LoadHunkFile (const char *path);
void QFS_LoadCacheFile (const char *path, struct cache_user_s *cu);

int QFS_CreatePath (const char *path);
int QFS_Rename (const char *old_path, const char *new_path);
int QFS_Remove (const char *path);
int QFS_NextFilename (struct dstring_s *filename, const char *prefix,
					  const char *ext);

char *QFS_FileBase (const char *in);
void QFS_DefaultExtension (char *path, const char *extension);
void QFS_StripExtension (const char *in, char *out);
char *QFS_CompressPath (const char *pth);
const char *QFS_SkipPath (const char *pathname);
const char *QFS_FileExtension (const char *in);

void QFS_GamedirCallback (gamedir_callback_t *);

filelist_t *QFS_FilelistNew (void);
void QFS_FilelistAdd (filelist_t *filelist, const char *fname,
					  const char *ext);
void QFS_FilelistFill (filelist_t *list, const char *path, const char *ext,
					   int strip);
void QFS_FilelistFree (filelist_t *list);
void QFS_FilelistEnumerate(filelist_t* list, const char* path);
qboolean QFS_IsDirectory (const char *path);


// FIXME: This is here temporarily until fs_usercfg gets sorted out
char *expand_squiggle (const char *path);

//@}

#endif // __quakefs_h
