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

typedef struct type_s type_t;
typedef struct expr_s expr_t;
typedef struct symtab_s symtab_t;

typedef struct basis_blade_s {
	pr_uint_t   mask;				///< bit-mask of basis vectors
	int         scale;				///< 1, 0, or -1
} basis_blade_t;

typedef struct basis_group_s {
	short       count;			///< number of blades
	short       alignment;		///< alignment of group
	pr_uint_t   group_mask;
	pr_uvec2_t  range;			///< min, max of basis blade masks
	basis_blade_t *blades;
	const char *name;			///< group name for symbolic access
	int        *map;			///< map from basis mask (-range[0]) to index
	set_t      *set;			///< set of basis masks in group
} basis_group_t;

typedef struct group_map_s {
	int         num;			///< group number
	int         ind;			///< index of component within group
	int         base;			///< base of group in full multivector (n/u)
} group_map_t;

typedef struct basis_layout_s {
	int         count;			///< number of groups
	pr_uvec2_t  range;			///< min, max of basis blade masks
	basis_group_t *groups;
	group_map_t *group_map;		///< map from component number to group
	int        *mask_map;		///< map from basis mask to component number
	int         blade_count;	///< number of basis blade masks
	set_t      *set;			///< set of basis masks in layout
} basis_layout_t;

typedef struct metric_s {
	pr_uint_t   plus;			///< mask of elements that square to +1
	pr_uint_t   minus;			///< mask of elements that square to -1
	pr_uint_t   zero;			///< mask of elements that square to  0
} metric_t;

typedef struct algebra_s {
	const type_t *type;			///< underlying type (float or double)
	const type_t *algebra_type;	///< type for algebra
	metric_t    metric;
	basis_layout_t layout;
	basis_group_t *groups;
	const type_t **mvec_types;
	struct symbol_s *mvec_sym;
	int         num_components;	///< number of componets (2^d)
	int         dimension;		///< number of dimensions (plus + minus + zero)
	int         plus;			///< number of elements squaring to +1
	int         minus;			///< number of elements squaring to -1
	int         zero;			///< number of elements squaring to 0
} algebra_t;

typedef struct multivector_s {
	int         num_components;
	int         group_mask;
	algebra_t  *algebra;
	struct symbol_s *mvec_sym;	///< null if single group
} multivector_t;

typedef struct attribute_s attribute_t;
bool is_algebra (const type_t *type) __attribute__((pure));
etype_t algebra_low_level_type (const type_t *type) __attribute__((pure));
const type_t *algebra_type (const type_t *type, const expr_t *params);
const type_t *algebra_subtype (const type_t *type, const attribute_t *attr);
const type_t *algebra_mvec_type (algebra_t *algebra, pr_uint_t group_mask);
const type_t *algebra_float_type (algebra_t *algebra, pr_uint_t group_mask);
int algebra_count_flips (const algebra_t *alg, pr_uint_t a, pr_uint_t b) __attribute__((pure));
struct ex_value_s *algebra_blade_value (algebra_t *alg, const char *name);
struct symtab_s *algebra_scope (struct symtab_s *curscope, void *data);
algebra_t *algebra_context (const type_t *type) __attribute__((pure));
void algebra_print_type_str (struct dstring_s *str, const type_t *type);
void algebra_encode_type (struct dstring_s *encoding, const type_t *type);
int algebra_type_size (const type_t *type) __attribute__((pure));
int algebra_type_width (const type_t *type) __attribute__((pure));

int metric_apply (const metric_t *metric, pr_uint_t a, pr_uint_t b) __attribute__((pure));

algebra_t *algebra_get (const type_t *type) __attribute__((pure));
bool algebra_type_assignable (const type_t *dst, const type_t *src) __attribute__((pure));
const type_t *algebra_base_type (const type_t *type) __attribute__((pure));
const type_t *algebra_struct_type (const type_t *type) __attribute__((pure));
bool is_mono_grade (const type_t *type) __attribute__((pure));
bool is_mono_group (const type_t *type) __attribute__((pure));
int algebra_get_grade (const type_t *type) __attribute__((pure));
int algebra_blade_grade (basis_blade_t blade) __attribute__((const));

pr_uint_t get_group_mask (const type_t *type, algebra_t *algebra) __attribute__((pure));

symtab_t *get_mvec_struct (const type_t *type) __attribute__((pure));
int algebra_type_count (const type_t *type) __attribute__((pure));

const expr_t *algebra_binary_expr (int op, const expr_t *e1, const expr_t *e2);
const expr_t *algebra_negate (const expr_t *e);
const expr_t *algebra_dual (const expr_t *e);
const expr_t *algebra_undual (const expr_t *e);
const expr_t *algebra_reverse (const expr_t *e);
const expr_t *algebra_cast_expr (const type_t *dstType, const expr_t *e);
const expr_t *algebra_compound_expr (const type_t *dst, const expr_t *src);
const expr_t *algebra_field_expr (const expr_t *mvec, const expr_t *field_name);
const expr_t *algebra_optimize (const expr_t *e);

const expr_t *mvec_expr (const expr_t *expr, algebra_t *algebra);
void mvec_scatter (const expr_t **components, const expr_t *mvec,
				   algebra_t *algebra);
const expr_t *mvec_gather (const expr_t **components, algebra_t *algebra);

#endif//__algebra_h
