/*
	quakefs.c

	virtual filesystem functions

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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <fcntl.h>

#include <dirent.h>
#ifdef HAVE_FNMATCH_H
#include <fnmatch.h>
#endif

#ifdef WIN32
#include <io.h>
#endif

#ifdef _MSC_VER
#define _POSIX_
#endif

#include "host.h"
#include "qtypes.h"
#include "quakefs.h"
#include "sys.h"
#include "console.h"
#include "draw.h"
#include "cmd.h"
#include "cvar.h"
#include "qendian.h"
#include "info.h"
#include "server.h"
#include "va.h"
#include "qargs.h"
#include "compat.h"

qboolean		standard_quake = true, abyss, rogue, hipnotic;
cvar_t			*registered;

/*
	All of Quake's data access is through a hierchical file system, but the
	contents of the file system can be transparently merged from several
	sources.

	The "user directory" is the path to the directory holding the quake.exe
	and all game directories.  The sys_* files pass this to host_init in
	quakeparms_t->basedir.  This can be overridden with the "-basedir"
	command line parm to allow code debugging in a different directory.
	The base directory is only used during filesystem initialization.

	The "game directory" is the first tree on the search path and directory
	that all generated files (savegames, screenshots, demos, config files)
	will be saved to.  This can be overridden with the "-game" command line
	parameter.  The game directory can never be changed while quake is
	executing.  This is a precacution against having a malicious server
	instruct clients to write files over areas they shouldn't.

	The "cache directory" is only used during development to save network
	bandwidth, especially over ISDN / T1 lines.  If there is a cache directory
	specified, when a file is found by the normal search path, it will be
	mirrored into the cache directory, then opened there.
*/

/*
=============================================================================

QUAKE FILESYSTEM

=============================================================================
*/

char    gamedirfile[MAX_OSPATH];

cvar_t	*fs_userpath;
cvar_t	*fs_sharepath;
cvar_t	*fs_basegame;


int com_filesize;

/*
	In-memory pack file structs
*/

typedef struct
{
	char	name[MAX_QPATH];
	int		filepos, filelen;
} packfile_t;

typedef struct pack_s
{
	char	filename[MAX_OSPATH];
	QFile	*handle;
	int		numfiles;
	packfile_t	*files;
} pack_t;

/*
	Structs for pack files on disk
*/
typedef struct
{
	char	name[56];
	int		filepos, filelen;
} dpackfile_t;

typedef struct
{
	char	id[4];
	int		dirofs;
	int		dirlen;
} dpackheader_t;

#define	MAX_FILES_IN_PACK	2048

char	com_gamedir[MAX_OSPATH];

typedef struct searchpath_s
{
	char	filename[MAX_OSPATH];
	pack_t	*pack;		// only one of filename / pack will be used
	struct searchpath_s *next;
} searchpath_t;

searchpath_t	*com_searchpaths;
searchpath_t	*com_base_searchpaths;	// without gamedirs

/*
	COM_FileBase
*/
void
COM_FileBase (char *in, char *out)
{
	char *s, *s2;
	
	s = in + strlen(in) - 1;
	
	while (s != in && *s != '.')
		s--;
	
	for (s2 = s ; *s2 && *s2 != '/' ; s2--)
	;
	
	if (s-s2 < 2)
		strcpy (out,"?model?");
	else
	{
		s--;
		strncpy (out,s2+1, s-s2);
		out[s-s2] = 0;
	}
}

/*
	COM_filelength
*/
int
COM_filelength (QFile *f)
{
	int		pos;
	int		end;

	pos = Qtell (f);
	Qseek (f, 0, SEEK_END);
	end = Qtell (f);
	Qseek (f, pos, SEEK_SET);

	return end;
}

/*
	COM_FileOpenRead
*/
int
COM_FileOpenRead (char *path, QFile **hndl)
{
	QFile	*f;

	f = Qopen(path, "rbz");
	if (!f)
	{
		*hndl = NULL;
		return -1;
	}
	*hndl = f;

	return COM_filelength(f);
}

/*
	COM_Path_f
*/
void
COM_Path_f (void)
{
	searchpath_t	*s;

	Con_Printf ("Current search path:\n");
	for (s=com_searchpaths ; s ; s=s->next)
	{
		if (s == com_base_searchpaths)
			Con_Printf ("----------\n");
		if (s->pack)
			Con_Printf ("%s (%i files)\n", s->pack->filename, s->pack->numfiles);
		else
			Con_Printf ("%s\n", s->filename);
	}
}

