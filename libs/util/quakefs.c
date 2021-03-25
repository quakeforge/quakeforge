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
#ifdef HAVE_IO_H
# include <io.h>
#endif

#if defined(_WIN32) && defined(HAVE_MALLOC_H)
#include <malloc.h>
#endif

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef HAVE_FNMATCH_H
# define model_t sunmodel_t
# include <fnmatch.h>
# undef model_t
#else
# ifdef _WIN32
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

#include "qfalloca.h"
#include "QF/alloc.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/hash.h"
#include "QF/mathlib.h"
#include "QF/pak.h"
#include "QF/pakfile.h"
#include "QF/plist.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
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
	The base directory is used only during filesystem initialization.

	The "game directory" is the first tree on the search path and directory
	that all generated files (savegames, screenshots, demos, config files)
	will be saved to.  This can be overridden with the "-game" command line
	parameter.  The game directory can never be changed while quake is
	executing.  This is a precacution against having a malicious server
	instruct clients to write files over areas they shouldn't.

	The "cache directory" is used only during development to save network
	bandwidth, especially over ISDN / T1 lines.  If there is a cache directory
	specified, when a file is found by the normal search path, it will be
	mirrored into the cache directory, then opened there.
*/

// QUAKE FILESYSTEM

static cvar_t *fs_userpath;
static cvar_t *fs_sharepath;
static cvar_t *fs_dirconf;

VISIBLE const char *qfs_userpath;

VISIBLE int qfs_filesize;

typedef struct searchpath_s {
	char       *filename;
	struct pack_s *pack;	// only one of filename / pack will be used
	struct searchpath_s *next;
} searchpath_t;

/** Represent a single game directory.

	A vpath is the union of all locations searched for a single gamedir. This
	includes all pak files in the gamedir in the user's directory, the
	filesystem in the user's gamedir, then all pak files in the share
	game directory, and finally share's gamedir filesystem.

	The purpose is to enable searches to be limited to a single gamedir or to
	not search past a certain gamedir.
*/
struct vpath_s {			// typedef to vpath_t is in quakefs.h
	char       *name;		// usually the gamedir name
	searchpath_t *user;
	searchpath_t *share;
	struct vpath_s *next;
};

typedef struct int_findfile_s {
	findfile_t ff;
	struct pack_s *pack;
	dpackfile_t *packfile;
	const char *path;
	int         fname_index;
} int_findfile_t;

static searchpath_t *searchpaths_freelist;
static vpath_t *vpaths_freelist;
static vpath_t *qfs_vpaths;

//QFS

typedef struct qfs_var_s {
	char       *var;
	char       *val;
} qfs_var_t;

static void qfs_add_gamedir (vpath_t *vpath, const char *dir);

VISIBLE gamedir_t  *qfs_gamedir;
static plitem_t *qfs_gd_plist;
static const char *qfs_game = "";
static const char *qfs_default_dirconf =
	"{"
	"   QF = {"
	"       Path = \"QF\";"
	"   };"
	"	Quake = {"
	"		Inherit = QF;"
	"		Path = \"id1\";"
	"		GameCode = \"progs.dat\";"
	"		HudType = \"id\";"
	"	};"
	"	QuakeWorld = {"
	"		Inherit = (Quake);"
	"		Path = \"qw\";"
	"		SkinPath = \"${path}/skins\";"
	"		GameCode = \"qwprogs.dat\";"
	"		HudType = \"id\";"
	"	};"
	"	\"Hipnotic\" = {"
	"		Inherit = (Quake);"
	"		Path = \"hipnotic\";"
	"		HudType = \"hipnotic\";"
	"	};"
	"	\"Rogue\" = {"
	"		Inherit = (Quake);"
	"		Path = \"rogue\";"
	"		HudType = \"rogue\";"
	"	};"
	"	\"qw:qw\" = {"
	"		Inherit = (QuakeWorld);"
	"	};"
	"	\"qw:*\" = {"
	"		Inherit = (QuakeWorld);"
	"		Path = \"$gamedir\";"
	"	};"
	"	\"nq:*\" = {"
	"		Inherit = (Quake);"
	"		Path = \"$gamedir\";"
	"	};"
	"	\"hipnotic:*\" = {"
	"		Inherit = (Hipnotic);"
	"		Path = \"$gamedir\";"
	"	};"
	"	\"rogue:*\" = {"
	"		Inherit = (Rogue);"
	"		Path = \"$gamedir\";"
	"	};"
	"	\"abyss\" = {"
	"		Inherit = (Quake);"
	"		Path = \"abyss\";"
	"	};"
	"	\"abyss:*\" = {"
	"		Inherit = (abyss);"
	"		Path = \"$gamedir\";"
	"	};"
	"}";


#define GAMEDIR_CALLBACK_CHUNK 16
static gamedir_callback_t **gamedir_callbacks;
static int num_gamedir_callbacks;
static int max_gamedir_callbacks;

static searchpath_t *
new_searchpath (void)
{
	searchpath_t *searchpath;
	ALLOC (16, searchpath_t, searchpaths, searchpath);
	return searchpath;
}

static void
delete_searchpath (searchpath_t *searchpath)
{
	if (searchpath->pack) {
		pack_del (searchpath->pack);
	}
	if (searchpath->filename) {
		free (searchpath->filename);
	}
	FREE (searchpaths, searchpath);
}

static vpath_t *
new_vpath (void)
{
	vpath_t    *vpath;
	ALLOC (16, vpath_t, vpaths, vpath);
	return vpath;
}

static void
delete_vpath (vpath_t *vpath)
{
	searchpath_t *next;

	if (vpath->name) {
		free (vpath->name);
	}
	while (vpath->user) {
		next = vpath->user->next;
		delete_searchpath (vpath->user);
		vpath->user = next;
	}
	while (vpath->share) {
		next = vpath->share->next;
		delete_searchpath (vpath->share);
		vpath->share = next;
	}
	FREE (vpaths, vpath);
}

