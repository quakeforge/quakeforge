/*
	pvsfile.h

	External pvs file support

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/07/27

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

#ifndef __QF_pvsfile_h
#define __QF_pvsfile_h

#include <stdint.h>

#include "QF/qtypes.h"

/** \defgroup pvsfile External PVS
	\ingroup utils

	External PVS data.
*/
///@{

#define PVS_MAGIC "QF PVS\r\n\x1a\x04"
#define PVS_VERSION 1

#define PVS_IS_FATPVS 1	///< data is for fat pvs (phs, lighting, etc)
						///< otherwise it's the base pvs
#define PVS_UTF8_RLE  2	///< 0-byte run length encoding uses utf-8

typedef struct pvsfile_s {
	char        magic[16];

	uint32_t    version;
	uint32_t    md4_offset;		///< offset of md4 sum, or 0 if not present
	uint32_t    flags;
	uint32_t    visleafs;		///< does NOT include leaf-0
	uint32_t    visoffsets[];	///< does NOT include leaf-0
} pvsfile_t;

///@}

#endif//__QF_pvsfile_h