/*
	COM_Maplist_f

	List map files in gamepaths.
*/

struct maplist {
	char **list;
	int count;
	int size;
};

static struct maplist*
maplist_new()
{
	return calloc(1, sizeof(struct maplist));
}

static void
maplist_free(struct maplist *maplist)
{
	free (maplist->list);
	free (maplist);
}

static void
maplist_add_map(struct maplist *maplist, char *fname)
{
	char **new_list;

	if (maplist->count == maplist->size) {
		maplist->size += 32;
		new_list = realloc (maplist->list, maplist->size * sizeof(char*));
		if (!new_list) {
			maplist->size -= 32;
			return;
		}
		maplist->list = new_list;
	}
	maplist->list[maplist->count++] = fname;
}

static int
maplist_cmp(const void *_a, const void *_b)
{
	char *a = *(char**)_a;
	char *b = *(char**)_b;
	int al = strstr (a, ".bsp") - a;
	int bl = strstr (b, ".bsp") - b;
	int cmp = strncmp (a, b, min (al, bl));

	if (cmp == 0)
		return al - bl;
	return cmp;
}

static void
maplist_print(struct maplist *maplist)
{
	int i;
	char *end;
	char *name;

	qsort(maplist->list, maplist->count, sizeof(char *), maplist_cmp);
	for (i=0; i<maplist->count - 1; i++) {
		name = maplist->list[i];
		end = strstr (name, ".bsp");
		Con_Printf ("%-8.*s%c", end - name, name, ((i + 1) % 4) ? ' ' : '\n');
	}
	name = maplist->list[i];
	end = strstr (name, ".bsp");
	Con_Printf ("%-9.*s\n", end - name, name);
}

void
COM_Maplist_f ( void )
{
	searchpath_t	*search;
	DIR				*dir_ptr;
	struct dirent	*dirent;
	char			buf[MAX_OSPATH];

	for (search = com_searchpaths ; search != NULL ; search = search->next) {
		if (search->pack) {
			int i;
			pack_t *pak = search->pack;
			struct maplist *maplist = maplist_new ();

			Con_Printf ("Looking in %s...\n",search->pack->filename);
			for (i=0 ; i<pak->numfiles ; i++) {
				char *name=pak->files[i].name;
				if (!fnmatch ("maps/*.bsp", name, FNM_PATHNAME)
					|| !fnmatch ("maps/*.bsp.gz", name, FNM_PATHNAME))
					maplist_add_map (maplist, name+5);
			}
			maplist_print (maplist);
			maplist_free (maplist);
		} else {
			struct maplist *maplist = maplist_new ();

			snprintf (buf, sizeof(buf), "%s/maps", search->filename);
			dir_ptr = opendir(buf);
			Con_Printf ("Looking in %s...\n",buf);
			if (!dir_ptr)
				continue;
			while ((dirent = readdir (dir_ptr)))
				if (!fnmatch ("*.bsp", dirent->d_name, 0)
					|| !fnmatch ("*.bsp.gz", dirent->d_name, 0))
					maplist_add_map (maplist, dirent->d_name);
			closedir (dir_ptr);
			maplist_print (maplist);
			maplist_free (maplist);
		}
	}
}

/*
	COM_WriteFile

	The filename will be prefixed by the current game directory
*/
void
COM_WriteFile ( char *filename, void *data, int len )
{
	QFile	*f;
	char	name[MAX_OSPATH];

	snprintf(name, sizeof(name), "%s/%s", com_gamedir, filename);

	f = Qopen (name, "wb");
	if (!f) {
		Sys_mkdir(com_gamedir);
		f = Qopen (name, "wb");
		if (!f)
			Sys_Error ("Error opening %s", filename);
	}

	Sys_Printf ("COM_WriteFile: %s\n", name);
	Qwrite (f, data, len);
	Qclose (f);
}


/*
	COM_CreatePath

	Only used for CopyFile and download
*/
void
COM_CreatePath ( char *path )
{
	char	*ofs;
	char	e_path[PATH_MAX];

	Qexpand_squiggle (path, e_path);
	path = e_path;

	for (ofs = path+1 ; *ofs ; ofs++) {
		if (*ofs == '/') {	// create the directory
			*ofs = 0;
			Sys_mkdir (path);
			*ofs = '/';
		}
	}
}


