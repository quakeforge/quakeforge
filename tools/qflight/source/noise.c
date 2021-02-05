/*
	noise.c

	3d noise functions.

	Copyright (C) 2000 Seth Galbraith <sgalbrai@linknet.kitsap.lib.wa.us>

	Author: Seth Galbraith <sgalbrai@linknet.kitsap.lib.wa.us>
	Date: 2000/11/23

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

#include <math.h>

#include "QF/qtypes.h"

#include "tools/qflight/include/noise.h"

// returns the 3D noise value for a point in space
float
noise3d (vec3_t v, int num)
{
	int         n;

	n = floor (v[0]) + floor (v[1]) * 57 + floor (v[2]) * 3251;
	n = (n << 13) ^ n;
	n = n * (n * n * 15731 + 789221) + 1376312589;
	return (n & 0x7fffffff) / 2147483648.0;
}

// a variation of Noise3D that takes 3 floats
float
noiseXYZ (float x, float y, float z, int num)
{
	vec3_t      v;

	v[0] = x;
	v[1] = y;
	v[2] = z;
	return noise3d (v, num);
}

// returns a noise value from a scaled up noise pattern
//
// Actually created by sampling points on a certain grid
// and interpolating the inbetween value, making the noise smooth
float
noise_scaled (vec3_t v, float s, int num)
{
	float       n = 0;
	vec3_t      a, b, c, d;

	a[0] = floor (v[0] / s) * s;
	b[0] = a[0] + s;
	c[0] = (v[0] - a[0]) / s;
	d[0] = 1 - c[0];
	a[1] = floor (v[1] / s) * s;
	b[1] = a[1] + s;
	c[1] = (v[1] - a[1]) / s;
	d[1] = 1 - c[1];
	a[2] = floor (v[2] / s) * s;
	b[2] = a[2] + s;
	c[2] = (v[2] - a[2]) / s;
	d[2] = 1 - c[2];

	n += noiseXYZ (a[0], a[1], a[2], num) * d[0] * d[1] * d[2];
	n += noiseXYZ (a[0], a[1], b[2], num) * d[0] * d[1] * c[2];
	n += noiseXYZ (a[0], b[1], a[2], num) * d[0] * c[1] * d[2];
	n += noiseXYZ (a[0], b[1], b[2], num) * d[0] * c[1] * c[2];
	n += noiseXYZ (b[0], a[1], a[2], num) * c[0] * d[1] * d[2];
	n += noiseXYZ (b[0], a[1], b[2], num) * c[0] * d[1] * c[2];
	n += noiseXYZ (b[0], b[1], a[2], num) * c[0] * c[1] * d[2];
	n += noiseXYZ (b[0], b[1], b[2], num) * c[0] * c[1] * c[2];

	return n;
}

// returns a Perlin noise value for a point in 3D space
//
// This noise function combines noise at several different scales
// which makes it slower than just using one layer of noise
float
noise_perlin (vec3_t v, float p, int num)
{
	float       n = 0;

	n += noise_scaled (v, 1024, num);
	n += 0.5 + (noise_scaled (v, 256, num) - 0.5) * p;
	n += 0.5 + (noise_scaled (v, 64, num) - 0.5) * p * p;
	n += 0.5 + (noise_scaled (v, 16, num) - 0.5) * p * p * p;

	return n / 2 - 0.5;
}

// Use to create low-res noise patterns without interpolation
// A good strategy for avoiding seams on terrain and curves
// when called with a scale value of about 32
//
// This is because surfaces have some texture pixels which are
// actually not in the polygon at all so their edges don't
// perfectly fit together.
//
// Creates a random pattern of light and dark squares on flat
// axis aligned surfaces, which is usually not what you want
// but it may have interesting applications
void
snap_vector (vec3_t v_old, vec3_t v_new, float scale)
{
	if (scale <= 0) {
		v_new[0] = v_old[0];
		v_new[1] = v_old[1];
		v_new[2] = v_old[2];
	} else {
		v_new[0] = floor (v_old[0] / scale + 0.5) * scale;
		v_new[1] = floor (v_old[1] / scale + 0.5) * scale;
		v_new[2] = floor (v_old[2] / scale + 0.5) * scale;
	}
}
