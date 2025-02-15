/*
	image.h

	Ruamoko image support code

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

#ifndef __image_h
#define __image_h

#include "QF/darray.h"

typedef struct type_s type_t;
typedef struct expr_s expr_t;
typedef struct symbol_s symbol_t;

typedef enum : unsigned {
	img_1d,
	img_2d,
	img_3d,
	img_cube,
	img_rect,
	img_buffer,
	img_subpassdata,
} imagedim_t;

typedef struct image_s {
	const type_t *sample_type;
	imagedim_t  dim;
	char        depth;
	bool        arrayed;
	bool        multisample;
	char        sampled;
	unsigned    format;
	uint32_t    id;
} image_t;

typedef struct sampled_image_s {
	const type_t *image_type;
} sampled_image_t;

typedef struct DARRAY_TYPE (image_t) imageset_t;
extern imageset_t imageset;
extern type_t type_image;
extern type_t type_sampler;
extern type_t type_sampled_image;

void image_init_types (void);

const type_t *create_image_type (image_t *image, const type_t *htype);
const type_t *image_type (const type_t *type, const expr_t *params);
const type_t *sampler_type (const type_t *type);

bool is_image (const type_t *type) __attribute__((pure));
bool is_sampled_image (const type_t *type) __attribute__((pure));

#endif//__image_h
