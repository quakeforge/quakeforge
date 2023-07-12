/*
	types.h

	Entity management

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2021/02/26

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

#ifndef __QF_scene_types_h
#define __QF_scene_types_h

#include "QF/darray.h"
#include "QF/qtypes.h"
#include "QF/simd/vec4f.h"
#include "QF/simd/mat4f.h"

/** \defgroup scene_types Scene type definitions
	\ingroup scene
*/
///@{

typedef struct mat4fset_s DARRAY_TYPE (mat4f_t) mat4fset_t;
typedef struct vec4fset_s DARRAY_TYPE (vec4f_t) vec4fset_t;
typedef struct uint32set_s DARRAY_TYPE (uint32_t) uint32set_t;
typedef struct byteset_s DARRAY_TYPE (byte) byteset_t;
typedef struct stringset_s DARRAY_TYPE (char *) stringset_t;
typedef struct xformset_s DARRAY_TYPE (struct transform_s *) xformset_t;

///@}

#endif//__QF_scene_types_h
