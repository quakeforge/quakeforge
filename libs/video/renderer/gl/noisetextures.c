/*
	noisetextures.c

	Noise texture generators

	noise_plasma
	Copyright (C) 2001 Ragnvald `Despair` Maartmann-Moe IV.

	noise_diamondsquare (originally fractalnoise)
	Copyright (C) 2000 Forest `LordHavoc` Hale.

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>

#include "QF/mathlib.h"


void
noise_diamondsquare (unsigned char *noise, int size)
{
	int         amplitude, max, min, g, g2, x, y;
	int         size1 = size - 1;
	int        *noisebuf;

#define n(x,y) noisebuf[((y)&size1)*size+((x)&size1)]
	noisebuf = calloc (size * size, sizeof (int));

	amplitude = 32767;
	g2 = size;
	n (0, 0) = 0;
	for (; (g = g2 >> 1) >= 1; g2 >>= 1) {
		// subdivide, diamond-square algorythm (really this has little
		// to do with squares)
		// diamond
		for (y = 0; y < size; y += g2)
			for (x = 0; x < size; x += g2)
				n (x + g, y + g) =
					(n (x, y) + n (x + g2, y) + n (x, y + g2) +
					 n (x + g2, y + g2)) >> 2;
		// square
		for (y = 0; y < size; y += g2)
			for (x = 0; x < size; x += g2) {
				n (x + g, y) =
					(n (x, y) + n (x + g2, y) + n (x + g, y - g) +
					 n (x + g, y + g)) >> 2;
				n (x, y + g) =
					(n (x, y) + n (x, y + g2) + n (x - g, y + g) +
					 n (x + g, y + g)) >> 2;
			}
		// Brownian motion ( at every smaller level, random behavior )
		amplitude >>= 1;
		for (y = 0; y < size; y += g)
			for (x = 0; x < size; x += g)
				n (x, y) += (rand () & amplitude);
	}
	// find range of noise values
	min = max = 0;
	for (y = 0; y < size; y++)
		for (x = 0; x < size; x++) {
			if (n (x, y) < min)
				min = n (x, y);
			if (n (x, y) > max)
				max = n (x, y);
		}
	max -= min;
	// normalize noise and copy to output
	for (y = 0; y < size; y++)
		for (x = 0; x < size; x++)
			*noise++ = (n (x, y) - min) * 255 / max;
	free (noisebuf);
	#undef n
}

void
noise_plasma (unsigned char *noise, int size)
{
	int   a, b, c, d, i, j, k;

	if (128 >= size)
		d = 64 / size;
	else
		d = -size / 64;

	memset(noise, 128, sizeof (*noise));

	for (i=size; i > 0; i/=2) {
		for (j=0; j < size; j+=i) {
			for (k=0; k < size; k+=i) {
				if (d>=0)
				        c = i * d;
				else
					c = -i / d;

				c=lhrandom(-c, c);

				for (a=j; a < j+i; a++)
					for (b=k; b < k+i; b++)
						noise[a*size+b] += c;
			}
		}
	}
}
