/*
	riff.h

	riff wav file handling

	Copyright (C) 2003 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2003/4/10

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

#ifndef __QF_riff_h
#define __QF_riff_h

#include "QF/quakeio.h"

typedef struct d_chunk_s {
	unsigned char name[4];
	unsigned    len;
} d_chunk_t;

typedef struct d_cue_point_s {
	unsigned    name;
	unsigned    position;
	char        chunk[4];
	unsigned    chunk_start;
	unsigned    block_start;
	unsigned    sample_offset;
} d_cue_point_t;

typedef struct d_cue_s {
	unsigned    count;
	d_cue_point_t cue_points[1];
} d_cue_t;

typedef struct d_format_s {
	unsigned short format_tag;
	unsigned short channels;
	unsigned    samples_pre_sec;
	unsigned    byte_per_sec;
	unsigned short align;
} d_format_t;

typedef struct d_ltxt_s {
	unsigned    name;
	unsigned    len;
	char        purpose[4];
	unsigned    country;
	unsigned    language;
	unsigned    dialect;
	unsigned    codepage;
	unsigned char data[0];
} d_ltxt_t;

typedef struct cue_s {
	d_chunk_t   ck;
	d_cue_t    *cue;
} cue_t;

typedef struct format_s {
	d_chunk_t   ck;
	d_format_t  format;
	char        fdata[0];
} format_t;

typedef struct ltxt_s {
	d_chunk_t   ck;
	d_ltxt_t    ltxt;
} ltxt_t;

typedef struct label_s {
	d_chunk_t   ck;
	unsigned    ofs;
	char       *label;
} label_t;

typedef struct data_s {
	d_chunk_t   ck;
	char       *data;
} data_t;

typedef struct list_s {
	d_chunk_t   ck;
	char        name[4];
	d_chunk_t  *chunks[0];
} list_t;

#define SWITCH(name) switch (((name)[0] << 24) | ((name)[1] << 16) \
							 | ((name)[2] << 8) | (name)[3])
#define CASE(a,b,c,d) (((unsigned char)(a) << 24) \
					   | ((unsigned char)(b) << 16) \
					   | ((unsigned char)(c) << 8) \
					   | (unsigned char)(d))

list_t *riff_read (QFile *file);
void riff_free (list_t *riff);

#endif//__QF_riff_h
