/*
	pak.h

	Structs for pack files on disk

	Copyright (C) 2001 Bill Currie

	Author: Bill Currie
	Date: 2002/3/12

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

#ifndef __qf_pak_h
#define __qf_pak_h

/** \addtogroup pak
*/
///@{

// little-endian PACK
#define IDPAKHEADER		(('K'<<24)+('C'<<16)+('A'<<8)+'P')

#define PAK_PATH_LENGTH 56

typedef struct {
	char		name[PAK_PATH_LENGTH];
	int			filepos, filelen;
} dpackfile_t;

typedef struct {
	char		id[4];
	int			dirofs;
	int			dirlen;
} dpackheader_t;

///@}

#endif//__qf_pak_h
