/*
	prefixsum.c

	Prefix sums

	Copyright (C) 2026 Bill Currie <bill@taniwha.org>

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

#include "QF/prefixsum.h"

uint32_t
prefixsum_uint32 (uint32_t *array, uint32_t count)
{
	uint32_t sum = 0;
	for (uint32_t i = 0; i < count; i++) {
		uint32_t temp = array[i];
		array[i] = sum;
		sum += temp;
	}
	return sum;
}
