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

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_ZLIB
# include <zlib.h>
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if defined(_WIN32) && defined(HAVE_MALLOC_H)
#include <malloc.h>
#endif

#ifdef HAVE_IO_H
#include <io.h>
#endif

#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <errno.h>

#include "qfalloca.h"

#include "QF/dstring.h"
#include "QF/qendian.h"
#include "QF/quakefs.h"
#include "QF/quakeio.h"

#ifdef _WIN32
# ifndef __BORLANDC__
#  define setmode _setmode
#  define O_BINARY _O_BINARY
# endif
#endif

#define QF_ZIP	1
#define QF_READ	2

struct QFile_s {
	FILE *file;
#ifdef HAVE_ZLIB
	gzFile gzfile;
#endif
	off_t size;
	off_t start;
	off_t pos;
	int   c;
	int   sub;
};


VISIBLE int
Qrename (const char *old_path, const char *new_path)
{
	return rename (old_path, new_path);
}

VISIBLE int
Qremove (const char *path)
{
	return remove (path);
}

VISIBLE int
Qfilesize (QFile *file)
{
	return file->size;
}

static int
check_file (int fd, int offs, int len, int *zip)
{
	unsigned char id[2], len_bytes[4];

	if (offs < 0 || len < 0) {
		// normal file
		offs = 0;
		len = lseek (fd, 0, SEEK_END);
		lseek (fd, 0, SEEK_SET);
	}
	if (zip && *zip) {
		int         r;

		lseek (fd, offs, SEEK_SET);
		r = read (fd, id, 2);
		if (r == 2 && id[0] == 0x1f && id[1] == 0x8b && len >= 6 &&
			lseek (fd, offs + len - 4, SEEK_SET) >= 0 &&
			read (fd, len_bytes, 4) == 4) {
			len = ((len_bytes[3] << 24)
				   | (len_bytes[2] << 16)
				   | (len_bytes[1] << 8)
				   | (len_bytes[0]));
		} else {
			*zip = 0;
		}
	}
	lseek (fd, offs, SEEK_SET);
	return len;
}

static int
file_mode (const char *mode, char *out)
{
	int         flags = 0;
	char       *p;

	for (p = out; *mode; mode++) {
		if (*mode == 'z') {
			flags |= QF_ZIP;
			continue;
		}
		if (*mode == 'r' || ((*mode == 'w' || *mode == 'a') && *mode == '+')) {
			flags |= QF_READ;
		}
#ifndef HAVE_ZLIB
		if (strchr ("0123456789fh", *mode)) {
			continue;
		}
#endif
		if (p)
			*p++ = *mode;
	}
	if (p)
		*p = 0;
	return flags;
}

VISIBLE QFile *
Qopen (const char *path, const char *mode)
{
	QFile      *file;
	char       *m;
	int         flags, reading, zip;
	int         size = -1;

	m = alloca (strlen (mode) + 1);
	flags = file_mode (mode, m);

	zip = flags & QF_ZIP;
	reading = flags & QF_READ;

	if (reading) {
		int         fd = open (path, O_RDONLY);
		if (fd != -1) {
			size = check_file (fd, -1, -1, &zip);
			close (fd);
		}
	}

	file = calloc (sizeof (*file), 1);
	if (!file)
		return 0;
	file->size = size;
#ifdef HAVE_ZLIB
	if (zip) {
		file->gzfile = gzopen (path, m);
		if (!file->gzfile) {
			free (file);
			return 0;
		}
	} else
#endif
	{
		file->file = fopen (path, m);
		if (!file->file) {
			free (file);
			return 0;
		}
	}
	file->c = -1;
	return file;
}