/*
	COM_CopyFile

	Copies a file over from the net to the local cache, creating any
	directories needed.  This is for the convenience of developers using
	ISDN from home.
*/
void
COM_CopyFile (char *netpath, char *cachepath)
{
	QFile	*in, *out;
	int		remaining, count;
	char	buf[4096];

	remaining = COM_FileOpenRead (netpath, &in);
	COM_CreatePath (cachepath);	// create directories up to the cache file
	out = Qopen(cachepath, "wb");
	if (!out)
		Sys_Error ("Error opening %s", cachepath);

	while (remaining)
	{
		if (remaining < sizeof(buf))
			count = remaining;
		else
			count = sizeof(buf);
		Qread  (in, buf, count);
		Qwrite (out, buf, count);
		remaining -= count;
	}

	Qclose (in);
	Qclose (out);
}

/*
	COM_OpenRead
*/
QFile *
COM_OpenRead (const char *path, int offs, int len, int zip)
{
	int fd=open(path,O_RDONLY);
	unsigned char id[2];
	unsigned char len_bytes[4];
	if (fd==-1) {
		Sys_Error ("Couldn't open %s", path);
		return 0;
	}
	if (offs<0 || len<0) {
		// normal file
		offs=0;
		len=lseek(fd,0,SEEK_END);
		lseek(fd,0,SEEK_SET);
	}
	lseek(fd,offs,SEEK_SET);
	if (zip) {
		read(fd,id,2);
		if (id[0]==0x1f && id[1]==0x8b) {
			lseek(fd,offs+len-4,SEEK_SET);
			read(fd,len_bytes,4);
			len=((len_bytes[3]<<24)
				 |(len_bytes[2]<<16)
				 |(len_bytes[1]<<8)
				 |(len_bytes[0]));
		}
	}
	lseek(fd,offs,SEEK_SET);
	com_filesize=len;

#ifdef WIN32
	setmode(fd,O_BINARY);
#endif
	if (zip)
		return Qdopen(fd,"rbz");
	else
		return Qdopen(fd,"rb");
}

int file_from_pak; // global indicating file came from pack file ZOID

/*
	COM_FOpenFile

	Finds the file in the search path.
	Sets com_filesize and one of handle or file
*/
int
_COM_FOpenFile (char *filename, QFile **gzfile, char *foundname, int zip)
{
	searchpath_t	*search;
	char		netpath[MAX_OSPATH];
	pack_t		*pak;
	int			i;
	int			findtime;
#ifdef HAVE_ZLIB
	char		gzfilename[MAX_OSPATH];
	int			filenamelen;;

	filenamelen = strlen(filename);
	strncpy(gzfilename,filename,sizeof(gzfilename));
	strncat(gzfilename,".gz",sizeof(gzfilename));
#endif

	file_from_pak = 0;

//
// search through the path, one element at a time
//
	for (search = com_searchpaths ; search ; search = search->next)
	{
	// is the element a pak file?
		if (search->pack)
		{
		// look through all the pak file elements
			pak = search->pack;
			for (i=0 ; i<pak->numfiles ; i++) {
				char *fn=0;
#ifdef HAVE_ZLIB
				if (!strncmp(pak->files[i].name, filename, filenamelen)) {
					if (!pak->files[i].name[filenamelen])
						fn=filename;
					else if (!strcmp (pak->files[i].name, gzfilename))
						fn=gzfilename;
				}
#else
				if (!strcmp (pak->files[i].name, filename))
					fn=filename;
#endif
				if (fn)
				{	// found it!
					if (developer->int_val)
						Sys_Printf ("PackFile: %s : %s\n",pak->filename, fn);
					// open a new file on the pakfile
					strncpy(foundname, fn, MAX_OSPATH);
					*gzfile=COM_OpenRead(pak->filename, pak->files[i].filepos,
										 pak->files[i].filelen, zip);
					file_from_pak = 1;
					return com_filesize;
				}
			}
		}
		else
		{
	// check a file in the directory tree
			snprintf(netpath, sizeof(netpath), "%s/%s",search->filename,
					 filename);

			strncpy(foundname, filename, MAX_OSPATH);
			findtime = Sys_FileTime (netpath);
			if (findtime == -1) {
#ifdef HAVE_ZLIB
				strncpy(foundname, gzfilename, MAX_OSPATH);
				snprintf(netpath, sizeof(netpath), "%s/%s",search->filename,
						 gzfilename);
				findtime = Sys_FileTime (netpath);
				if (findtime == -1)
#endif
					continue;
			}

			if(developer->int_val)
				Sys_Printf ("FindFile: %s\n",netpath);

			*gzfile=COM_OpenRead(netpath, -1, -1, zip);
			return com_filesize;
		}

	}

	if(developer->int_val)
		Sys_Printf ("FindFile: can't find %s\n", filename);

	*gzfile = NULL;
	com_filesize = -1;
	return -1;
}