static const char *
qfs_var_get_key (const void *_v, void *unused)
{
	return ((qfs_var_t *)_v)->var;
}

static void
qfs_var_free (void *_v, void *unused)
{
	qfs_var_t  *v = (qfs_var_t *) _v;
	free (v->var);
	free (v->val);
	free (v);
}

static hashtab_t *
qfs_new_vars (void)
{
	return Hash_NewTable (61, qfs_var_get_key, qfs_var_free, 0, 0);
}

static void
qfs_set_var (hashtab_t *vars, const char *var, const char *val)
{
	qfs_var_t  *v = Hash_Find (vars, var);

	if (!v) {
		v = malloc (sizeof (qfs_var_t));
		v->var = strdup (var);
		v->val = 0;
		Hash_Add (vars, v);
	}
	if (v->val)
		free (v->val);
	v->val = strdup (val);
}

static inline int
qfs_isident (byte c)
{
	return ((c >= 'a' && c <='z') || (c >= 'A' && c <='Z')
			|| (c >= '0' && c <= '9') || c == '_');
}

static char *
qfs_var_subst (const char *string, hashtab_t *vars)
{
	dstring_t  *new = dstring_newstr ();
	const char *s = string;
	const char *e = s;
	const char *var;
	qfs_var_t  *sub;

	while (1) {
		while (*e && *e != '$')
			e++;
		dstring_appendsubstr (new, s, (e - s));
		if (!*e++)
			break;
		if (*e == '$') {
			dstring_appendstr (new, "$");
			s = ++e;
		} else if (*e == '{') {
			s = e;
			while (*e && *e != '}')
				e++;
			if (!*e) {
				dstring_appendsubstr (new, s, (e - s));
				break;
			}
			var = va (0, "%.*s", (int) (e - s) - 1, s + 1);
			sub = Hash_Find (vars, var);
			if (sub)
				dstring_appendstr (new, sub->val);
			else
				dstring_appendsubstr (new, s - 1, (e - s) + 2);
			s = ++e;
		} else if (qfs_isident (*e)) {
			s = e;
			while (qfs_isident (*e))
				e++;
			var = va (0, "%.*s", (int) (e - s), s);
			sub = Hash_Find (vars, var);
			if (sub)
				dstring_appendstr (new, sub->val);
			else
				dstring_appendsubstr (new, s - 1, (e - s) + 1);
			s = e;
		} else {
			dstring_appendstr (new, "$");
			s = e;
		}
	}
	return dstring_freeze (new);
}

static void
qfs_get_gd_params (plitem_t *gdpl, gamedir_t *gamedir, dstring_t *path,
				   hashtab_t *vars)
{
	plitem_t   *p;
	const char *ps;

	if ((p = PL_ObjectForKey (gdpl, "Path")) && *(ps = PL_String (p))) {
		char       *str = qfs_var_subst (ps, vars);

		qfs_set_var (vars, "path", str);
		if (path->str[0])
			dstring_appendstr (path, ":");
		dstring_appendstr (path, str);
		free (str);
	}
	if (!gamedir->gamecode && (p = PL_ObjectForKey (gdpl, "GameCode")))
		gamedir->gamecode = qfs_var_subst (PL_String (p), vars);
	if (!gamedir->hudtype && (p = PL_ObjectForKey (gdpl, "HudType")))
		gamedir->hudtype = qfs_var_subst (PL_String (p), vars);
	if (!gamedir->dir.skins && (p = PL_ObjectForKey (gdpl, "SkinPath")))
		gamedir->dir.skins = qfs_var_subst (PL_String (p), vars);
	if (!gamedir->dir.models && (p = PL_ObjectForKey (gdpl, "ModelPath")))
		gamedir->dir.models = qfs_var_subst (PL_String (p), vars);
	if (!gamedir->dir.sound && (p = PL_ObjectForKey (gdpl, "SoundPath")))
		gamedir->dir.sound = qfs_var_subst (PL_String (p), vars);
	if (!gamedir->dir.maps && (p = PL_ObjectForKey (gdpl, "MapPath")))
		gamedir->dir.maps = qfs_var_subst (PL_String (p), vars);
	if (!gamedir->dir.shots && (p = PL_ObjectForKey (gdpl, "ShotsPath")))
		gamedir->dir.shots = qfs_var_subst (PL_String (p), vars);
}

static void
qfs_inherit (plitem_t *plist, plitem_t *gdpl, gamedir_t *gamedir,
			 dstring_t *path, hashtab_t *dirs, hashtab_t *vars)
{
	plitem_t   *base_item;

	if (!(base_item = PL_ObjectForKey (gdpl, "Inherit")))
		return;
	switch (PL_Type (base_item)) {
		case QFString:
			{
				const char *base = PL_String (base_item);
				if (Hash_Find (dirs, base))
					return;
				gdpl = PL_ObjectForKey (plist, base);
				if (!gdpl) {
					Sys_Printf ("base `%s' not found\n", base);
					return;
				}
				qfs_set_var (vars, "gamedir", base);
				Hash_Add (dirs, strdup (base));
				qfs_get_gd_params (gdpl, gamedir, path, vars);
				qfs_inherit (plist, gdpl, gamedir, path, dirs, vars);
			}
			break;
		case QFArray:
			{
				int         i, num_dirs;
				plitem_t   *basedir_item;
				const char *basedir;

				num_dirs = PL_A_NumObjects (base_item);
				for (i = 0; i < num_dirs; i++) {
					basedir_item = PL_ObjectAtIndex (base_item, i);
					if (!basedir_item)
						continue;
					basedir = PL_String (basedir_item);
					if (!basedir || Hash_Find (dirs, basedir))
						continue;
					gdpl = PL_ObjectForKey (plist, basedir);
					if (!gdpl) {
						Sys_Printf ("base `%s' not found\n", basedir);
						continue;
					}
					qfs_set_var (vars, "gamedir", basedir);
					Hash_Add (dirs, strdup (basedir));
					qfs_get_gd_params (gdpl, gamedir, path, vars);
					qfs_inherit (plist, gdpl, gamedir, path, dirs, vars);
				}
			}
			break;
		default:
			break;
	}
}

