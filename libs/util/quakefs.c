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

*/
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>

#ifdef HAVE_FNMATCH_H
# define model_t sunmodel_t
# include <fnmatch.h>
# undef model_t
#else
# ifdef WIN32
# include "fnmatch.h"
# endif
#endif

#ifdef HAVE_IO_H
# include <io.h>
#endif

#ifdef _MSC_VER
# define _POSIX_
#endif

#include <limits.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/pak.h"
#include "QF/pakfile.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "QF/qfplist.h"
#include "QF/qtypes.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/zone.h"

#include "compat.h"

#ifndef HAVE_FNMATCH_PROTO
int fnmatch (const char *__pattern, const char *__string, int __flags);
#endif

/*
	All of Quake's data access is through a hierchical file system, but the
	contents of the file system can be transparently merged from several
	sources.

	The "user directory" is the path to the directory holding the quake.exe
	and all game directories.  This can be overridden with the "fs_sharepath"
	and "fs_userpath" cvars to allow code debugging in a different directory.
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

// QUAKE FILESYSTEM

cvar_t     *fs_userpath;
cvar_t     *fs_sharepath;
cvar_t     *fs_dirconf;

char        gamedirfile[MAX_OSPATH];

char        com_gamedir[MAX_OSPATH];
int         com_filesize;

searchpath_t *com_searchpaths;

//QFS

static void COM_AddGameDirectory (const char *dir); //FIXME

gamedir_t  *qfs_gamedir;
static plitem_t *qfs_gd_plist;
static const char *qfs_game = "";
static const char *qfs_default_dirconf =
	"{"
	"	Quake = {"
	"		Path = \"id1\";"
	"	};"
	"	QuakeWorld = {"
	"		Inherit = (Quake);"
	"		Path = \"qw\";"
	"		SkinPath = \"qw/skins\";"
	"	};"
	"	\"qw:*\" = {"
	"		Inherit = (QuakeWorld);"
	"		Path = \"$gamedir\";"
	"		GameCode = \"qwprogs.dat\";"
	"	};"
	"	\"nq:*\" = {"
	"		Inherit = (Quake);"
	"		Path = \"$gamedir\";"
	"		GameCode = \"progs.dat\";"
	"	};"
	"}";

static void
qfs_get_gd_params (plitem_t *gdpl, gamedir_t *gamedir, dstring_t *path)
{
	plitem_t   *p;

	if ((p = PL_ObjectForKey (gdpl, "Path"))) {
		if (path->str[0])
			dstring_appendstr (path, ":");
		dstring_appendstr (path, p->data);
	}
	if (!gamedir->gamecode && (p = PL_ObjectForKey (gdpl, "GameCode")))
		gamedir->gamecode = strdup (p->data);
	if (!gamedir->skinpath && (p = PL_ObjectForKey (gdpl, "SkinPath")))
		gamedir->skinpath = strdup (p->data);
}

static void
qfs_inherit (plitem_t *plist, plitem_t *gdpl, gamedir_t *gamedir,
			 dstring_t *path, hashtab_t *dirs)
{
	plitem_t   *base;

	if (!(base = PL_ObjectForKey (gdpl, "Inherit")))
		return;
	if (base->type == QFString) {
		if (Hash_Find (dirs, base->data))
			return;
		gdpl = PL_ObjectForKey (plist, base->data);
		if (!gdpl) {
			Sys_Printf ("base `%s' not found\n", (char *)base->data);
			return;
		}
		Hash_Add (dirs, base->data);
		qfs_get_gd_params (gdpl, gamedir, path);
		qfs_inherit (plist, gdpl, gamedir, path, dirs);
	} else if (base->type == QFArray) {
		int         i;
		plarray_t  *a = base->data;
		for (i = 0; i < a->numvals; i++) {
			base = a->values[i];
			if (Hash_Find (dirs, base->data))
				continue;
			gdpl = PL_ObjectForKey (plist, base->data);
			if (!gdpl) {
				Sys_Printf ("base `%s' not found\n", (char *)base->data);
				continue;
			}
			Hash_Add (dirs, base->data);
			qfs_get_gd_params (gdpl, gamedir, path);
			qfs_inherit (plist, gdpl, gamedir, path, dirs);
		}
	}
}

static int
qfs_compare (const void *a, const void *b)
{
	return strcmp ((*(dictkey_t **)a)->key, (*(dictkey_t **)b)->key);
}

static const char *
qfs_dir_get_key (void *_k, void *unused)
{
	return _k;
}

static gamedir_t *
qfs_get_gamedir (const char *name)
{
	gamedir_t  *gamedir;
	plitem_t   *gdpl;
	dstring_t  *path;
	hashtab_t  *dirs = Hash_NewTable (31, qfs_dir_get_key, 0, 0);

	gdpl = PL_ObjectForKey (qfs_gd_plist, name);
	if (!gdpl) {
		dictkey_t **list = (dictkey_t **) Hash_GetList (qfs_gd_plist->data);
		dictkey_t **l;
		for (l = list; *l; l++)
			;
		qsort (list, l - list, sizeof (char *), qfs_compare);
		while (l-- != list) {
			if (!fnmatch ((*l)->key, name, 0)) {
				gdpl = (*l)->value;
				Hash_Add (dirs, (*l)->key);
				break;
			}
		}
		free (list);
		if (!gdpl) {
			Sys_Printf ("gamedir `%s' not found\n", name);
			Hash_DelTable (dirs);
			return 0;
		}
	} else {
		Hash_Add (dirs, (void *)name);
	}
	gamedir = calloc (1, sizeof (gamedir_t));
	gamedir->name = strdup (name);
	path = dstring_newstr ();
	qfs_get_gd_params (gdpl, gamedir, path);
	qfs_inherit (qfs_gd_plist, gdpl, gamedir, path, dirs);
	gamedir->path = path->str;
	free (path);
	Hash_DelTable (dirs);
	return gamedir;
}

static void
qfs_load_config (void)
{
	QFile      *f;
	int         len;
	char       *buf;

	if (!(f = Qopen (fs_dirconf->string, "rt"))) {
		Sys_DPrintf ("Could not load `%s', using builtin defaults\n",
					fs_dirconf->string);
		goto no_config;
	}
	len = Qfilesize (f);
	buf = malloc (len + 3); // +3 for { } and \0

	Qread (f, buf + 1, len);
	Qclose (f);

	// convert the config file to a plist dictionary
	buf[0] = '{';
	buf[len + 1] = '}';
	buf[len + 2] = 0;
	qfs_gd_plist = PL_GetPropertyList (buf);
	free (buf);
	if (qfs_gd_plist && qfs_gd_plist->type == QFDictionary)
		return;		// done
	Sys_Printf ("not a dictionary\n");
no_config:
	qfs_gd_plist = PL_GetPropertyList (qfs_default_dirconf);
}

static void
qfs_process_path (const char *path, const char *gamedir)
{
	const char *e = path + strlen (path);
	const char *s = e;
	dstring_t  *dir = dstring_new ();

	while (s >= path) {
		while (s != path && s[-1] !=':')
			s--;
		if (s != e) {
			dsprintf (dir, "%.*s", e - s, s);
			if (strequal (dir->str, "$gamedir"))
				dsprintf (dir, "%s", gamedir);
			COM_AddGameDirectory (dir->str);
		}
		e = --s;
	}
	dstring_delete (dir);
}

void
COM_FileBase (const char *in, char *out)
{
	const char *slash, *dot, *s;

	slash = in;
	dot = NULL;
	s = in;
	while (*s) {
		if (*s == '/')
			slash = s + 1;
		if (*s == '.')
			dot = s;
		s++;
	}
	if (dot == NULL)
		dot = s;
	if (dot - slash < 2)
		strcpy (out, "?model?");
	else {
		while (slash < dot)
			*out++ = *slash++;
		*out++ = 0;
	}
}


void
COM_Path_f (void)
{
	searchpath_t *s;

	Sys_Printf ("Current search path:\n");
	for (s = com_searchpaths; s; s = s->next) {
		if (s->pack)
			Sys_Printf ("%s (%i files)\n", s->pack->filename,
						s->pack->numfiles);
		else
			Sys_Printf ("%s\n", s->filename);
	}
}

/*
	COM_WriteFile

	The filename will be prefixed by the current game directory
*/
void
COM_WriteFile (const char *filename, void *data, int len)
{
	char        name[MAX_OSPATH];
	QFile      *f;

	snprintf (name, sizeof (name), "%s/%s", com_gamedir, filename);

	f = Qopen (name, "wb");
	if (!f) {
		Sys_mkdir (com_gamedir);
		f = Qopen (name, "wb");
		if (!f)
			Sys_Error ("Error opening %s", filename);
	}

	Sys_Printf ("COM_WriteFile: %s\n", name);
	Qwrite (f, data, len);
	Qclose (f);
}