int
COM_FOpenFile (char *filename, QFile **gzfile)
{
	char foundname[MAX_OSPATH];
	return _COM_FOpenFile (filename, gzfile, foundname, 1);
}

cache_user_t *loadcache;
byte	*loadbuf;
int		loadsize;

/*
	COM_LoadFile

	Filename are relative to the quake directory.
	Allways appends a 0 byte to the loaded data.
*/
byte *
COM_LoadFile (char *path, int usehunk)
{
	QFile	*h;
	byte	*buf;
	char	base[32];
	int		len;

	buf = NULL;	// quiet compiler warning

// look for it in the filesystem or pack files
	len = com_filesize = COM_FOpenFile (path, &h);
	if (!h)
		return NULL;

// extract the filename base name for hunk tag
	COM_FileBase (path, base);

	if (usehunk == 1)
		buf = Hunk_AllocName (len+1, base);
	else if (usehunk == 2)
		buf = Hunk_TempAlloc (len+1);
	else if (usehunk == 0)
		buf = calloc (1, len+1);
	else if (usehunk == 3)
		buf = Cache_Alloc (loadcache, len+1, base);
	else if (usehunk == 4)
	{
		if (len+1 > loadsize)
			buf = Hunk_TempAlloc (len+1);
		else
			buf = loadbuf;
	}
	else
		Sys_Error ("COM_LoadFile: bad usehunk");

	if (!buf)
		Sys_Error ("COM_LoadFile: not enough space for %s", path);

	((byte *)buf)[len] = 0;
	Draw_BeginDisc();
	Qread (h, buf, len);
	Qclose (h);
	Draw_EndDisc();

	return buf;
}

byte *
COM_LoadHunkFile (char *path)
{
	return COM_LoadFile (path, 1);
}

byte *
COM_LoadTempFile (char *path)
{
	return COM_LoadFile (path, 2);
}

void
COM_LoadCacheFile (char *path, struct cache_user_s *cu)
{
	loadcache = cu;
	COM_LoadFile (path, 3);
}

// uses temp hunk if larger than bufsize
byte *
COM_LoadStackFile (char *path, void *buffer, int bufsize)
{
	byte	*buf;

	loadbuf = (byte *)buffer;
	loadsize = bufsize;
	buf = COM_LoadFile (path, 4);

	return buf;
}

/*
	COM_LoadPackFile

	Takes an explicit (not game tree related) path to a pak file.

	Loads the header and directory, adding the files at the beginning
	of the list so they override previous pack files.
*/
pack_t *
COM_LoadPackFile (char *packfile)
{
	dpackheader_t		header;
	int			i;
	packfile_t		*newfiles;
	int			numpackfiles;
	pack_t			*pack;
	QFile			*packhandle;
	dpackfile_t		info[MAX_FILES_IN_PACK];

	if (COM_FileOpenRead (packfile, &packhandle) == -1)
		return NULL;

	Qread (packhandle, &header, sizeof(header));
	if (header.id[0] != 'P' || header.id[1] != 'A'
	|| header.id[2] != 'C' || header.id[3] != 'K')
		Sys_Error ("%s is not a packfile", packfile);
	header.dirofs = LittleLong (header.dirofs);
	header.dirlen = LittleLong (header.dirlen);

	numpackfiles = header.dirlen / sizeof(dpackfile_t);

	if (numpackfiles > MAX_FILES_IN_PACK)
		Sys_Error ("%s has %i files", packfile, numpackfiles);

	newfiles = calloc (1, numpackfiles * sizeof(packfile_t));

	Qseek (packhandle, header.dirofs, SEEK_SET);
	Qread (packhandle, info, header.dirlen);


// parse the directory
	for (i=0 ; i<numpackfiles ; i++)
	{
		strcpy (newfiles[i].name, info[i].name);
		newfiles[i].filepos = LittleLong(info[i].filepos);
		newfiles[i].filelen = LittleLong(info[i].filelen);
	}

	pack = calloc (1, sizeof (pack_t));
	strcpy (pack->filename, packfile);
	pack->handle = packhandle;
	pack->numfiles = numpackfiles;
	pack->files = newfiles;

	Con_Printf ("Added packfile %s (%i files)\n", packfile, numpackfiles);
	return pack;
}

