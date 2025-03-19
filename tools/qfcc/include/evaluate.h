/*
	evaluate.h

	constant evaluation

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/11/16

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

#ifndef __evaluate_h
#define __evaluate_h

#include "QF/progs.h"

/** \defgroup qfcc_evaluate Constant evaluation.
	\ingroup qfcc_expr
*/
///@{

void evaluate_debug_handler (prdebug_t event, void *param, void *data);

struct type_s;
struct ex_value_s;
struct ex_value_s *convert_value (struct ex_value_s *value,
								  const struct type_s *type);
const struct expr_s *evaluate_constexpr (const struct expr_s *e);
void setup_value_progs (void);

///@}

#endif//__evaluate_h