/*
	COM_WriteBuffers

	The filename will be prefixed by the current game directory
*/
void
COM_WriteBuffers (const char *filename, int count, ...)
{
	char        name[MAX_OSPATH];
	va_list     args;
	QFile      *f;

	va_start (args, count);

	snprintf (name, sizeof (name), "%s/%s", com_gamedir, filename);

	f = Qopen (name, "wb");
	if (!f) {
		Sys_mkdir (com_gamedir);
		f = Qopen (name, "wb");
		if (!f)
			Sys_Error ("Error opening %s", filename);
	}

	Sys_Printf ("COM_WriteBuffers: %s\n", name);
	while (count--) {
		void       *data = va_arg (args, void *);
		int         len = va_arg (args, int);

		Qwrite (f, data, len);
	}
	Qclose (f);
	va_end (args);
}

/*
	COM_CreatePath

	Only used for CopyFile and download
*/
void
COM_CreatePath (const char *path)
{
	char       *ofs;
	char        e_path[MAX_OSPATH];

	Qexpand_squiggle (path, e_path);

	for (ofs = e_path + 1; *ofs; ofs++) {
		if (*ofs == '/') {				// create the directory
			*ofs = 0;
			Sys_mkdir (e_path);
			*ofs = '/';
		}
	}
}

