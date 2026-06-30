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
typedef struct qio_funcs_s {
	size_t    (*read)  (QFile *file, void *buf, size_t count);
	size_t    (*write) (QFile *file, const void *buf, size_t count);
	off_t     (*seek)  (QFile *file, off_t offset, int whence);
	long      (*tell)  (QFile *file);
	int       (*flush) (QFile *file);
	int       (*eof)   (QFile *file);

	void      (*close) (QFile *file);
} qio_funcs_t;

#ifdef HAVE_ZLIB
typedef struct qio_gzsub_s {
	gzFile      gzfile;
	size_t      pos;
} qio_gzsub_t;
#endif

typedef struct qio_sub_s {
	int         fd;
	size_t      start;
	size_t      pos;
} qio_sub_t;

struct QFile_s {
	void       *file;
	qio_funcs_t funcs;
	dstring_t  *buf;
	size_t      size;
	int         c;
};

static size_t
qio_FILE_read (QFile *file, void *buf, size_t count)
{
	return fread (buf, 1, count, file->file);
}

static size_t
qio_FILE_write (QFile *file, const void *buf, size_t count)
{
	return fwrite (buf, 1, count, file->file);
}

static off_t
qio_FILE_seek (QFile *file, off_t offset, int whence)
{
	return fseek (file->file, offset, whence);
}

static long
qio_FILE_tell (QFile *file)
{
	return ftell (file->file);
}

static int
qio_FILE_flush (QFile *file)
{
	return fflush (file->file);
}

static int
qio_FILE_eof (QFile *file)
{
	return feof (file->file);
}

static void
qio_FILE_close (QFile *file)
{
	fclose (file->file);
}

static qio_funcs_t qio_FILE_funcs = {
	.read  = qio_FILE_read,
	.write = qio_FILE_write,
	.seek  = qio_FILE_seek,
	.tell  = qio_FILE_tell,
	.flush = qio_FILE_flush,
	.eof   = qio_FILE_eof,

	.close = qio_FILE_close,
};

static size_t
qio_sub_FILE_read (QFile *file, void *buf, size_t count)
{
	qio_sub_t  *sub = file->file;
	if (sub->pos > file->size) {
		// shouldn't happen, but...
		return 0;
	}
	if (count > file->size - sub->pos) {
		count = file->size - sub->pos;
	}

	// sub-files are always opened in binary mode, so we don't need to
	// worry about character translation messing up count/pos. Normal
	// files can be left to the operating system to take care of EOF.
	ssize_t ret = read (sub->fd, buf, count);
	if (ret >= 0) {
		sub->pos += ret;
	}
	return ret;
}

static off_t
qio_sub_FILE_seek (QFile *file, off_t offset, int whence)
{
	qio_sub_t  *sub = file->file;
	switch (whence) {
		case SEEK_SET:
			sub->pos = offset;
			break;
		case SEEK_CUR:
			sub->pos += offset;
			break;
		case SEEK_END:
			sub->pos = file->size + offset;
			break;
	}
	if (sub->pos > file->size) {
		sub->pos = file->size;
	}
	off_t pos = lseek (sub->fd, sub->start + sub->pos, SEEK_SET);
	if (pos >= 0) {
		return pos - sub->start;
	}
	return pos;
}

static long
qio_sub_FILE_tell (QFile *file)
{
	qio_sub_t  *sub = file->file;
	return sub->pos;
}

static int
qio_sub_FILE_flush (QFile *file)
{
	return 0;
}

static int
qio_sub_FILE_eof (QFile *file)
{
	qio_sub_t  *sub = file->file;
	return sub->pos >= file->size;
}

static void
qio_sub_FILE_close (QFile *file)
{
	qio_sub_t  *sub = file->file;
	close (sub->fd);
}

static qio_funcs_t qio_sub_FILE_funcs = {
	.read  = qio_sub_FILE_read,
	.seek  = qio_sub_FILE_seek,
	.tell  = qio_sub_FILE_tell,
	.flush = qio_sub_FILE_flush,
	.eof   = qio_sub_FILE_eof,

	.close = qio_sub_FILE_close,
};

#ifdef HAVE_ZLIB
static size_t
qio_gzip_read (QFile *file, void *buf, size_t count)
{
	return gzread (file->file, buf, count);
}

static size_t
qio_gzip_write (QFile *file, const void *buf, size_t count)
{
	return gzwrite (file->file, (const voidp) buf, count);
}