static int
qfs_compare (const void *a, const void *b)
{
	return strcmp (*(const char **) a, *(const char **) b);
}

static const char *
qfs_dir_get_key (const void *_k, void *unused)
{
	return _k;
}

static void
qfs_dir_free (void *_k, void *unused)
{
	free (_k);
}

static plitem_t *
qfs_find_gamedir (const char *name, hashtab_t *dirs)
{
	plitem_t   *gdpl = PL_ObjectForKey (qfs_gd_plist, name);

	if (!gdpl) {
		plitem_t   *keys = PL_D_AllKeys (qfs_gd_plist);
		int         num_keys = PL_A_NumObjects (keys);
		const char **list = malloc (num_keys * sizeof (char *));
		int         i;

		for (i = 0; i < num_keys; i++)
			list[i] = PL_String (PL_ObjectAtIndex (keys, i));
		qsort (list, num_keys, sizeof (const char *), qfs_compare);

		while (i--) {
			if (!fnmatch (list[i], name, 0)) {
				gdpl = PL_ObjectForKey (qfs_gd_plist, list[i]);
				Hash_Add (dirs, strdup (list[i]));
				break;
			}
		}
		free (list);
		PL_Free (keys);
	}
	return gdpl;
}

static void
qfs_process_path (const char *path, const char *gamedir)
{
	const char *e = path + strlen (path);
	const char *s = e;
	char       *dir;
	vpath_t    *vpath;

	while (s >= path) {
		while (s != path && s[-1] !=':')
			s--;
		if (s != e) {
			dir = nva ("%.*s", (int) (e - s), s);
			vpath = new_vpath ();
			vpath->name = dir;
			qfs_add_gamedir (vpath, dir);
			vpath->next = qfs_vpaths;
			qfs_vpaths = vpath;
		}
		e = --s;
	}
}

static void
qfs_build_gamedir (const char **list)
{
	int         j;
	gamedir_t  *gamedir;
	plitem_t   *gdpl;
	dstring_t  *path;
	hashtab_t  *dirs = Hash_NewTable (31, qfs_dir_get_key, qfs_dir_free, 0, 0);
	hashtab_t  *vars = qfs_new_vars ();
	const char *dir = 0;

	qfs_set_var (vars, "game", qfs_game);

	if (qfs_gamedir) {
		if (qfs_gamedir->name)
			free ((char *)qfs_gamedir->name);
		if (qfs_gamedir->gamedir)
			free ((char *)qfs_gamedir->gamedir);
		if (qfs_gamedir->path)
			free ((char *)qfs_gamedir->path);
		if (qfs_gamedir->gamecode)
			free ((char *)qfs_gamedir->gamecode);
		if (qfs_gamedir->dir.def)
			free ((char *)qfs_gamedir->dir.def);
		if (qfs_gamedir->dir.skins)
			free ((char *)qfs_gamedir->dir.skins);
		if (qfs_gamedir->dir.models)
			free ((char *)qfs_gamedir->dir.models);
		if (qfs_gamedir->dir.sound)
			free ((char *)qfs_gamedir->dir.sound);
		if (qfs_gamedir->dir.maps)
			free ((char *)qfs_gamedir->dir.maps);
		free (qfs_gamedir);
	}

	while (qfs_vpaths) {
		vpath_t    *next = qfs_vpaths->next;
		delete_vpath (qfs_vpaths);
		qfs_vpaths = next;
	}

	for (j = 0; list[j]; j++)
		;
	gamedir = calloc (1, sizeof (gamedir_t));
	path = dstring_newstr ();
	while (j--) {
		const char *name = va (0, "%s:%s", qfs_game, dir = list[j]);
		if (Hash_Find (dirs, name))
			continue;
		gdpl = qfs_find_gamedir (name, dirs);
		if (!gdpl) {
			Sys_Printf ("gamedir `%s' not found\n", name);
			continue;
		}
		Hash_Add (dirs, strdup (name));
		if (!j) {
			gamedir->name = strdup (name);
			gamedir->gamedir = strdup (list[j]);
		}
		qfs_set_var (vars, "gamedir", dir);
		qfs_get_gd_params (gdpl, gamedir, path, vars);
		qfs_inherit (qfs_gd_plist, gdpl, gamedir, path, dirs, vars);
	}
	gamedir->path = path->str;

	for (dir = gamedir->path; *dir && *dir != ':'; dir++)
		;
	gamedir->dir.def = nva ("%.*s", (int) (dir - gamedir->path),
							gamedir->path);
	if (!gamedir->hudtype)
		gamedir->hudtype = strdup ("id");
	if (!gamedir->dir.skins)
		gamedir->dir.skins = nva ("%s/skins", gamedir->dir.def);
	if (!gamedir->dir.models)
		gamedir->dir.models = nva ("%s/progs", gamedir->dir.def);
	if (!gamedir->dir.sound)
		gamedir->dir.sound = nva ("%s/sound", gamedir->dir.def);
	if (!gamedir->dir.maps)
		gamedir->dir.maps = nva ("%s/maps", gamedir->dir.def);
	if (!gamedir->dir.shots)
		gamedir->dir.shots = strdup ("QF");

	qfs_gamedir = gamedir;
	Sys_MaskPrintf (SYS_FS, "%s\n", qfs_gamedir->name);
	Sys_MaskPrintf (SYS_FS, "    gamedir : %s\n", qfs_gamedir->gamedir);
	Sys_MaskPrintf (SYS_FS, "    path    : %s\n", qfs_gamedir->path);
	Sys_MaskPrintf (SYS_FS, "    gamecode: %s\n", qfs_gamedir->gamecode);
	Sys_MaskPrintf (SYS_FS, "    hudtype : %s\n", qfs_gamedir->hudtype);
	Sys_MaskPrintf (SYS_FS, "    def     : %s\n", qfs_gamedir->dir.def);
	Sys_MaskPrintf (SYS_FS, "    skins   : %s\n", qfs_gamedir->dir.skins);
	Sys_MaskPrintf (SYS_FS, "    models  : %s\n", qfs_gamedir->dir.models);
	Sys_MaskPrintf (SYS_FS, "    sound   : %s\n", qfs_gamedir->dir.sound);
	Sys_MaskPrintf (SYS_FS, "    maps    : %s\n", qfs_gamedir->dir.maps);
	qfs_process_path (qfs_gamedir->path, dir);
	free (path);
	Hash_DelTable (dirs);
	Hash_DelTable (vars);
}