VISIBLE QFile *
Qdopen (int fd, const char *mode)
{
	QFile      *file;
	char       *m, *p;
#ifdef HAVE_ZLIB
	int         zip = 0;
#endif
	int         len = strlen (mode);

	m = alloca (len + 1);
#ifdef _WIN32
	setmode (fd, O_BINARY);
#endif
	for (p = m; *mode && p - m < len; mode++) {
		if (*mode == 'z') {
#ifdef HAVE_ZLIB
			zip = 1;
#endif
			continue;
		}
		*p++ = *mode;
	}

	*p = 0;

	file = calloc (sizeof (*file), 1);
	if (!file)
		return 0;
#ifdef HAVE_ZLIB
	if (zip) {
		file->gzfile = gzdopen (fd, m);
		if (!file->gzfile) {
			free (file);
			return 0;
		}
	} else
#endif
	{
		file->file = fdopen (fd, m);
		if (!file->file) {
			free (file);
			return 0;
		}
	}
	file->c = -1;
	return file;
}

VISIBLE QFile *
Qfopen (FILE *file, const char *mode)
{
	QFile      *qfile;
	int         flags = file_mode (mode, 0);

	if (!file)
		return 0;
	qfile = calloc (sizeof (*qfile), 1);
	if (!qfile)
		return 0;
	qfile->file = file;
	if (flags & QF_READ)
		qfile->size = check_file (fileno (file), -1, -1, 0);
	qfile->c = -1;
	return qfile;
}

VISIBLE QFile *
Qsubopen (const char *path, int offs, int len, int zip)
{
	int         fd = open (path, O_RDONLY);
	QFile      *file;

	if (fd == -1)
		return 0;
#ifdef _WIN32
	setmode (fd, O_BINARY);
#endif

	len = check_file (fd, offs, len, &zip);
	file = Qdopen (fd, zip ? "rbz" : "rb");
	file->size = len;
	file->start = offs;
	file->sub = 1;
	return file;
}

VISIBLE void
Qclose (QFile *file)
{
	if (file->file)
		fclose (file->file);
#ifdef HAVE_ZLIB
	else
		gzclose (file->gzfile);
#endif
	free (file);
}

VISIBLE int
Qread (QFile *file, void *buf, int count)
{
	int         offs = 0;
	int         ret;

	if (file->c != -1) {
		char       *b = buf;
		*b++ = file->c;
		buf = b;
		offs = 1;
		file->c = -1;
		count--;
		if (!count)
			return 1;
	}
	if (file->sub) {
		// sub-files are always opened in binary mode, so we don't need to
		// worry about character translation messing up count/pos. Normal
		// files can be left to the operating system to take care of EOF.
		if (file->pos + count > file->size)
			count = file->size - file->pos;
		if (count < 0)
			return -1;
		if (!count)
			return 0;
	}
	if (file->file)
		ret = fread (buf, 1, count, file->file);
	else
#ifdef HAVE_ZLIB
		ret = gzread (file->gzfile, buf, count);
#else
		return -1;
#endif
	if (file->sub)
		file->pos += ret;
	return ret == -1 ? ret : ret + offs;
}

VISIBLE int
Qwrite (QFile *file, const void *buf, int count)
{
	if (file->sub)		// can't write to a sub-file
		return -1;
	if (file->file)
		return fwrite (buf, 1, count, file->file);
#ifdef HAVE_ZLIB
	else
		return gzwrite (file->gzfile, (const voidp)buf, count);
#else
	return -1;
#endif
}

VISIBLE int
Qprintf (QFile *file, const char *fmt, ...)
{
	va_list     args;
	int         ret = -1;

	if (file->sub)		// can't write to a sub-file
		return -1;
	va_start (args, fmt);
	if (file->file)
		ret = vfprintf (file->file, fmt, args);
#ifdef HAVE_ZLIB
	else {
		static dstring_t *buf;

		if (!buf)
			buf = dstring_new ();

		dvsprintf (buf, fmt, args);
		ret = strlen (buf->str);
		if (ret > 0)
			ret = gzwrite (file->gzfile, buf, (unsigned) ret);
	}
#endif
	va_end (args);
	return ret;
}

VISIBLE int
Qputs (QFile *file, const char *buf)
{
	if (file->sub)		// can't write to a sub-file
		return -1;
	if (file->file)
		return fputs (buf, file->file);
#ifdef HAVE_ZLIB
	else
		return gzputs (file->gzfile, buf);
#else
	return 0;
#endif
}

