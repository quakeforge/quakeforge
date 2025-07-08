/*
	base64.h

	Base 64 encoding and decoding

	Copyright (C) 2025 Bill Currie <bill@taniwha.org>

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

#ifndef __QF_base64_h
#define __QF_base64_h

#include "QF/qtypes.h"

typedef struct dstring_s dstring_t;
size_t base64_encode (dstring_t *out, const void *data, size_t count);
size_t base64_decode (dstring_t *out, const char *data, size_t count);

#endif//__QF_base64_h