static void
qfs_load_config (void)
{
	QFile      *f = 0;
	int         len;
	char       *buf;
	char       *dirconf;

	if (*fs_dirconf->string) {
		dirconf = Sys_ExpandSquiggle (fs_dirconf->string);
		if (!(f = Qopen (dirconf, "rt")))
			Sys_MaskPrintf (SYS_FS,
							"Could not load `%s', using builtin defaults\n",
							dirconf);
		free (dirconf);
	}
	if (!f)
		goto no_config;

	len = Qfilesize (f);
	buf = malloc (len + 3); // +3 for { } and \0

	Qread (f, buf + 1, len);
	Qclose (f);

	// convert the config file to a plist dictionary
	buf[0] = '{';
	buf[len + 1] = '}';
	buf[len + 2] = 0;
	if (qfs_gd_plist)
		PL_Free (qfs_gd_plist);
	qfs_gd_plist = PL_GetPropertyList (buf, 0);
	free (buf);
	if (qfs_gd_plist && PL_Type (qfs_gd_plist) == QFDictionary)
		return;		// done
	Sys_Printf ("not a dictionary\n");
no_config:
	if (qfs_gd_plist)
		PL_Free (qfs_gd_plist);
	qfs_gd_plist = PL_GetPropertyList (qfs_default_dirconf, 0);
}

/*
	qfs_contains_updir

	Checks if a string contains an updir ('..'), either at the ends or
	surrounded by slashes ('/').  Doesn't check for leading slash.
	Assumes canonical (compressed) path.
*/
static inline int
qfs_contains_updir (const char *path, int levels)
{
	do {
		if (path[0] != '.' || path[1] != '.'
			|| (path[2] != '/' && path[2] != 0))
			return 0;
		if (!path[2])
			break;
		// first part of path is ../
		if (levels <= 0)
			return 1;
		path += 3;
	} while (levels-- > 0);
	return 0;
}

static int
qfs_expand_path (dstring_t *full_path, const char *base, const char *path,
				 int levels)
{
	const char *separator = "/";
	char       *cpath;
	int         len;

	if (!base || !*base) {
		errno = EACCES;
		return -1;
	}
	cpath = QFS_CompressPath (path);
	if (qfs_contains_updir (cpath, levels)) {
		free (cpath);
		errno = EACCES;
		return -1;
	}
	if (*cpath == '/')
		separator = "";
	len = strlen (base);
	if (len && base[len - 1] == '/')
		len--;
	dsprintf (full_path, "%.*s%s%s", len, base, separator, cpath);
	free (cpath);
	return 0;
}

static int
qfs_expand_userpath (dstring_t *full_path, const char *path)
{
	return qfs_expand_path (full_path, qfs_userpath, path, 0);
}

VISIBLE char *
QFS_FileBase (const char *in)
{
	const char *base;
	const char *ext;
	int         len;
	char       *out;

	base = QFS_SkipPath (in);
	ext = QFS_FileExtension (base);
	len = ext - base;
	out = malloc (len + 1);
	strncpy (out, base, len);
	out [len] = 0;
	return out;
}

static void
qfs_path_print (searchpath_t *sp)
{
	if (sp->pack) {
		Sys_Printf ("%s (%i files)\n", sp->pack->filename, sp->pack->numfiles);
	} else {
		Sys_Printf ("%s\n", sp->filename);
	}
}

static void
qfs_path_f (void)
{
	vpath_t    *vp;
	searchpath_t *sp;

	Sys_Printf ("Current search path:\n");
	for (vp = qfs_vpaths; vp; vp = vp->next) {
		Sys_Printf ("%s\n", vp->name);
		for (sp = vp->user; sp; sp = sp->next) {
			qfs_path_print (sp);
		}
		for (sp = vp->share; sp; sp = sp->next) {
			qfs_path_print (sp);
		}
	}
}

VISIBLE void
QFS_WriteFile (const char *filename, const void *data, int len)
{
	QFile      *f;

	f = QFS_WOpen (filename, 0);
	if (!f) {
		Sys_Error ("Error opening %s", filename);
	}

	Qwrite (f, data, len);
	Qclose (f);
}

static int_findfile_t *
qfs_findfile_search (const vpath_t *vpath, const searchpath_t *sp,
					 const char **fnames)
{
	static int_findfile_t found;
	const char **fn;

	found.ff.vpath = 0;
	found.ff.in_pak = false;
	found.pack = 0;
	found.packfile = 0;
	found.fname_index = 0;
	if (found.ff.realname) {
		free ((char *) found.ff.realname);
		found.ff.realname = 0;
	}
	if (found.path) {
		free ((char *) found.path);
		found.path = 0;
	}
	// is the element a pak file?
	if (sp->pack) {
		dpackfile_t *packfile = 0;

		for (fn = fnames; *fn; fn++) {
			packfile = pack_find_file (sp->pack, *fn);
			if (packfile) {
				break;
			}
		}
		if (packfile) {
			Sys_MaskPrintf (SYS_FS_F, "PackFile: %s : %s\n",
							sp->pack->filename, packfile->name);
			found.ff.vpath = vpath;
			found.ff.in_pak = true;
			found.ff.realname = strdup (*fn);
			found.pack = sp->pack;
			found.packfile = packfile;
			found.fname_index = fn - fnames;
			found.path = strdup (sp->pack->filename);
			return &found;
		}
	} else {
		// check a file in the directory tree
		dstring_t  *path = dstring_new ();

		for (fn = fnames; *fn; fn++) {
			if (qfs_expand_path (path, sp->filename, *fn, 1) == 0) {
				if (Sys_FileExists (path->str) == -1) {
					continue;
				}

				Sys_MaskPrintf (SYS_FS_F, "FindFile: %s\n", path->str);

				found.ff.vpath = vpath;
				found.ff.in_pak = false;
				found.ff.realname = strdup (*fn);
				found.path = strdup (path->str);
				found.fname_index = fn - fnames;
				dstring_delete (path);
				return &found;
			}
		}
		dstring_delete (path);
	}
	return 0;
}

