/*
	target_v6.c

	V6 and V6+ progs backends.

	Copyright (C) 2024 Bill Currie <bill@taniwha.org>

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

#include "QF/progs/pr_comp.h"

#include "tools/qfcc/include/target.h"
#include "tools/qfcc/include/type.h"

static bool
v6_value_too_large (const type_t *val_type)
{
	return type_size (val_type) > type_size (&type_param);
}

target_t v6_target = {
	.value_too_large = v6_value_too_large,
};

target_t v6p_target = {
	.value_too_large = v6_value_too_large,
};