static QFile *
COM_OpenRead (const char *path, int offs, int len, int zip)
{
	QFile      *file;

	if (offs < 0 || len < 0)
		file = Qopen (path, zip ? "rbz" : "rb");
	else
		file = Qsubopen (path, offs, len, zip);

	if (!file) {
		Sys_Error ("Couldn't open %s", path);
		return 0;
	}

	com_filesize = Qfilesize (file);

	return file;
}

char *
COM_CompressPath (const char *pth)
{
	char       *p, *d;
	char       *path= malloc (strlen (pth) + 1);

	for (d = path; *pth; d++, pth++) {
		if (*pth == '\\')
			*d = '/';
		else
			*d = *pth;
	}
	*d = 0;

	p = path;
	while (*p) {
		if (p[0] == '.') {
			if (p[1] == '.') {
				if (p[2] == '/' || p[2] == 0) {
					d = p;
					if (d > path)
						d--;
					while (d > path && d[-1] != '/')
						d--;
					if (d == path
						&& d[0] == '.' && d[1] == '.'
						&& (d[2] == '/' || d[2] == '0')) {
						p += 2 + (p[2] == '/');
						continue;
					}
					strcpy (d, p + 2 + (p[2] == '/'));
					p = d;
					continue;
				}
			} else if (p[1] == '/') {
				strcpy (p, p + 2);
				continue;
			} else if (p[1] == 0) {
				p[0] = 0;
			}
		}
		while (*p && *p != '/')
			p++;
		if (*p == '/')
			p++;
	}

	return path;
}

