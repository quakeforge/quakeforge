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

*/

#ifndef __QF_riff_h
#define __QF_riff_h

#include <stdint.h>
#include <sys/types.h>

#include "QF/quakeio.h"

typedef struct riff_d_chunk_s {
	char        name[4];
	uint32_t    len;
} riff_d_chunk_t;

typedef struct riff_d_cue_point_s {
	uint32_t    name;
	uint32_t    position;
	char        chunk[4];
	uint32_t    chunk_start;
	uint32_t    block_start;
	uint32_t    sample_offset;
} riff_d_cue_point_t;

typedef struct riff_d_cue_s {
	uint32_t    count;
	riff_d_cue_point_t cue_points[1];
} riff_d_cue_t;

typedef struct riff_d_format_s {
	uint16_t    format_tag;
	uint16_t    channels;
	uint32_t    samples_per_sec;
	uint32_t    bytes_per_sec;
	uint16_t    align;
	uint16_t    bits_per_sample;	// only if format_tag == 1
} riff_d_format_t;

typedef struct riff_d_ltxt_s {
	uint32_t    name;
	uint32_t    len;
	char        purpose[4];
	uint32_t    country;
	uint32_t    language;
	uint32_t    dialect;
	uint32_t    codepage;
	unsigned char data[0];
} riff_d_ltxt_t;

typedef struct riff_cue_s {
	riff_d_chunk_t   ck;
	riff_d_cue_t    *cue;
} riff_cue_t;

typedef struct riff_format_s {
	riff_d_chunk_t   ck;
	riff_d_format_t  format;
	char        fdata[0];
} riff_format_t;

typedef struct riff_ltxt_s {
	riff_d_chunk_t   ck;
	riff_d_ltxt_t    ltxt;
} riff_ltxt_t;

typedef struct riff_label_s {
	riff_d_chunk_t   ck;
	off_t       ofs;
	char       *label;
} riff_label_t;

typedef struct riff_data_s {
	riff_d_chunk_t   ck;
	char       *data;
} riff_data_t;

typedef struct riff_list_s {
	riff_d_chunk_t   ck;
	char        name[4];
	riff_d_chunk_t  *chunks[0];
} riff_list_t;
typedef riff_list_t riff_t;	// a riff file is one huge list chunk

#define RIFF_SWITCH(name) switch ((((unsigned char) (name)[0]) << 24) \
								  | (((unsigned char) (name)[1]) << 16) \
								  | (((unsigned char) (name)[2]) << 8) \
								  | ((unsigned char) (name)[3]))
#define RIFF_CASE(a,b,c,d) (((unsigned char)(a) << 24) \
							| ((unsigned char)(b) << 16) \
							| ((unsigned char)(c) << 8) \
							| (unsigned char)(d))

riff_t *riff_read (QFile *file);
void riff_free (riff_t *riff);

#endif//__QF_riff_h
