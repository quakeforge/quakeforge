/*
	sv_crudefile.c

	(description)

	Copyright (C) 2001  Adam Olsen

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
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_CTYPE_H
# include <ctype.h>
#endif

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/zone.h"

#include "compat.h"

#include "qw/include/crudefile.h"

int cf_maxsize; // max combined file size (eg quota)
int cf_cursize; // current combined file size

typedef struct cf_file_s {
	QFile *file;
	char *path;
	char *buf;
	int size;
	int writtento;
	char mode; // 'r' for read, 'w' for write, 'a' for append
} cf_file_t;

cf_file_t *cf_filep;
cvar_t    *crudefile_quota;
int        cf_filepcount; // elements in array
int        cf_openfiles; // used elements

#define CF_DIR "cf/"
#define CF_MAXFILES 100
#define CF_BUFFERSIZE 256


/*
	CF_ValidDesc

	Returns 1 if the file descriptor is valid.
*/
static int
CF_ValidDesc (int desc)
{
	if (desc >= 0 && desc < cf_filepcount && cf_filep[desc].file)
		return 1;
	return 0;
}

/*
	CF_AlreadyOpen

	Returns 1 if mode == 'r' and the file is already open for
	writing, or if if mode == 'w' or 'a' and the file's already open at all.
*/
static int
CF_AlreadyOpen (const char * path, char mode)
{
	int i;

	for (i = 0; i < cf_filepcount; i++) {
		if (!cf_filep[i].file)
			continue;
		if (mode == 'r' && (cf_filep[i].mode == 'w' || cf_filep[i].mode == 'a') &&
			strequal (path, cf_filep[i].path))
			return 1;
		if ((mode == 'w' || mode == 'a')&& strequal (path, cf_filep[i].path))
			return 1;
	}
	return 0;
}

/*
	CF_GetFileSize

	Returns the size of the specified file
*/
static int
CF_GetFileSize (const char *path)
{
	struct stat buf;

	if (!stat (path, &buf))
		return 0;

	return buf.st_size;
}

/*
	CF_BuildQuota

	Calculates the currently used space
*/
static void
CF_BuildQuota (void)
{
	static dstring_t *path;
	struct dirent *i;
	DIR *dir;

	if (!path)
		path = dstring_new ();
	dsprintf (path, "%s/%s/%s", qfs_userpath, qfs_gamedir->dir.def, CF_DIR);

	dir = opendir (path->str);
	if (!dir)
		return;

	cf_cursize = 0;

	while ((i = readdir (dir))) {
		cf_cursize += CF_GetFileSize (va ("%s/%s", path->str, i->d_name));
	}
	closedir (dir);
}

/*
	CF_Init

	Ye ol' Init function :)
*/
void
CF_Init (void)
{
	CF_BuildQuota ();
	crudefile_quota = Cvar_Get ("crudefile_quota", "-1", CVAR_ROM, NULL,
								"Maximum space available to the Crude File "
								"system, -1 to totally disable file writing");
	cf_maxsize = crudefile_quota->int_val;
}

/*
	CF_CloseAllFiles

	Closes all open files, printing warnings if developer is on
*/
void
CF_CloseAllFiles ()
{
	int i;

	for (i = 0; i < cf_filepcount; i++)
		if (cf_filep[i].file) {
			Sys_MaskPrintf (SYS_DEV, "Warning: closing Crude File %d left "
							"over from last map\n", i);
			CF_Close (i);
		}
}