static int_findfile_t *
qfs_findfile (const char **fnames, const vpath_t *start, const vpath_t *end)
{
	const vpath_t *vp;
	searchpath_t *sp;
	int_findfile_t *found;

	if (!start) {
		start = qfs_vpaths;
	}
	if (end) {
		for (vp = start; vp; vp = vp->next) {
			if (vp == end) {
				break;
			}
		}
		if (!vp) {
			Sys_Error ("QFS_FindFile: end vpath not in search list");
		}
		end = end->next;
	}
	for (vp = start; vp && vp != end; vp = vp->next) {
		for (sp = vp->user; sp; sp = sp->next) {
			found = qfs_findfile_search (vp, sp, fnames);
			if (found) {
				return found;
			}
		}
		for (sp = vp->share; sp; sp = sp->next) {
			found = qfs_findfile_search (vp, sp, fnames);
			if (found) {
				return found;
			}
		}
	}
	// file not found
	return 0;
}

VISIBLE findfile_t *
QFS_FindFile (const char *fname, const vpath_t *start, const vpath_t *end)
{
	int_findfile_t *found;
	const char *fname_list[2];

	fname_list[0] = fname;
	fname_list[1] = 0;

	found = qfs_findfile (fname_list, start, end);
	if (found) {
		return &found->ff;
	}
	return 0;
}

static QFile *
qfs_openread (const char *path, int offs, int len, int zip)
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

	qfs_filesize = Qfilesize (file);

	return file;
}

VISIBLE char *
QFS_CompressPath (const char *pth)
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
						&& (d[2] == '/' || d[2] == '\0')) {
						p += 2 + (p[2] == '/');
						continue;
					}
					if (d[0] == '/'
						&& d[1] == '.' && d[2] == '.'
						&& (d[3] == '/' || d[3] == '\0')) {
						*p = 0;
						p += 2 + (p[2] == '/');
						continue;
					}
					p = p + 2 + (p[2] == '/');
					memmove (d, p, strlen (p) + 1);
					p = d;
					continue;
				}
			} else if (p[1] == '/') {
				memmove (p, p + 2, strlen (p + 2) + 1);
				continue;
			} else if (p[1] == 0) {
				p[0] = 0;
			}
		}
		while (*p && *p != '/')
			p++;
		if (*p == '/') {
			p++;
			// skip over multiple / (foo//bar -> foo/bar)
			for (d = p; *d == '/'; d++)
				;
			if (d != p)
				memmove (p, d, strlen (d) + 1);
		}
	}
	// strip any trailing /, but not if it's the root /
	if (--p > path && *p == '/')
		*p = 0;

	return path;
}

VISIBLE findfile_t qfs_foundfile;

/*
	QFS_FOpenFile

	Finds the file in the search path.
	Sets qfs_filesize and one of handle or file
*/

static void
open_file (int_findfile_t *found, QFile **gzfile, int zip)
{
	qfs_foundfile = found->ff;
	if (found->ff.in_pak) {
		*gzfile = qfs_openread (found->pack->filename,
								found->packfile->filepos,
								found->packfile->filelen, zip);
	} else {
		*gzfile = qfs_openread (found->path, -1, -1, zip);
	}
}

VISIBLE QFile *
_QFS_VOpenFile (const char *filename, int zip,
				const vpath_t *start, const vpath_t *end)
{
	QFile      *gzfile;
	int_findfile_t *found;
	char       *path;
	const char *fnames[4];
	int         zip_flags[3];
	int         ind;

	// make sure they're not trying to do weird stuff with our private files
	path = QFS_CompressPath (filename);
	if (qfs_contains_updir(path, 1)) {
		Sys_MaskPrintf (SYS_FS,
						"FindFile: %s: attempt to escape directory tree!\n",
						path);
		goto error;
	}

	ind = 0;
#ifdef HAVE_VORBIS
	if (strequal (".wav", QFS_FileExtension (path))) {
		char       *oggfilename;
		oggfilename = alloca (strlen (path) + 1);
		QFS_StripExtension (path, oggfilename);
		strncat (oggfilename, ".ogg",
				 sizeof (oggfilename) - strlen (oggfilename) - 1);
		fnames[ind] = oggfilename;
		zip_flags[ind] = 0;
		ind++;
	}
#endif
#ifdef HAVE_ZLIB
	{
		char       *gzfilename;
		gzfilename = alloca (strlen (path) + 3 + 1);
		sprintf (gzfilename, "%s.gz", path);
		fnames[ind] = gzfilename;
		zip_flags[ind] = zip;
		ind++;
	}
#endif

	fnames[ind] = path;
	zip_flags[ind] = zip;
	ind++;

	fnames[ind] = 0;

	found = qfs_findfile (fnames, start, end);

	if (found) {
		open_file (found, &gzfile, zip_flags[found->fname_index]);
		free(path);
		return gzfile;
	}

	Sys_MaskPrintf (SYS_FS_NF, "FindFile: can't find %s\n", filename);
error:
	qfs_filesize = -1;
	free (path);
	return 0;
}