static off_t
qio_gzip_seek (QFile *file, off_t offset, int whence)
{
	if (whence == SEEK_END) {
		// libz does not support SEEK_END
		return EINVAL;
	}
	return gzseek (file->file, offset, whence);
}

static long
qio_gzip_tell (QFile *file)
{
	return ftell (file->file);
}

static int
qio_gzip_flush (QFile *file)
{
	return fflush (file->file);
}

static int
qio_gzip_eof (QFile *file)
{
	return feof (file->file);
}

static void
qio_gzip_close (QFile *file)
{
	gzclose (file->file);
}

static qio_funcs_t qio_gzip_funcs = {
	.read  = qio_gzip_read,
	.write = qio_gzip_write,
	.seek  = qio_gzip_seek,
	.tell  = qio_gzip_tell,
	.flush = qio_gzip_flush,
	.eof   = qio_gzip_eof,

	.close = qio_gzip_close,
};

static size_t
qio_gzsub_FILE_read (QFile *file, void *buf, size_t count)
{
	qio_gzsub_t *sub = file->file;
	if (sub->pos > file->size) {
		// shouldn't happen, but...
		return 0;
	}
	if (count > file->size - sub->pos) {
		count = file->size - sub->pos;
	}

	// sub-files are always opened in binary mode, so we don't need to
	// worry about character translation messing up count/pos. Normal
	// files can be left to the operating system to take care of EOF.
	ssize_t ret = gzread (sub->gzfile, buf, count);
	if (ret >= 0) {
		sub->pos += ret;
	}
	return ret;
}

static off_t
qio_gzsub_FILE_seek (QFile *file, off_t offset, int whence)
{
	qio_gzsub_t *sub = file->file;
	switch (whence) {
		case SEEK_SET:
			sub->pos = offset;
			break;
		case SEEK_CUR:
			sub->pos += offset;
			break;
		case SEEK_END:
			sub->pos = file->size + offset;
			break;
	}
	if (sub->pos > file->size) {
		sub->pos = file->size;
	}
	off_t pos = gzseek (sub->gzfile, sub->pos, SEEK_SET);
	if (pos >= 0) {
		return pos;
	}
	return pos;
}

static long
qio_gzsub_FILE_tell (QFile *file)
{
	qio_gzsub_t *sub = file->file;
	return sub->pos;
}

static int
qio_gzsub_FILE_flush (QFile *file)
{
	return 0;
}

static int
qio_gzsub_FILE_eof (QFile *file)
{
	qio_gzsub_t *sub = file->file;
	return sub->pos >= file->size;
}

static void
qio_gzsub_FILE_close (QFile *file)
{
	qio_gzsub_t *sub = file->file;
	gzclose (sub->gzfile);
}

static qio_funcs_t qio_gzsub_FILE_funcs = {
	.read  = qio_gzsub_FILE_read,
	.seek  = qio_gzsub_FILE_seek,
	.tell  = qio_gzsub_FILE_tell,
	.flush = qio_gzsub_FILE_flush,
	.eof   = qio_gzsub_FILE_eof,

	.close = qio_gzsub_FILE_close,
};
#endif

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

VISIBLE size_t
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
		return nullptr;
	file->size = size;
#ifdef HAVE_ZLIB
	if (zip) {
		file->file = gzopen (path, m);
		if (!file->file) {
			free (file);
			return nullptr;
		}
		file->funcs = qio_gzip_funcs;
	} else
#endif
	{
		file->file = fopen (path, m);
		if (!file->file) {
			free (file);
			return nullptr;
		}
		file->funcs = qio_FILE_funcs;
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
		file->file = gzdopen (fd, m);
		if (!file->file) {
			free (file);
			return 0;
		}
		file->funcs = qio_gzip_funcs;
	} else
