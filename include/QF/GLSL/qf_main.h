/*
	qf_main.h

	glsl main stuff from the renderer.

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

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

#ifndef __QF_GLSL_qf_main_h
#define __QF_GLSL_qf_main_h

struct entqueue_s;

void glsl_R_NewMap (model_t *worldmodel, model_t **models, int num_models);
void glsl_R_RenderEntities (struct entqueue_s *queue);
void glsl_R_RenderView (void);
void glsl_R_ClearState (void);
void glsl_R_ViewChanged (void);
void glsl_R_LineGraph (int x, int y, int *h_vals, int count, int height);

#endif//__QF_GLSL_qf_main_h