VISIBLE QFile *
QFS_VOpenFile (const char *filename, const vpath_t *start, const vpath_t *end)
{
	return _QFS_VOpenFile (filename, 1, start, end);
}

VISIBLE QFile *
_QFS_FOpenFile (const char *filename, int zip)
{
	return _QFS_VOpenFile (filename, zip, 0, 0);
}

VISIBLE QFile *
QFS_FOpenFile (const char *filename)
{
	return _QFS_VOpenFile (filename, 1, 0, 0);
}

static cache_user_t *loadcache;

/*
	QFS_LoadFile

	Filename are relative to the quake directory.
	Always appends a 0 byte to the loaded data.
*/
VISIBLE byte *
QFS_LoadFile (QFile *file, int usehunk)
{
	byte       *buf = NULL;
	char       *base;
	int         len;

	// look for it in the filesystem or pack files
	if (!file)
		return NULL;

	base = strdup ("QFS_LoadFile");

	len = qfs_filesize = Qfilesize (file);

	// extract the filename base name for hunk tag
	//base = QFS_FileBase (path);

	if (usehunk == 1)
		buf = Hunk_AllocName (len + 1, base);
	else if (usehunk == 2)
		buf = Hunk_TempAlloc (len + 1);
	else if (usehunk == 0)
		buf = calloc (1, len + 1);
	else if (usehunk == 3)
		buf = Cache_Alloc (loadcache, len + 1, base);
	else
		Sys_Error ("QFS_LoadFile: bad usehunk");

	if (!buf)
		Sys_Error ("QFS_LoadFile: not enough space");
		//Sys_Error ("QFS_LoadFile: not enough space for %s", path);

	buf[len] = 0;
	Qread (file, buf, len);
	Qclose (file);

	free (base);

	return buf;
}

VISIBLE byte *
QFS_LoadHunkFile (QFile *file)
{
	return QFS_LoadFile (file, 1);
}

VISIBLE void
QFS_LoadCacheFile (QFile *file, struct cache_user_s *cu)
{
	loadcache = cu;
	QFS_LoadFile (file, 3);
}

/*
	qfs_load_pakfile

	Takes an explicit (not game tree related) path to a pak file.

	Loads the header and directory, adding the files at the beginning
	of the list so they override previous pack files.
*/
static pack_t     *
qfs_load_pakfile (char *packfile)
{
	pack_t     *pack = pack_open (packfile);

	if (pack)
		Sys_MaskPrintf (SYS_FS, "Added packfile %s (%i files)\n",
					packfile, pack->numfiles);
	return pack;
}

#define FBLOCK_SIZE	32

