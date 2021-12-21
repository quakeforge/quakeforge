/*
	mersenne.h

	Mersenne Twister PRNG

	Copyright (C) 2013 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2013/01/21

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

#ifndef __QF_mersenne_h
#define __QF_mersenne_h

#include "QF/qtypes.h"

#define MT_STATE_SIZE	624
typedef struct {
	uint32_t    state[MT_STATE_SIZE];
	int index;
} mtstate_t;


void mtwist_seed (mtstate_t *state, uint32_t seed);
uint32_t mtwist_rand (mtstate_t *state);
GNU89INLINE inline float mtwist_rand_0_1 (mtstate_t *state);
GNU89INLINE inline float mtwist_rand_m1_1 (mtstate_t *state);

#ifndef IMPLEMENT_MTWIST_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
float
mtwist_rand_0_1 (mtstate_t *state)
{
	union {
		uint32_t    u;
		float       f;
	}           uf;

	uf.u = mtwist_rand (state) & 0x007fffff;
	uf.u |= 0x3f800000;
	return uf.f - 1.0;
}

#ifndef IMPLEMENT_MTWIST_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
float
mtwist_rand_m1_1 (mtstate_t *state)
{
	union {
		uint32_t    u;
		float       f;
	}           uf;

	do {
		uf.u = mtwist_rand (state) & 0x007fffff;
	} while (!uf.u);
	uf.u |= 0x40000000;
	return uf.f - 3.0;
}

#endif//__QF_mersenne_h
