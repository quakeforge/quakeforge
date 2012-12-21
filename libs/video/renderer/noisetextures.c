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
#include "QF/sys.h"
#include "noisetextures.h"

#include "compat.h"


void
noise_diamondsquare (unsigned char *noise, unsigned int size,
					 unsigned int startgrid)
{
	int         amplitude, max, min;
	int			size1 = size - 1;
	int        *noisebuf;
	unsigned int	gridpower, sizepower, g, g2, x, y;

#define n(x, y) noisebuf[(((y) & size1) << sizepower) + ((x) & size1)]

	for (sizepower = 0; (unsigned int) (1 << sizepower) < size; sizepower++);
	if (size != (unsigned int) (1 << sizepower))
		Sys_Error("fractalnoise: size must be power of 2");

	for (gridpower = 0; (unsigned int) (1 << gridpower) < startgrid;
		 gridpower++);
	if (startgrid != (unsigned int) (1 << gridpower))
		Sys_Error("fractalnoise: grid must be power of 2");

	startgrid = min(startgrid, size);
	amplitude = 0xFFFF; // this gets halved before use
	noisebuf = calloc (size * size, sizeof (int));
	memset(noisebuf, 0, size * size * sizeof(int));

	for (g2 = startgrid; g2; g2 >>= 1) {
		// Brownian Motion
		amplitude >>= 1;
		for (y = 0; y < size; y += g2)
			for (x = 0; x < size; x += g2)
				n (x,y) += (rand () & amplitude);

		g = g2 >> 1;
		if (g) {
			// subdivide, diamond-square algorithm
			// diamond
			for (y = 0; y < size; y += g2)
				for (x = 0; x < size; x += g2)
					n (x + g, y + g) =
						(n (x, y) + n (x + g2, y) + n (x, y + g2) +
						 n (x + g2, y + g2)) >> 2;
			// square
			for (y = 0; y < size; y += g2)
				for (x = 0; x < size; x += g2) {
					n (x + g, y) = (n (x, y) + n (x + g2, y) +
									n (x + g, y - g) + n (x + g, y + g)) >> 2;
					n (x, y + g) = (n (x, y) + n (x, y + g2) +
									n (x - g, y + g) + n (x + g, y + g)) >> 2;
			}
		}
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
	max++;
	// normalize noise and copy to output
	for (y = 0; y < size; y++)
		for (x = 0; x < size; x++)
			*noise++ = (unsigned char) (((n (x, y) - min) * 256) / max);
	free (noisebuf);
	#undef n
}

void
noise_plasma (unsigned char *noise, int size)
{
	unsigned int   a, b, c, i;
	int            d, j, k;

	if (128 >= size)
		d = 64 / size;
	else
		d = -size / 64;

	memset (noise, 128, sizeof (*noise));

	for (i = size; i > 0; i /= 2) {
		for (j = 0; j < size; j += i) {
			for (k = 0; k < size; k += i) {
				if (d >= 0)
				        c = i * d;
				else
					c = -i / d;

				c = qfrandom (c * 2) - c;

				for (a = j; a < j + i; a++)
					for (b = k; b < k + i; b++)
						noise[a * size + b] += c;
			}
		}
	}
}