/*
	contains_updir

	Checks if a string contains an updir ('..'), either at the ends or
	surrounded by slashes ('/').  Doesn't check for leading slash.
*/
static int
contains_updir (const char *filename)
{
	int i;

	if (filename[0] == 0 || filename [1] == 0)
                return 0;

	for (i = 0; filename[i+1]; i++) {
		if (!(i == 0 || filename[i-1] == '/')           // beginning of string
														// or first slash
		    || filename[i] != '.'                       // first dot
		    || filename[i+1] != '.')                    // second dot
			continue;

		if (filename[i+2] == 0 || filename[i+2] == '/')
			// end of string or second slash
			return 1;
	}
	return 0;
}

int file_from_pak; // global indicating file came from pack file ZOID

/*
	COM_FOpenFile

	Finds the file in the search path.
	Sets com_filesize and one of handle or file
*/

static int
open_file (searchpath_t *search, const char *filename, QFile **gzfile,
		   char *foundname, int zip)
{
	char        netpath[MAX_OSPATH];

	file_from_pak = 0;

	// is the element a pak file?
	if (search->pack) {
		dpackfile_t *packfile;

		packfile = pack_find_file (search->pack, filename);
		if (packfile) {
			Sys_DPrintf ("PackFile: %s : %s\n", search->pack->filename,
						 packfile->name);
			// open a new file on the pakfile
			strncpy (foundname, packfile->name, MAX_OSPATH);
			*gzfile = COM_OpenRead (search->pack->filename, packfile->filepos,
									packfile->filelen, zip);
			file_from_pak = 1;
			return com_filesize;
		}
	} else {
		// sanity check the strings
		if (strnlen (search->filename, sizeof (netpath))
			+ strnlen (filename, sizeof (netpath)) + 2 > sizeof (netpath))
			Sys_Error ("open_file: search->filename and/or filename "
					   "bogus: `%.*s'  `%.*s'",
					   (int) sizeof (netpath), search->filename,
					   (int) sizeof (netpath), filename);
		// check a file in the directory tree
		snprintf (netpath, sizeof (netpath), "%s/%s", search->filename,
				  filename);

		strncpy (foundname, filename, MAX_OSPATH);
		if (Sys_FileTime (netpath) == -1)
			return -1;

		Sys_DPrintf ("FindFile: %s\n", netpath);

		*gzfile = COM_OpenRead (netpath, -1, -1, zip);
		return com_filesize;
	}

	return -1;
}

int
_COM_FOpenFile (const char *filename, QFile **gzfile, char *foundname, int zip)
{
	searchpath_t *search;
	char       *path;
#ifdef HAVE_VORBIS
	char        oggfilename[MAX_OSPATH];
#endif
#ifdef HAVE_ZLIB
	char        gzfilename[MAX_OSPATH];
#endif

	// make sure they're not trying to do weird stuff with our private files
	path = COM_CompressPath (filename);
	if (contains_updir(path)) {
		Sys_Printf ("FindFile: %s: attempt to escape directory tree!\n", path);
		goto error;
	}

#ifdef HAVE_VORBIS
	if (strequal (".wav", COM_FileExtension (path))) {
		COM_StripExtension (path, oggfilename);
		strncat (oggfilename, ".ogg",
				 sizeof (oggfilename) - strlen (oggfilename) - 1);
	} else {
		oggfilename[0] = 0;
	}
#endif
#ifdef HAVE_ZLIB
	snprintf (gzfilename, sizeof (gzfilename), "%s.gz", path);
#endif

	// search through the path, one element at a time
	for (search = com_searchpaths; search; search = search->next) {
#ifdef HAVE_VORBIS
		//NOTE gzipped oggs not supported
		if (oggfilename[0]
			&& open_file (search, oggfilename, gzfile, foundname, false) != -1)
			goto ok;
#endif
#ifdef HAVE_ZLIB
		if (open_file (search, gzfilename, gzfile, foundname, zip) != -1)
			goto ok;
#endif
		if (open_file (search, path, gzfile, foundname, zip) != -1)
			goto ok;
	}

	Sys_DPrintf ("FindFile: can't find %s\n", filename);
error:
	*gzfile = NULL;
	com_filesize = -1;
	free (path);
	return -1;
ok:
	free(path);
	return com_filesize;
}

