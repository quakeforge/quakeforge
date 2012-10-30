/*
	flow.h

	Flow graph analysis.

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/10/30

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
#ifndef flow_h
#define flow_h

/** \defgroup qfcc_flow Flow graph analysis
	\ingroup qfcc
*/
//@{

struct function_s;
struct sblock_s;
struct statement_s;

int flow_is_cond (struct statement_s *s);
int flow_is_goto (struct statement_s *s);
int flow_is_return (struct statement_s *s);
struct sblock_s *flow_get_target (struct statement_s *s);
void flow_build_vars (struct function_s *func);
void flow_build_graph (struct function_s *func);
void flow_calc_dominators (struct function_s *func);

//@}

#endif//flow_h