VISIBLE char *
Qgets (QFile *file, char *buf, int count)
{
	char       *ret = buf;
	char        c;

	while (buf - ret < count - 1) {
		c = Qgetc (file);
		if (c < 0)
			break;
		*buf++ = c;
		if (c == '\n')
			break;
	}
	if (buf == ret)
		return 0;

	*buf++ = 0;
	return ret;
}

VISIBLE int
Qgetc (QFile *file)
{
	if (file->c != -1) {
		int         c = file->c;
		file->c = -1;
		return c;
	}
	if (file->sub) {
		if (file->pos >= file->size)
			return EOF;
		file->pos++;
	}
	if (file->file)
		return fgetc (file->file);
#ifdef HAVE_ZLIB
	else
		return gzgetc (file->gzfile);
#else
	return -1;
#endif
}

VISIBLE int
Qputc (QFile *file, int c)
{
	if (file->sub)		// can't write to a sub-file
		return -1;
	if (file->file)
		return fputc (c, file->file);
#ifdef HAVE_ZLIB
	else
		return gzputc (file->gzfile, c);
#else
	return -1;
#endif
}

VISIBLE int
Qungetc (QFile *file, int c)
{
	if (file->c == -1)
		file->c = (byte) c;
	return c;
}

VISIBLE int
Qseek (QFile *file, long offset, int whence)
{
	int         res;

	file->c = -1;
	if (file->file) {
		switch (whence) {
			case SEEK_SET:
				res = fseek (file->file, file->start + offset, whence);
				break;
			case SEEK_CUR:
				res = fseek (file->file, offset, whence);
				break;
			case SEEK_END:
				if (file->size == -1) {
					// we don't know the size (due to writing) so punt and
					// pass on the request as-is
					res = fseek (file->file, offset, SEEK_END);
				} else {
					res = fseek (file->file,
								 file->start + file->size - offset, SEEK_SET);
				}
				break;
			default:
				errno = EINVAL;
				return -1;
		}
		if (res != -1)
			res = ftell (file->file) - file->start;
		if (file->sub)
			file->pos = res;
		return res;
	}
#ifdef HAVE_ZLIB
	else {
		// libz seems to keep track of the true start position itself
		// doesn't support SEEK_END, though
		res = gzseek (file->gzfile, offset, whence);
		if (file->sub)
			file->pos = res;
		return res;
	}
#else
	return -1;
#endif
}

VISIBLE long
Qtell (QFile *file)
{
	int         offs;
	int         ret;

	offs =  (file->c != -1) ? 1 : 0;
	if (file->file)
		ret = ftell (file->file) - file->start;
	else
#ifdef HAVE_ZLIB
		ret = gztell (file->gzfile);	//FIXME does gztell do the right thing?
#else
		return -1;
#endif
	if (file->sub)
		file->pos = ret;
	return ret == -1 ? ret : ret - offs;
}

VISIBLE int
Qflush (QFile *file)
{
	if (file->file)
		return fflush (file->file);
#ifdef HAVE_ZLIB
	else
		return gzflush (file->gzfile, Z_SYNC_FLUSH);
#else
	return -1;
#endif
}

VISIBLE int
Qeof (QFile *file)
{
	if (file->c != -1)
		return 0;
	if (file->sub)
		return file->pos >= file->size;
	if (file->file)
		return feof (file->file);
#ifdef HAVE_ZLIB
	else
		return gzeof (file->gzfile);
#else
	return -1;
#endif
}

/*
	Qgetline

	Dynamic length version of Qgets. Do NOT free the buffer.
*/
VISIBLE const char *
Qgetline (QFile *file)
{
	static int  size = 256;
	static char *buf = 0;
	int         len;

	if (!buf) {
		buf = malloc (size);
		if (!buf)
			return 0;
	}

	if (!Qgets (file, buf, size))
		return 0;

	len = strlen (buf);
	while (len && buf[len - 1] != '\n') {
		char       *t = realloc (buf, size + 256);

		if (!t)
			return 0;
		buf = t;
		size += 256;
		if (!Qgets (file, buf + len, size - len))
			break;
		len = strlen (buf);
	}
	return buf;
}