int
COM_FOpenFile (const char *filename, QFile **gzfile)
{
	char        foundname[MAX_OSPATH];

	return _COM_FOpenFile (filename, gzfile, foundname, 1);
}

cache_user_t *loadcache;
byte       *loadbuf;
int         loadsize;

/*
	COM_LoadFile

	Filename are relative to the quake directory.
	Always appends a 0 byte to the loaded data.
*/
byte       *
COM_LoadFile (const char *path, int usehunk)
{
	QFile      *h;
	byte       *buf = NULL;
	char        base[32];
	int         len;

	// look for it in the filesystem or pack files
	len = com_filesize = COM_FOpenFile (path, &h);
	if (!h)
		return NULL;

	// extract the filename base name for hunk tag
	COM_FileBase (path, base);

	if (usehunk == 1)
		buf = Hunk_AllocName (len + 1, base);
	else if (usehunk == 2)
		buf = Hunk_TempAlloc (len + 1);
	else if (usehunk == 0)
		buf = calloc (1, len + 1);
	else if (usehunk == 3)
		buf = Cache_Alloc (loadcache, len + 1, base);
	else if (usehunk == 4) {
		if (len + 1 > loadsize)
			buf = Hunk_TempAlloc (len + 1);
		else
			buf = loadbuf;
	} else
		Sys_Error ("COM_LoadFile: bad usehunk");

	if (!buf)
		Sys_Error ("COM_LoadFile: not enough space for %s", path);

	buf[len] = 0;
	Qread (h, buf, len);
	Qclose (h);

	return buf;
}

byte       *
COM_LoadHunkFile (const char *path)
{
	return COM_LoadFile (path, 1);
}

byte       *
COM_LoadTempFile (const char *path)
{
	return COM_LoadFile (path, 2);
}

void
COM_LoadCacheFile (const char *path, struct cache_user_s *cu)
{
	loadcache = cu;
	COM_LoadFile (path, 3);
}

