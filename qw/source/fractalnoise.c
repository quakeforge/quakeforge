/*
	fractalnoise.c

	LordHavocs fractal noise generator.

	Copyright (C) 2000  Forest `LordHavoc` Hale.

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

#include <stdlib.h>

void
fractalnoise (unsigned char *noise, int size)
{
	int         x, y, g, g2, amplitude, min, max, size1 = size - 1;
	int        *noisebuf;

#define n(x,y) noisebuf[((y)&size1)*size+((x)&size1)]
	noisebuf = calloc (size * size, sizeof (int));

	amplitude = 32767;
	g2 = size;
	n (0, 0) = 0;
	for (; (g = g2 >> 1) >= 1; g2 >>= 1) {
		// subdivide, diamond-square algorythm (really this has little to do
		// with squares)
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
		// brownian motion theory
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
