/*
	mersenne.c

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/*	This implementation is based on the pseudo-code given on
	http://en.wikipedia.org/wiki/Mersenne_twister (as of the above date) with
	some references to the code available from
	http://www.cs.hmc.edu/~geoff/mtwist.html

	The main difference is the actual generator has been modified such that
	rather than pre-generating the 624 untempered values and then
	regenerating new ones every 624th call, one single untempered value is
	generated every call. While the per-call time will be higher for this
	implementation, it should be constant and should also be very close to
	the averate call time of the standard implementation.

	The code has been tested by generating 2048 values with both
	implementations using the same seed (0xdeadbeef) and comparing the output.
	There were no differences.
*/

#define IMPLEMENT_MTWIST_Funcs
#include "QF/mersenne.h"

#define KNUTH_MULT		1812433253ul	// 0x6c078965
#define KNUTH_SHIFT		30
#define HIGH_BITS		0x80000000
#define LOW_BITS		0x7fffffff
#define MT_RECURRENCE	0x9908b0df
#define MT_TEMPER_U		11
#define MT_TEMPER_S		7
#define MT_TEMPER_T		15
#define MT_TEMPER_L		18
#define MT_TEMPER_B		0x9d2c5680
#define MT_TEMPER_C		0xefc60000
#define MT_FEEDBACK		397

#define MT state->state

void
mtwist_seed (mtstate_t *state, uint32_t seed)
{
	int         i;

	state->index = 0;
	MT[0] = seed;
	for (i = 1; i < MT_STATE_SIZE; i++) {
		MT[i] = KNUTH_MULT * (MT[i - 1] ^ (MT[i - 1] >> KNUTH_SHIFT)) + i;
	}
}

static inline void
mtwist_generate (mtstate_t *state, int i)
{
	uint32_t    y;
	static const uint32_t recurrence[] = { 0, MT_RECURRENCE };

	y = (MT[i] & HIGH_BITS) | (MT[(i + 1) % MT_STATE_SIZE] & LOW_BITS);
	MT[i] = MT[(i + MT_FEEDBACK) % MT_STATE_SIZE] ^ (y >> 1);
	MT[i] ^= recurrence[y & 1];
}

uint32_t
mtwist_rand (mtstate_t *state)
{
	uint32_t    y;

	mtwist_generate (state, state->index);
	y = MT[state->index];
	y ^= (y >> MT_TEMPER_U);
	y ^= (y << MT_TEMPER_S) & MT_TEMPER_B;
	y ^= (y << MT_TEMPER_T) & MT_TEMPER_C;
	y ^= (y >> MT_TEMPER_L);
	state->index = (state->index + 1) % MT_STATE_SIZE;
	return y;
}