/*
	CF_Open

	cfopen opens a file, either for reading or writing (not both).
	returns a file descriptor >= 0 on success, < 0 on failure.
	mode is either r or w.
*/
int
CF_Open (const char *path, const char *mode)
{
	char *j;
	dstring_t *fullpath = dstring_new ();
	int desc, oldsize, i;
	QFile *file;

	if (cf_openfiles >= CF_MAXFILES) {
		return -1;
	}

	// check for paths with ..
	if (strequal (path, "..")
		|| !strncmp (path, "../", 3)
		|| strstr (path, "/../")
		|| (strlen (path) >= 3
			&& strequal (path + strlen (path) - 3, "/.."))) {
		return -1;
	}

	if (!(strequal(mode, "w") || strequal(mode, "r") || strequal(mode, "a"))) {
		return -1;
	}

	if (mode[0] == 'w' && cf_maxsize < 0) { // can't even delete if quota < 0
		return -1;
	}

	dsprintf (fullpath, "%s/%s/%s", qfs_gamedir->dir.def, CF_DIR, path);

	j = fullpath->str + strlen (fullpath->str) - strlen (path);
	for (i = 0; path[i]; i++, j++) // strcpy, but force lowercase
		*j = tolower ((byte) path[i]);
	*j = '\0';

	if (CF_AlreadyOpen (fullpath->str, mode[0])) {
		dstring_delete (fullpath);
		return -1;
	}

	if (mode[0] == 'w')
		oldsize = CF_GetFileSize (fullpath->str);
	else
		oldsize = 0;

	file = QFS_Open (fullpath->str, mode);
	if (file) {
		if (cf_openfiles >= cf_filepcount) {
			cf_filepcount++;
			cf_filep = realloc (cf_filep, sizeof (cf_file_t) * cf_filepcount);
			if (!cf_filep) {
				Sys_Error ("CF_Open: memory allocation error!");
			}
			cf_filep[cf_filepcount - 1].file = 0;
		}

		for (desc = 0; cf_filep[desc].file; desc++)
			;
		cf_filep[desc].path = fullpath->str;
		cf_filep[desc].file = file;
		cf_filep[desc].buf = 0;
		cf_filep[desc].size = 0;
		cf_filep[desc].writtento = 0;
		cf_filep[desc].mode = mode[0];

		cf_cursize -= oldsize;
		cf_openfiles++;

		return desc;
	}
	return -1;
}

/*
	CF_Close

	cfclose closes a file descriptor.  returns nothing.  to prevent
	leakage, all open files are closed on map load, and warnings are
	printed if developer is set to 1.
*/
void
CF_Close (int desc)
{
	char *path;

	if (!CF_ValidDesc (desc))
		return;

	if (cf_filep[desc].mode == 'w' && !cf_filep[desc].writtento)
		unlink (cf_filep[desc].path);

	path = cf_filep[desc].path;

	Qclose (cf_filep[desc].file);
	cf_filep[desc].file = 0;
	free (cf_filep[desc].buf);
	cf_openfiles--;

	cf_cursize -= CF_GetFileSize (path);
	free (path);
}

/*
	CF_Read

	cfread returns a single string read in from the file.  an empty
	string either means an empty string or eof, use cfeof to check.
*/
const char *
CF_Read (int desc)
{
	int len = 0;

	if (!CF_ValidDesc (desc) || cf_filep[desc].mode != 'r') {
		return "";
	}

	do {
		int foo;
		if (cf_filep[desc].size <= len) {
			char *t = realloc (cf_filep[desc].buf, cf_filep[desc].size +
							   CF_BUFFERSIZE);
			if (!t) {
				Sys_Error ("CF_Read: memory allocation error!");
			}
			cf_filep[desc].buf = t;
			cf_filep[desc].size += CF_BUFFERSIZE;
		}
		foo = Qgetc (cf_filep[desc].file);
		if (foo == EOF)
			foo = 0;
		cf_filep[desc].buf[len] = (char) foo;
		len++;
	} while (cf_filep[desc].buf[len - 1]);

	return cf_filep[desc].buf;
}

/*
	CF_Write

	cfwrite writes a string to the file, including a trailing nul,
	returning the number of characters written.  It returns 0 if
	there was an error in writing, such as if the quota would have
	been exceeded.
*/
int
CF_Write (int desc, const char *buf) // should be const char *, but Qwrite isn't...
{
	int len;

	if (!CF_ValidDesc (desc) || !(cf_filep[desc].mode == 'w' || cf_filep[desc].mode == 'a') || cf_cursize >=
		cf_maxsize) {
		return 0;
	}

	len = strlen (buf) + 1;
	if (len > cf_maxsize - cf_cursize) {
		return 0;
	}

	len = Qwrite (cf_filep[desc].file, buf, len);
	if (len < 0)
		len = 0;

	cf_cursize += len;
	cf_filep[desc].writtento = 1;

	return len;
}

/*
	CF_EOF

	cfeof returns 1 if you're at the end of the file, 0 if not, and
	-1 on a bad descriptor.
*/
int
CF_EOF (int desc)
{
	if (!CF_ValidDesc (desc)) {
		return -1;
	}

	return Qeof (cf_filep[desc].file);
}

/*
	CF_Quota

	returns the number of characters left in the quota, or <= 0 if it's met or
	(somehow) exceeded.
*/
int
CF_Quota ()
{
	return cf_maxsize - cf_cursize;
}
