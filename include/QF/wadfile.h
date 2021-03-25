/*
	wadfile.h

	(description)

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

/** \defgroup wad Wad Files
	\ingroup utils
	Wadfile processing
*/
///@{

#ifndef __QF_wadfile_h
#define __QF_wadfile_h

#include "QF/quakeio.h"
#include "QF/qtypes.h"

//===============
//   TYPES
//===============

#define	CMP_NONE		0
#define	CMP_LZSS		1

#define	TYP_NONE		0
#define	TYP_LABEL		1

#define	TYP_LUMPY		64				// 64 + grab command number
#define	TYP_PALETTE		64
#define	TYP_QTEX		65
#define	TYP_QPIC		66
#define	TYP_SOUND		67
#define	TYP_MIPTEX		68

typedef struct qpic_s {
	int32_t		width, height;
	byte		data[];			// variably sized
} qpic_t;

typedef struct wadinfo_s {
	char		id[4];					// should be WAD2 or 2DAW
	int32_t		numlumps;
	int32_t		infotableofs;
} wadinfo_t;

typedef struct lumpinfo_s {
	int32_t		filepos;
	int32_t		disksize;
	int32_t		size;					// uncompressed
	byte		type;
	byte		compression;
	byte		pad1, pad2;
	char		name[16];				// must be null terminated
} lumpinfo_t;

typedef struct wad_s {
	char       *filename;
	QFile      *handle;
	int         numlumps;
	int         lumps_size;
	lumpinfo_t *lumps;
	struct hashtab_s *lump_hash;

	wadinfo_t   header;

	int         modified;
	int         old_numlumps;
	int         pad;
} wad_t;

wad_t *wad_new (const char *name);
void wad_del (wad_t *wad);
void wad_rehash (wad_t *wad);
wad_t *wad_open (const char *name);
void wad_close (wad_t *wad);
wad_t *wad_create (const char *name);
int wad_add (wad_t *wad, const char *filename, const char *lumpname,
			 byte type);
int wad_add_data (wad_t *wad, const char *lumpname, byte type,
				  const void *data, int bytes);
lumpinfo_t *wad_find_lump (wad_t *wad, const char *filename);

///@}

#endif//__QF_wadfile_h
