/*
	quakeio.c

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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#ifdef HAVE_MALLOC_H
# include <malloc.h>
#endif
#include <stdarg.h>
#include <stdlib.h>
#include <quakeio.h>
#include <string.h>
#ifdef WIN32
# include <io.h>
# include <fcntl.h>
#else
# include <pwd.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef _MSC_VER
# define _POSIX_
#endif
#include <limits.h>

void
Qexpand_squiggle(const char *path, char *dest)
{
	char *home;
#ifndef _WIN32
	struct passwd *pwd_ent;
#endif

	if (strncmp (path, "~/",2) != 0) {
		strcpy (dest,path);
		return;
	}

#ifndef _WIN32
	if ((pwd_ent = getpwuid (getuid()))) {
		home = pwd_ent->pw_dir;
	} else
#endif
		home = getenv("HOME");

	if (home) {
		strcpy (dest, home);
		strcat (dest, path+1); // skip leading ~
	} else
		strcpy (dest,path);
}

int
Qrename(const char *old, const char *new)
{
	char e_old[PATH_MAX];
	char e_new[PATH_MAX];

	Qexpand_squiggle (old, e_old);
	Qexpand_squiggle (new, e_new);
	return rename (e_old, e_new);
}

QFile *
Qopen(const char *path, const char *mode)
{
	QFile *file;
	char m[80],*p;
	int zip=0;
	char e_path[PATH_MAX];

	Qexpand_squiggle (path, e_path);
	path = e_path;

	for (p=m; *mode && p-m<(sizeof(m)-1); mode++) {
		if (*mode=='z') {
			zip=1;
			continue;
		}
		*p++=*mode;
	}
	*p=0;

	file=calloc(sizeof(*file),1);
	if (!file)
		return 0;
#ifdef HAVE_ZLIB
	if (zip) {
		file->gzfile=gzopen(path,m);
		if (!file->gzfile) {
			free(file);
			return 0;
		}
	} else
#endif
	{
		file->file=fopen(path,m);
		if (!file->file) {
			free(file);
			return 0;
		}
	}
	return file;
}

QFile *
Qdopen(int fd, const char *mode)
{
	QFile *file;
	char m[80],*p;
	int zip=0;

	for (p=m; *mode && p-m<(sizeof(m)-1); mode++) {
		if (*mode=='z') {
			zip=1;
			continue;
		}
		*p++=*mode;
	}

	*p=0;

	file=calloc(sizeof(*file),1);
	if (!file)
		return 0;
#ifdef HAVE_ZLIB
	if (zip) {
		file->gzfile=gzdopen(fd,m);
		if (!file->gzfile) {
			free(file);
			return 0;
		}
	} else
#endif
	{
		file->file=fdopen(fd,m);
		if (!file->file) {
			free(file);
			return 0;
		}
	}
#ifdef WIN32
#ifdef __BORLANDC__
	setmode(_fileno(file->file),O_BINARY);
#else
	_setmode(_fileno(file->file),_O_BINARY);
#endif
#endif
	return file;
}

void
Qclose(QFile *file)
{
	if (file->file)
		fclose(file->file);
#ifdef HAVE_ZLIB
	else
		gzclose(file->gzfile);
#endif
	free(file);
}

int
Qread(QFile *file, void *buf, int count)
{
	if (file->file)
		return fread(buf, 1, count, file->file);
#ifdef HAVE_ZLIB
	else
		return gzread(file->gzfile,buf,count);
#else
	return -1;
#endif
}

int
Qwrite (QFile *file, void *buf, int count)
{
	if (file->file)
		return fwrite(buf, 1, count, file->file);
#ifdef HAVE_ZLIB
	else
		return gzwrite(file->gzfile,buf,count);
#else
	return -1;
#endif
}

int
Qprintf(QFile *file, const char *fmt, ...)
{
	va_list args;
	int ret=-1;

	va_start(args,fmt);
	if (file->file)
		ret=vfprintf(file->file, fmt, args);
#ifdef HAVE_ZLIB
	else {
		char buf[4096];
		va_start(args, fmt);
#ifdef HAVE_VSNPRINTF
		(void)vsnprintf(buf, sizeof(buf), fmt, args);
#else
		(void)vsprintf(buf, fmt, args);
#endif
		va_end(args);
		ret = strlen(buf); /* some *sprintf don't return the nb of bytes written */
		if (ret>0)
			ret=gzwrite(file, buf, (unsigned)ret);
	}
#endif
	va_end(args);
	return ret;
}

char *
Qgets(QFile *file, char *buf, int count)
{
	if (file->file)
		return fgets(buf, count, file->file);
#ifdef HAVE_ZLIB
	else
		return gzgets(file->gzfile,buf,count);
#else
	return 0;
#endif
}

int
Qgetc(QFile *file)
{
	if (file->file)
		return fgetc(file->file);
#ifdef HAVE_ZLIB
	else
		return gzgetc(file->gzfile);
#else
	return -1;
#endif
}

int
Qputc(QFile *file, int c)
{
	if (file->file)
		return fputc(c, file->file);
#ifdef HAVE_ZLIB
	else
		return gzputc(file->gzfile,c);
#else
	return -1;
#endif
}

int
Qseek(QFile *file, long offset, int whence)
{
	if (file->file)
		return fseek(file->file, offset, whence);
#ifdef HAVE_ZLIB
	else
		return gzseek(file->gzfile,offset,whence);
#else
	return -1;
#endif
}

long
Qtell(QFile *file)
{
	if (file->file)
		return ftell(file->file);
#ifdef HAVE_ZLIB
	else
		return gztell(file->gzfile);
#else
	return -1;
#endif
}

int
Qflush(QFile *file)
{
	if (file->file)
		return fflush(file->file);
#ifdef HAVE_ZLIB
	else
		return gzflush(file->gzfile,Z_SYNC_FLUSH);
#else
	return -1;
#endif
}

int
Qeof(QFile *file)
{
	if (file->file)
		return feof(file->file);
#ifdef HAVE_ZLIB
	else
		return gzeof(file->gzfile);
#else
	return -1;
#endif
}

int
Qgetpos(QFile *file, fpos_t *pos)
{
#ifdef HAVE_FPOS_T_STRUCT
	pos->__pos = Qtell(file);
	return pos->__pos==-1?-1:0;
#else
	*pos = Qtell(file);
	return *pos==-1?-1:0;
#endif
}

int
Qsetpos(QFile *file, fpos_t *pos)
{
#ifdef HAVE_FPOS_T_STRUCT
	return Qseek(file, pos->__pos, 0);
#else
	return Qseek(file, *pos, 0);
#endif
}