#define FBLOCK_SIZE	32
#define FNAME_SIZE	MAX_OSPATH

// Note, this is /NOT/ a work-alike strcmp, this groups numbers sanely.
//int qstrcmp(const char *val, const char *ref)
int qstrcmp(char **os1, char **os2)
{
	int in1, in2, n1, n2;
	char *s1, *s2;
	s1 = *os1;
	s2 = *os2;

	while (1) {
		in1 = in2 = n1 = n2 = 0;

		if ((in1 = isdigit((int) *s1)))
			n1 = strtol(s1, &s1, 10);

		if ((in2 = isdigit((int) *s2)))
			n2 = strtol(s2, &s2, 10);

		if (in1 && in2) {
			if (n1 != n2)
				return n1-n2;
		} else {
			if (*s1 != *s2)
				return *s1 - *s2;
			else if (*s1 == '\0')
				return *s1 - *s2;
			s1++;
			s2++;
		}
	}
}


void
COM_LoadGameDirectory(char *dir)
{
	searchpath_t	*search;
	pack_t			*pak;
	DIR				*dir_ptr;
	struct dirent	*dirent;
	char			**pakfiles = NULL;
	int				i = 0, bufsize = 0, count = 0;

	Con_DPrintf ("COM_LoadGameDirectory (\"%s\")\n", dir);

	pakfiles = calloc(1, FBLOCK_SIZE * sizeof(char *));
	bufsize += FBLOCK_SIZE;
	if (!pakfiles)
		goto COM_LoadGameDirectory_free;

	for (i = count; i < bufsize; i++) {
		pakfiles[i] = NULL;
	}

	dir_ptr = opendir(dir);
	if (!dir_ptr)
		return;

	while ((dirent = readdir(dir_ptr))) {
		if (!fnmatch("*.pak", dirent->d_name, 0)) {
			if (count >= bufsize) {
				bufsize += FBLOCK_SIZE;
				pakfiles = realloc(pakfiles, bufsize * sizeof(char *));
				if (!pakfiles)
					goto COM_LoadGameDirectory_free;
				for (i = count; i < bufsize; i++)
					pakfiles[i] = NULL;
			}

			pakfiles[count] = malloc(FNAME_SIZE);
			snprintf(pakfiles[count], FNAME_SIZE - 1, "%s/%s", dir,
					dirent->d_name);
			pakfiles[count][FNAME_SIZE - 1] = '\0';
			count++;
		}
	}
	closedir(dir_ptr);

	// XXX WARNING!!! This is /NOT/ subtable for strcmp!!!!!
	// This passes 'char **' instead of 'char *' to the cmp function!
	qsort(pakfiles, count, sizeof(char *), 
			(int (*)(const void *, const void *)) qstrcmp);

	for (i = 0; i < count; i++) {
		pak = COM_LoadPackFile(pakfiles[i]);

		if (!pak) {
			Sys_Error(va("Bad pakfile %s!!", pakfiles[i]));
		} else {
			search = calloc (1, sizeof(searchpath_t));
			search->pack = pak;
			search->next = com_searchpaths;
			com_searchpaths = search;
		}
	}

COM_LoadGameDirectory_free:
	for (i = 0; i < count; i++)
		free(pakfiles[i]);
	free(pakfiles);
}

