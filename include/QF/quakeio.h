/*
	quakeio.h

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
#ifndef __quakeio_h
#define __quakeio_h

#include <stdio.h>

/** \defgroup quakeio File IO
	\ingroup utils
*/
///@{

typedef struct QFile_s QFile;

int Qrename(const char *old_path, const char *new_path);
int Qremove(const char *path);
int Qfilesize (QFile *file) __attribute__((pure));
QFile *Qopen(const char *path, const char *mode);
QFile *Qdopen(int fd, const char *mode);
QFile *Qfopen (FILE *file, const char *mode);
QFile *Qsubopen (const char *path, int offs, int len, int zip);
void Qclose(QFile *file);
int Qread(QFile *file, void *buf, int count);
int Qwrite(QFile *file, const void *buf, int count);
int Qprintf(QFile *file, const char *fmt, ...) __attribute__((format(printf,2,3)));
int Qputs(QFile *file, const char *buf);
char *Qgets(QFile *file, char *buf, int count);
int Qgetc(QFile *file);
int Qputc(QFile *file, int c);
int Qungetc (QFile *file, int c);
int Qseek(QFile *file, long offset, int whence);
long Qtell(QFile *file);
int Qflush(QFile *file);
int Qeof(QFile *file);
const char *Qgetline(QFile *file);

///@}

#endif /*__quakeio_h*/