#endif
	{
		file->file = fdopen (fd, m);
		if (!file->file) {
			free (file);
			return 0;
		}
		file->funcs = qio_FILE_funcs;
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

static QFile *
qio_sub_gzip_open (int fd, int offs, int len)
{
	QFile *file = malloc (sizeof (QFile) + sizeof (qio_sub_t));
	auto sub = (qio_gzsub_t *) &file[1];
	*file = (QFile) {
		.file = sub,
		.funcs = qio_gzsub_FILE_funcs,
		.size = len,
		.c = -1,
	};
	*sub = (qio_gzsub_t) {
		.gzfile = gzdopen (fd, "rb"),
		.pos = 0,
	};
	return file;
}

static QFile *
qio_sub_FILE_open (int fd, int offs, int len)
{
	QFile *file = malloc (sizeof (QFile) + sizeof (qio_sub_t));
	auto sub = (qio_sub_t *) &file[1];
	*file = (QFile) {
		.file = sub,
		.funcs = qio_sub_FILE_funcs,
		.size = len,
		.c = -1,
	};
	*sub = (qio_sub_t) {
		.fd = fd,
		.start = offs,
		.pos = 0,
	};
	return file;
}

VISIBLE QFile *
Qsubopen (const char *path, int offs, int len, int zip)
{
	int         fd = open (path, O_RDONLY);

	if (fd == -1)
		return 0;
#ifdef _WIN32
	setmode (fd, O_BINARY);
#endif

	len = check_file (fd, offs, len, &zip);

	if (zip) {
		return qio_sub_gzip_open (fd, offs, len);
	} else {
		return qio_sub_FILE_open (fd, offs, len);
	}
}

VISIBLE void
Qclose (QFile *file)
{
	if (file->buf) {
		dstring_delete (file->buf);
	}
	file->funcs.close (file);
	free (file);
}

VISIBLE size_t
Qread (QFile *file, void *buf, size_t count)
{
	int         offs = 0;
	if (file->c != -1) {
		char       *b = buf;
		*b++ = file->c;
		buf = b;
		offs = 1;
		file->c = -1;
		count--;
		if (!count) {
			return 1;
		}
	}

	int         ret = file->funcs.read (file, buf, count);
	return ret < 0 ? ret : ret + offs;
}

VISIBLE size_t
Qwrite (QFile *file, const void *buf, size_t count)
{
	if (!file->funcs.write) {
		return 0;
	}
	return file->funcs.write (file, buf, count);
}

VISIBLE int
Qprintf (QFile *file, const char *fmt, ...)
{
	if (!file->funcs.write) {
		return -1;
	}

	if (!file->buf) {
		file->buf = dstring_new ();
	}

	va_list     args;
	va_start (args, fmt);
	dvsprintf (file->buf, fmt, args);
	va_end (args);

	if (file->buf->size > 1) {
		return Qwrite (file, file->buf->str, file->buf->size - 1);
	}
	return 0;
}

VISIBLE int
Qputs (QFile *file, const char *buf)
{
	if (!file->funcs.write) {
		return -1;
	}

	size_t len = strlen (buf);
	return Qwrite (file, buf, len);
}

VISIBLE char *
Qgets (QFile *file, char *buf, int count)
{
	char       *ret = buf;
	int         c;

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
	byte        c;
	size_t ret = Qread (file, &c, 1);
	if (ret < 1) {
		return EOF;
	}
	return c;
}

VISIBLE int
Qputc (QFile *file, int c)
{
	byte        b = c;
	return Qwrite (file, &b, 1);
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
	file->c = -1;
	return file->funcs.seek (file, offset, whence);
}

VISIBLE long
Qtell (QFile *file)
{
	int offs = (file->c != -1) ? 1 : 0;
	long ret = file->funcs.tell (file);
	return ret == -1 ? ret : ret - offs;
}

VISIBLE int
Qflush (QFile *file)
{
	file->c = -1;
	return file->funcs.flush (file);
}

VISIBLE int
Qeof (QFile *file)
{
	if (file->c != -1) {
		return 0;
	}
	return file->funcs.eof (file);
}

/*
	Qgetline

	Dynamic length version of Qgets.
*/
VISIBLE char *
Qgetline (QFile *file)
{
	int         len;

	if (!file->buf) {
		file->buf = dstring_new ();
	}
	dstring_t  *buf = file->buf;

	buf->size = 256 < buf->truesize ? buf->truesize : 256;
	dstring_adjust (buf);

	if (!Qgets (file, buf->str, buf->size)) {
		return 0;
	}

	len = strlen (buf->str);
	while (len && buf->str[len - 1] != '\n') {
		buf->size = buf->truesize + 256;
		dstring_adjust (buf);
		if (!Qgets (file, buf->str, buf->size)) {
			break;
		}
		len = strlen (buf->str);
	}
	buf->size = len + 1;
	return buf->str;
}
