/*
	noise.h

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

/** \defgroup qflight_noise Light noise functions.
	\ingroup qflight
*/
///@{

float noise3d (vec3_t v, int num) __attribute__((pure));
float noiseXYZ (float x, float y, float z, int num) __attribute__((pure));
float noise_scaled (vec3_t v, float s, int num) __attribute__((pure));
float noise_perlin (vec3_t v, float p, int num) __attribute__((pure));
void snap_vector (vec3_t v_old, vec3_t v_new, float scale);

///@}