/*
	COM_AddDirectory

	Sets com_gamedir, adds the directory to the head of the path,
	then loads and adds pak1.pak pak2.pak ...
*/
void
COM_AddDirectory (char *dir)
{
	searchpath_t	*search;
	char			*p;
	char			e_dir[PATH_MAX];

	Qexpand_squiggle (dir, e_dir);
	dir = e_dir;

	if ((p = strrchr(dir, '/')) != NULL) {
		strcpy (gamedirfile, ++p);
		strcpy (com_gamedir, dir);
	} else {
		strcpy (gamedirfile, dir);
		strcpy (com_gamedir, va("%s/%s", fs_userpath->string, dir));
	}

//
// add the directory to the search path
//
	search = calloc (1, sizeof(searchpath_t));
	strcpy (search->filename, dir);
	search->next = com_searchpaths;
	com_searchpaths = search;

//
// add any pak files in the format pak0.pak pak1.pak, ...
//

	COM_LoadGameDirectory (dir);
}

/*
	COM_AddGameDirectory

	FIXME: this is a wrapper for COM_AddDirectory (which used to be
	this function) to have it load share and base paths.  Whenver
	someone goes through to clean up the fs code, this function should
	merge with COM_AddGameDirectory.
*/
void
COM_AddGameDirectory (char *dir)
{
	Con_DPrintf ("COM_AddGameDirectory (\"%s/%s\")\n",
			fs_sharepath->string, dir);

	if (strcmp (fs_sharepath->string, fs_userpath->string) != 0)
		COM_AddDirectory (va("%s/%s", fs_sharepath->string, dir));
	COM_AddDirectory (va("%s/%s", fs_userpath->string, dir));
}

/*
	COM_CreateGameDirectory
*/
void
COM_CreateGameDirectory (char *gamename)
{
	if (strcmp (fs_userpath->string, FS_USERPATH))
		COM_CreatePath (va("%s/%s/dummy", fs_userpath->string,
						gamename));
	COM_AddGameDirectory (gamename);
}

/*
	COM_InitFilesystem
*/
void
COM_InitFilesystem ( void )
{
	int i;

	fs_sharepath = Cvar_Get ("fs_sharepath", FS_SHAREPATH, CVAR_ROM,
			"location of shared (read only) game directories");
	fs_userpath = Cvar_Get ("fs_userpath", FS_USERPATH, CVAR_ROM,
			"location of your game directories");
	fs_basegame = Cvar_Get ("fs_basegame", BASEGAME, CVAR_ROM,
			"game to use by default");

/*
	start up with basegame->string by default
*/
	COM_CreateGameDirectory (fs_basegame->string);

	if ((i = COM_CheckParm ("-game")) && i < com_argc - 1) {
		char *gamedirs = NULL;
		char *where;

		gamedirs = strdup (com_argv[i+1]);
		where = strtok (gamedirs, ",");
		while (where) {
			COM_CreateGameDirectory (where);
			where = strtok (NULL, ",");
		}
		free (gamedirs);
	}
	if ((i = COM_CheckParm ("-game")) && i < com_argc - 1) {
		COM_CreateGameDirectory(com_argv[i+1]);
	}
	if (hipnotic) {
		COM_CreateGameDirectory ("hipnotic");
	}
	if (rogue) {
		COM_CreateGameDirectory ("rogue");
	}
	if (abyss) {
		COM_CreateGameDirectory ("abyss");
	}

	// any set gamedirs will be freed up to here
	com_base_searchpaths = com_searchpaths;
}

/*
============
COM_SkipPath
============
*/
char *COM_SkipPath (char *pathname)
{
	char	*last;
	
	last = pathname;
	while (*pathname)
	{
		if (*pathname=='/')
			last = pathname+1;
		pathname++;
	}
	return last;
}

/*
============
COM_StripExtension
============
*/
void COM_StripExtension (char *in, char *out)
{
	while (*in && *in != '.')
		*out++ = *in++;
	*out = 0;
}

/*
============
COM_FileExtension
============
*/
char *COM_FileExtension (char *in)
{
	static char exten[8];
	int		i;

	while (*in && *in != '.')
		in++;
	if (!*in)
		return "";
	in++;
	for (i=0 ; i<7 && *in ; i++,in++)
		exten[i] = *in;
	exten[i] = 0;
	return exten;
}


/*
==================
COM_DefaultExtension
==================
*/
void COM_DefaultExtension (char *path, char *extension)
{
	char    *src;
//
// if path doesn't have a .EXT, append extension
// (extension should include the .)
//
	src = path + strlen(path) - 1;

	while (*src != '/' && src != path)
	{
		if (*src == '.')
			return;                 // it has an extension
		src--;
	}

	strcat (path, extension);
}
