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

	$Id$
*/
#ifndef __quakeio_h
#define __quakeio_h

#include <stdio.h>

#include "QF/gcc_attr.h"

typedef struct VFile_s VFile;

void Qexpand_squiggle(const char *path, char *dest);
int Qrename(const char *old, const char *new);
VFile *Qopen(const char *path, const char *mode);
VFile *Qdopen(int fd, const char *mode);
void Qclose(VFile *file);
int Qread(VFile *file, void *buf, int count);
int Qwrite(VFile *file, const void *buf, int count);
int Qprintf(VFile *file, const char *fmt, ...) __attribute__((format(printf,2,3)));
char *Qgets(VFile *file, char *buf, int count);
int Qgetc(VFile *file);
int Qputc(VFile *file, int c);
int Qseek(VFile *file, long offset, int whence);
long Qtell(VFile *file);
int Qflush(VFile *file);
int Qeof(VFile *file);
const char *Qgetline(VFile *file);
int Qgetpos(VFile *file, fpos_t *pos);
int Qsetpos(VFile *file, fpos_t *pos);

#endif /*__quakeio_h*/
