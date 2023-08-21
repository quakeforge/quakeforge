/*
	algebra.h

	QC geometric algebra support code

	Copyright (C) 2023 Bill Currie <bill@taniwha.org>

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

#ifndef __algebra_h
#define __algebra_h

#include "QF/set.h"
#include "QF/progs/pr_comp.h"

typedef struct basis_blade_s {
	pr_uint_t   mask;				///< bit-mask of basis vectors
	int         scale;				///< 1, 0, or -1
} basis_blade_t;

typedef struct basis_group_s {
	int         count;
	pr_uivec2_t range;
	basis_blade_t *blades;
	int        *map;
	set_t      *set;
	struct type_s *type;
} basis_group_t;

typedef struct basis_layout_s {
	int         count;
	pr_uivec2_t range;
	basis_group_t *groups;
	pr_ivec3_t *group_map;
	int        *mask_map;
	int         blade_count;
	set_t      *set;
} basis_layout_t;

typedef struct metric_s {
	pr_uint_t   plus;			///< mask of elements that square to +1
	pr_uint_t   minus;			///< mask of elements that square to -1
	pr_uint_t   zero;			///< mask of elements that square to  0
} metric_t;

typedef struct algebra_s {
	struct type_s *type;		///< underlying type (float or double)
	struct type_s *algebra_type;///< type for algebra
	metric_t    metric;
	basis_layout_t layout;
	basis_group_t *groups;
	int         num_components;	///< number of componets (2^d)
	int         dimension;		///< number of dimensions (plus + minus + zero)
	int         plus;			///< number of elements squaring to +1
	int         minus;			///< number of elements squaring to -1
	int         zero;			///< number of elements squaring to 0
} algebra_t;

typedef struct multivector_s {
	int         num_components;
	int         element;
	algebra_t  *algebra;
} multivector_t;

struct expr_s;
bool is_algebra (const struct type_s *type) __attribute__((pure));
struct type_s *algebra_type (struct type_s *type, struct expr_s *params);
struct symtab_s *algebra_scope (struct type_s *type, struct symtab_s *curscope);
void algebra_print_type_str (struct dstring_s *str, const struct type_s *type);
void algebra_encode_type (struct dstring_s *encoding,
						  const struct type_s *type);
int algebra_type_size (const struct type_s *type) __attribute__((pure));
int algebra_type_width (const struct type_s *type) __attribute__((pure));

int metric_apply (const metric_t *metric, pr_uint_t a, pr_uint_t b) __attribute__((pure));

algebra_t *algebra_get (const struct type_s *type) __attribute__((pure));
int algebra_type_assignable (const struct type_s *dst,
							 const struct type_s *src) __attribute__((pure));

struct expr_s *algebra_binary_expr (int op, struct expr_s *e1,
									struct expr_s *e2);
struct expr_s *algebra_negate (struct expr_s *e);
struct expr_s *algebra_dual (struct expr_s *e);
struct expr_s *algebra_reverse (struct expr_s *e);
struct expr_s *algebra_cast_expr (struct type_s *dstType, struct expr_s *e);
struct expr_s *algebra_assign_expr (struct expr_s *dst, struct expr_s *src);

#endif//__algebra_h