// Note, this is /NOT/ a work-alike strcmp, this groups numbers sanely.
static int
qfs_file_sort (char **os1, char **os2)
{
	int         in1, in2, n1, n2;
	char       *s1, *s2;

	s1 = *os1;
	s2 = *os2;

	while (1) {
		n1 = n2 = 0;

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
qfs_load_gamedir (searchpath_t **searchpath, const char *dir)
{
	searchpath_t *sp;
	pack_t     *pak;
	DIR        *dir_ptr;
	struct dirent *dirent;
	char      **pakfiles = NULL;
	int         i = 0, bufsize = 0, count = 0;

	Sys_MaskPrintf (SYS_FS, "qfs_load_gamedir (\"%s\")\n", dir);

	pakfiles = calloc (1, FBLOCK_SIZE * sizeof (char *));

	bufsize += FBLOCK_SIZE;
	if (!pakfiles)
		goto qfs_load_gamedir_free;

	for (i = 0; i < bufsize; i++) {
		pakfiles[i] = NULL;
	}

	dir_ptr = opendir (dir);
	if (!dir_ptr)
		goto qfs_load_gamedir_free;

	while ((dirent = readdir (dir_ptr))) {
		if (!fnmatch ("*.pak", dirent->d_name, 0)) {
			if (count >= bufsize) {
				bufsize += FBLOCK_SIZE;
				pakfiles = realloc (pakfiles, bufsize * sizeof (char *));

				if (!pakfiles)
					goto qfs_load_gamedir_free;
				for (i = count; i < bufsize; i++)
					pakfiles[i] = NULL;
			}

			// at this point, dir is known to not have a trailing /, and
			// dirent->d_name definitely won't start with one.
			pakfiles[count] = nva ("%s/%s", dir, dirent->d_name);
			if (!pakfiles[count])
				Sys_Error ("qfs_load_gamedir: Memory allocation failure");
			count++;
		}
	}
	closedir (dir_ptr);

	qsort (pakfiles, count, sizeof (char *),
		   (int (*)(const void *, const void *)) qfs_file_sort);

	for (i = 0; i < count; i++) {
		pak = qfs_load_pakfile (pakfiles[i]);

		if (!pak) {
			Sys_Error ("Bad pakfile %s!!", pakfiles[i]);
		} else {
			sp = new_searchpath ();
			sp->pack = pak;
			sp->next = *searchpath;
			*searchpath = sp;
		}
	}

  qfs_load_gamedir_free:
	for (i = 0; i < count; i++)
		free (pakfiles[i]);
	free (pakfiles);
}

/*
	qfs_add_dir

	Adds the directory to the head of the path, then loads and adds pak0.pak
	pak1.pak ...
*/
static void
qfs_add_dir (searchpath_t **searchpath, const char *dir)
{
	searchpath_t *sp;

	// add the directory to the search path
	sp = new_searchpath ();
	sp->filename = strdup (dir);
	sp->next = *searchpath;
	*searchpath = sp;

	// add any pak files in the format pak0.pak pak1.pak, ...
	qfs_load_gamedir (searchpath, dir);
}

static void
qfs_add_gamedir (vpath_t *vpath, const char *dir)
{
	const char *e;
	const char *s;
	dstring_t  *s_dir;
	dstring_t  *f_dir;

	if (!*dir)
		return;
	e = fs_sharepath->string + strlen (fs_sharepath->string);
	s = e;
	s_dir = dstring_new ();
	f_dir = dstring_new ();

	while (s >= fs_sharepath->string) {
		while (s != fs_sharepath->string && s[-1] !=':')
			s--;
		if (s != e) {
			dsprintf (s_dir, "%.*s", (int) (e - s), s);
			if (strcmp (s_dir->str, fs_userpath->string) != 0) {
				if (qfs_expand_path (f_dir, s_dir->str, dir, 0) != 0) {
					Sys_Printf ("dropping bad directory %s\n", dir);
					break;
				}
				Sys_MaskPrintf (SYS_FS, "qfs_add_gamedir (\"%s\")\n",
								f_dir->str);

				qfs_add_dir (&vpath->share, f_dir->str);
			}
		}
		e = --s;
	}

	qfs_expand_userpath (f_dir, dir);
	Sys_MaskPrintf (SYS_FS, "qfs_add_gamedir (\"%s\")\n", f_dir->str);
	qfs_add_dir (&vpath->user, f_dir->str);

	dstring_delete (f_dir);
	dstring_delete (s_dir);
}

/*
	QFS_Gamedir

	Sets the gamedir and path to a different directory.
*/
VISIBLE void
QFS_Gamedir (const char *gamedir)
{
	int         i;
	const char *list[2] = {gamedir, 0};

	qfs_build_gamedir (list);

	// Make sure everyone else knows we've changed gamedirs
	for (i = 0; i < num_gamedir_callbacks; i++) {
		gamedir_callbacks[i] (0);
	}
	Cache_Flush ();
	for (i = 0; i < num_gamedir_callbacks; i++) {
		gamedir_callbacks[i] (1);
	}
}

VISIBLE void
QFS_GamedirCallback (gamedir_callback_t *func)
{
	if (num_gamedir_callbacks == max_gamedir_callbacks) {
		size_t size = (max_gamedir_callbacks + GAMEDIR_CALLBACK_CHUNK)
					  * sizeof (gamedir_callback_t *);
		gamedir_callbacks = realloc (gamedir_callbacks, size);
		if (!gamedir_callbacks)
			Sys_Error ("Too many gamedir callbacks!\n");
		max_gamedir_callbacks += GAMEDIR_CALLBACK_CHUNK;
	}

	if (!func) {
		Sys_Error ("null gamedir callback\n");
	}

	gamedir_callbacks[num_gamedir_callbacks] = func;
	num_gamedir_callbacks++;
}

static void
qfs_path_cvar (cvar_t *var)
{
	char       *cpath = QFS_CompressPath (var->string);
	if (strcmp (cpath, var->string))
		Cvar_Set (var, cpath);
	free (cpath);
}

static void
qfs_shutdown (void *data)
{
	while (qfs_vpaths) {
		vpath_t    *next = qfs_vpaths->next;
		delete_vpath (qfs_vpaths);
		qfs_vpaths = next;
	}
}

VISIBLE void
QFS_Init (const char *game)
{
	int         i;

	fs_sharepath = Cvar_Get ("fs_sharepath", FS_SHAREPATH, CVAR_ROM,
							 qfs_path_cvar,
							 "location of shared (read-only) game "
							 "directories");
	fs_userpath = Cvar_Get ("fs_userpath", FS_USERPATH, CVAR_ROM,
							qfs_path_cvar,
							"location of your game directories");
	fs_dirconf = Cvar_Get ("fs_dirconf", "", CVAR_ROM, NULL,
							"full path to gamedir.conf FIXME");

	Cmd_AddCommand ("path", qfs_path_f, "Show what paths Quake is using");

	qfs_userpath = Sys_ExpandSquiggle (fs_userpath->string);

	qfs_load_config ();

	qfs_game = game;

	if ((i = COM_CheckParm ("-game")) && i < com_argc - 1) {
		char       *gamedirs = NULL;
		const char **list;
		char       *where;
		int         j, count = 1;

		gamedirs = strdup (com_argv[i + 1]);

		for (j = 0; gamedirs[j]; j++)
			if (gamedirs[j] == ',')
				count++;

		list = calloc (count + 1, sizeof (char *));

		j = 0;
		where = strtok (gamedirs, ",");
		while (where) {
			list[j++] = where;
			where = strtok (NULL, ",");
		}
		qfs_build_gamedir (list);
		free (gamedirs);
		free ((void*)list);
	} else {
		QFS_Gamedir ("");
	}
	Sys_RegisterShutdown (qfs_shutdown, 0);
}

VISIBLE const char *
QFS_SkipPath (const char *pathname)
{
	const char *last;

	// char after last / on the line
	if ((last = strrchr (pathname, '/')))
		last++;
	else
		last = pathname;

	return last;
}

VISIBLE void
QFS_StripExtension (const char *in, char *out)
{
	char       *tmp;

	if (out != in)
		strcpy (out, in);

	tmp = out + (QFS_FileExtension (out) - out);
	*tmp = 0;
}

VISIBLE const char *
QFS_FileExtension (const char *in)
{
	const char *tmp;
	const char *end = in + strlen (in);

	for (tmp = end; tmp != in; tmp--) {
		if (tmp[-1] == '/')
			return end;
		if (tmp[-1] == '.') {
			if (tmp - 1 == in || tmp[-2] == '/')
				return end;
			return tmp - 1;
		}
	}

	return end;
}

VISIBLE void
QFS_DefaultExtension (dstring_t *path, const char *extension)
{
	const char *ext;

	// if path doesn't have a .EXT, append extension
	// (extension should include the .)
	ext = QFS_FileExtension (path->str);
	if (*ext)
		return;						// it has an extension

	dstring_appendstr (path, extension);
}

VISIBLE void
QFS_SetExtension (struct dstring_s *path, const char *extension)
{
	const char *ext = QFS_FileExtension (path->str);
	int         offs = ext - path->str;

	if (*ext) {
		// path has an extension... cut it off
		path->str[offs] = 0;
		path->size = offs + 1;
	}
	dstring_appendstr (path, extension);
}

VISIBLE int
QFS_NextFilename (dstring_t *filename, const char *prefix, const char *ext)
{
	char       *digits;
	int         i;
	int         ret = 0;
	dstring_t  *full_path = dstring_new ();

	dsprintf (filename, "%s0000%s", prefix, ext);
	digits = filename->str + strlen (prefix);

	for (i = 0; i <= 9999; i++) {
		digits[0] = i / 1000 + '0';
		digits[1] = i / 100 % 10 + '0';
		digits[2] = i / 10 % 10 + '0';
		digits[3] = i % 10 + '0';

		if (qfs_expand_userpath (full_path, filename->str) == -1)
			break;
		if (Sys_FileExists (full_path->str) == -1) {
			// file doesn't exist, so we can use this name
			ret = 1;
			break;
		}
	}
	dstring_delete (full_path);
	return ret;
}

VISIBLE QFile *
QFS_Open (const char *path, const char *mode)
{
	dstring_t  *full_path = dstring_new ();
	QFile      *file = 0;
	const char *m;
	int         write = 0;

	if (qfs_expand_userpath (full_path, path) == 0) {
		Sys_MaskPrintf (SYS_FS, "QFS_Open: %s %s\n", full_path->str, mode);
		for (m = mode; *m; m++)
			if (*m == 'w' || *m == '+' || *m == 'a')
				write = 1;
		if (write)
			if (Sys_CreatePath (full_path->str) == -1)
				goto done;
		file = Qopen (full_path->str, mode);
	}
done:
	dstring_delete (full_path);
	return file;
}

VISIBLE QFile *
QFS_WOpen (const char *path, int zip)
{
	char        mode[5] = "wb\000\000\000";

	if (zip) {
		mode[2] = 'z';
		mode[3] = bound (1, zip, 9) + '0';
	}
	return QFS_Open (path, mode);
}

VISIBLE int
QFS_Rename (const char *old_path, const char *new_path)
{
	dstring_t  *full_old = dstring_new ();
	dstring_t  *full_new = dstring_new ();
	int         ret;

	if ((ret = qfs_expand_userpath (full_old, old_path)) != -1)
		if ((ret = qfs_expand_userpath (full_new, new_path)) != -1)
			if ((ret = Sys_CreatePath (full_new->str)) != -1) {
				Sys_MaskPrintf (SYS_FS, "QFS_Rename %s %s\n", full_old->str,
								full_new->str);
				ret = Qrename (full_old->str, full_new->str);
			}
	dstring_delete (full_old);
	dstring_delete (full_new);
	return ret;
}

VISIBLE int
QFS_Remove (const char *path)
{
	dstring_t  *full_path = dstring_new ();
	int         ret;

	if ((ret = qfs_expand_userpath (full_path, path)) != -1)
		ret = Qremove (full_path->str);
	dstring_delete (full_path);
	return ret;
}

VISIBLE filelist_t *
QFS_FilelistNew (void)
{
	return calloc (1, sizeof (filelist_t));
}

VISIBLE void
QFS_FilelistAdd (filelist_t *filelist, const char *fname, const char *ext)
{
	char      **new_list;
	char      *s, *str;

	while ((s = strchr(fname, '/')))
		fname = s + 1;
	if (filelist->count == filelist->size) {
		filelist->size += 32;
		new_list = realloc (filelist->list, filelist->size * sizeof (char *));

		if (!new_list) {
			filelist->size -= 32;
			return;
		}
		filelist->list = new_list;
	}
	str = strdup (fname);

	if (ext && (s = strstr(str, va (0, ".%s", ext))))
		*s = 0;
	filelist->list[filelist->count++] = str;
}

static void
qfs_filelistfill_do (filelist_t *list, const searchpath_t *search, const char *cp, const char *ext, int strip)
{
	const char *separator = "/";

	if (*cp && cp[strlen (cp) - 1] == '/')
		separator = "";

	if (search->pack) {
		int         i;
		pack_t     *pak = search->pack;

		for (i = 0; i < pak->numfiles; i++) {
			char       *name = pak->files[i].name;
			if (!fnmatch (va (0, "%s%s*.%s", cp, separator, ext), name,
						  FNM_PATHNAME)
				|| !fnmatch (va (0, "%s%s*.%s.gz", cp, separator, ext), name,
							 FNM_PATHNAME))
				QFS_FilelistAdd (list, name, strip ? ext : 0);
		}
	} else {
		DIR        *dir_ptr;
		struct dirent *dirent;

		dir_ptr = opendir (va (0, "%s/%s", search->filename, cp));
		if (!dir_ptr)
			return;
		while ((dirent = readdir (dir_ptr)))
			if (!fnmatch (va (0, "*.%s", ext), dirent->d_name, 0)
				|| !fnmatch (va (0, "*.%s.gz", ext), dirent->d_name, 0))
				QFS_FilelistAdd (list, dirent->d_name, strip ? ext : 0);
		closedir (dir_ptr);
	}
}

VISIBLE void
QFS_FilelistFill (filelist_t *list, const char *path, const char *ext,
				  int strip)
{
	vpath_t    *vpath;
	searchpath_t *search;
	char       *cpath, *cp;

	if (strchr (ext, '/') || strchr (ext, '\\'))
		return;

	cp = cpath = QFS_CompressPath (path);

	for (vpath = qfs_vpaths; vpath; vpath = vpath->next) {
		for (search = vpath->user; search; search = search->next) {
			qfs_filelistfill_do (list, search, cp, ext, strip);
		}
	}
	free (cpath);
}

VISIBLE void
QFS_FilelistFree (filelist_t *list)
{
	int         i;

	for (i = 0; i < list->count; i++)
		free (list->list[i]);
	free (list->list);
	free (list);
}