// uses temp hunk if larger than bufsize
byte       *
COM_LoadStackFile (const char *path, void *buffer, int bufsize)
{
	byte       *buf;

	loadbuf = (byte *) buffer;
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
static pack_t     *
COM_LoadPackFile (char *packfile)
{
	pack_t     *pack = pack_open (packfile);

	if (pack)
		Sys_Printf ("Added packfile %s (%i files)\n",
					packfile, pack->numfiles);
	return pack;
}

#define FBLOCK_SIZE	32
#define FNAME_SIZE	MAX_OSPATH

// Note, this is /NOT/ a work-alike strcmp, this groups numbers sanely.
static int
qstrcmp (char **os1, char **os2)
{
	int         in1, in2, n1, n2;
	char       *s1, *s2;

	s1 = *os1;
	s2 = *os2;

	while (1) {
		in1 = in2 = n1 = n2 = 0;

		if ((in1 = isdigit ((byte) *s1)))
			n1 = strtol (s1, &s1, 10);

		if ((in2 = isdigit ((byte) *s2)))
			n2 = strtol (s2, &s2, 10);

		if (in1 && in2) {
			if (n1 != n2)
				return n1 - n2;
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

static void
COM_LoadGameDirectory (const char *dir)
{
	searchpath_t *search;
	pack_t     *pak;
	DIR        *dir_ptr;
	struct dirent *dirent;
	char      **pakfiles = NULL;
	int         i = 0, bufsize = 0, count = 0;

	Sys_DPrintf ("COM_LoadGameDirectory (\"%s\")\n", dir);

	pakfiles = calloc (1, FBLOCK_SIZE * sizeof (char *));

	bufsize += FBLOCK_SIZE;
	if (!pakfiles)
		goto COM_LoadGameDirectory_free;

	for (i = 0; i < bufsize; i++) {
		pakfiles[i] = NULL;
	}

	dir_ptr = opendir (dir);
	if (!dir_ptr)
		goto COM_LoadGameDirectory_free;

	while ((dirent = readdir (dir_ptr))) {
		if (!fnmatch ("*.pak", dirent->d_name, 0)) {
			if (count >= bufsize) {
				bufsize += FBLOCK_SIZE;
				pakfiles = realloc (pakfiles, bufsize * sizeof (char *));

				if (!pakfiles)
					goto COM_LoadGameDirectory_free;
				for (i = count; i < bufsize; i++)
					pakfiles[i] = NULL;
			}

			pakfiles[count] = malloc (FNAME_SIZE);
			if (!pakfiles[count])
				Sys_Error ("COM_LoadGameDirectory: MemoryAllocationFailure");
			snprintf (pakfiles[count], FNAME_SIZE - 1, "%s/%s", dir,
					  dirent->d_name);
			pakfiles[count][FNAME_SIZE - 1] = '\0';
			count++;
		}
	}
	closedir (dir_ptr);

	// XXX WARNING!!! This is /NOT/ subtable for strcmp!!!!!
	// This passes 'void **' instead of 'char *' to the cmp function!
	qsort (pakfiles, count, sizeof (char *),
		   (int (*)(const void *, const void *)) qstrcmp);

	for (i = 0; i < count; i++) {
		pak = COM_LoadPackFile (pakfiles[i]);

		if (!pak) {
			Sys_Error (va ("Bad pakfile %s!!", pakfiles[i]));
		} else {
			search = calloc (1, sizeof (searchpath_t));
			search->pack = pak;
			search->next = com_searchpaths;
			com_searchpaths = search;
		}
	}

  COM_LoadGameDirectory_free:
	for (i = 0; i < count; i++)
		free (pakfiles[i]);
	free (pakfiles);
}

/*
	COM_AddDirectory

	Sets com_gamedir, adds the directory to the head of the path,
	then loads and adds pak1.pak pak2.pak ...
*/
static void
COM_AddDirectory (const char *dir)
{
	searchpath_t *search;
	char       *p;
	char        e_dir[MAX_OSPATH];

	Qexpand_squiggle (dir, e_dir);
	dir = e_dir;

	if ((p = strrchr (dir, '/')) != NULL) {
		strcpy (gamedirfile, ++p);
		strcpy (com_gamedir, dir);
	} else {
		strcpy (gamedirfile, dir);
		strcpy (com_gamedir, va ("%s/%s", fs_userpath->string, dir));
	}

	// add the directory to the search path
	search = calloc (1, sizeof (searchpath_t));
	strcpy (search->filename, dir);
	search->next = com_searchpaths;
	com_searchpaths = search;

	// add any pak files in the format pak0.pak pak1.pak, ...
	COM_LoadGameDirectory (dir);
}

static void
COM_AddGameDirectory (const char *dir)
{
	if (!*dir)
		return;
	Sys_DPrintf ("COM_AddGameDirectory (\"%s/%s\")\n",
				 fs_sharepath->string, dir);

	if (strcmp (fs_sharepath->string, fs_userpath->string) != 0)
		COM_AddDirectory (va ("%s/%s", fs_sharepath->string, dir));
	COM_AddDirectory (va ("%s/%s", fs_userpath->string, dir));
}

/*
	COM_Gamedir

	Sets the gamedir and path to a different directory.
*/
void
COM_Gamedir (const char *dir)
{
	if (qfs_gamedir) {
		if (qfs_gamedir->name)
			free ((char *)qfs_gamedir->name);
		if (qfs_gamedir->path)
			free ((char *)qfs_gamedir->path);
		if (qfs_gamedir->gamecode)
			free ((char *)qfs_gamedir->gamecode);
		if (qfs_gamedir->skinpath)
			free ((char *)qfs_gamedir->skinpath);
		free (qfs_gamedir);
	}

	while (com_searchpaths) {
		searchpath_t *next;

		if (com_searchpaths->pack) {
			Qclose (com_searchpaths->pack->handle);
			free (com_searchpaths->pack->files);
			free (com_searchpaths->pack);
		}
		next = com_searchpaths->next;
		free (com_searchpaths);
		com_searchpaths = next;
	}

	qfs_gamedir = qfs_get_gamedir (va ("%s:%s", qfs_game, dir));
	if (qfs_gamedir) {
		Sys_DPrintf ("%s\n", qfs_gamedir->name);
		Sys_DPrintf ("    %s\n", qfs_gamedir->path);
		Sys_DPrintf ("    %s\n", qfs_gamedir->gamecode);
		Sys_DPrintf ("    %s\n", qfs_gamedir->skinpath);
		qfs_process_path (qfs_gamedir->path, dir);
	}

	// flush all data, so it will be forced to reload
	Cache_Flush ();
}

void
COM_CreateGameDirectory (const char *gamename)
{
	if (strcmp (fs_userpath->string, "."))
		COM_CreatePath (va ("%s/%s/dummy", fs_userpath->string, gamename));
	COM_AddGameDirectory (gamename);
}

void
QFS_Init (const char *game)
{
	fs_sharepath = Cvar_Get ("fs_sharepath", FS_SHAREPATH, CVAR_ROM, NULL,
							 "location of shared (read only) game "
							 "directories");
	fs_userpath = Cvar_Get ("fs_userpath", FS_USERPATH, CVAR_ROM, NULL,
							"location of your game directories");
	fs_dirconf = Cvar_Get ("fs_dirconf", "", CVAR_ROM, NULL,
							"full path to gamedir.conf FIXME");

	qfs_load_config ();

	qfs_game = game;

	COM_Gamedir ("");
}

const char *
COM_SkipPath (const char *pathname)
{
	const char *last;

	// char after last / on the line
	if ((last = strrchr (pathname, '/')))
		last++;
	else
		last = pathname;

	return last;
}

void
COM_StripExtension (const char *in, char *out)
{
	while (*in && *in != '.')
		*out++ = *in++;
	*out = 0;
}

const char *
COM_FileExtension (const char *in)
{
	while (*in && *in != '.')
		in++;
	return in;
}

void
COM_DefaultExtension (char *path, const char *extension)
{
	char       *src;

	// if path doesn't have a .EXT, append extension
	// (extension should include the .)
	src = path + strlen (path) - 1;

	while (*src != '/' && src != path) {
		if (*src == '.')
			return;						// it has an extension
		src--;
	}

	strncat (path, extension, MAX_OSPATH - strlen (path));
}

int
COM_NextFilename (char *filename, const char *prefix, const char *ext)
{
	char       *digits;
	char        checkname[MAX_OSPATH];
	int         i;

	strncpy (filename, prefix, MAX_OSPATH - 4);
	filename[MAX_OSPATH - 4] = 0;
	digits = filename + strlen (filename);
	strcat (filename, "000");
	strncat (filename, ext, MAX_OSPATH - strlen (filename));

	for (i = 0; i <= 999; i++) {
		digits[0] = i / 100 + '0';
		digits[1] = i / 10 % 10 + '0';
		digits[2] = i % 10 + '0';
		snprintf (checkname, sizeof (checkname), "%s/%s", com_gamedir,
				  filename);
		if (Sys_FileTime (checkname) == -1)
			return 1;					// file doesn't exist
	}
	return 0;
}
