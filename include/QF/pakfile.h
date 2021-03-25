/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2002 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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
#ifndef __QF_pakfile_h
#define __QF_pakfile_h

/** \defgroup pak pakfile proccessing
	\ingroup utils
*/
///@{

#include "QF/hash.h"
#include "QF/pak.h"
#include "QF/quakeio.h"

typedef struct pack_s {
	char       *filename;
	QFile      *handle;
	int         numfiles;
	int         files_size;
	dpackfile_t *files;
	hashtab_t  *file_hash;

	dpackheader_t header;

	int         modified;
	int         old_numfiles;
	int         pad;
} pack_t;

pack_t *pack_new (const char *name);
void pack_del (pack_t *pack);
void pack_rehash (pack_t *pack);
pack_t *pack_open (const char *name);
void pack_close (pack_t *pack);
pack_t *pack_create (const char *name);
int pack_add (pack_t *pack, const char *filename);
int pack_extract (pack_t *pack, dpackfile_t *pf);
dpackfile_t *pack_find_file (pack_t *pack, const char *filename);

///@}

#endif//__QF_pakfile_h
