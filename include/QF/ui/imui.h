/*
	imui.h

	Immediate mode user interface

	Copyright (C) 2023 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2023/07/01

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

#ifndef __QF_ui_imui_h
#define __QF_ui_imui_h

typedef struct imui_ctx_s imui_ctx_t;
struct canvas_system_s;

typedef enum IMUI_SizeKind {
	IMUI_SizeKind_Null,
	IMUI_SizeKind_Pixels,
	IMUI_SizeKind_TextContent,
	IMUI_SizeKind_PercentOfParent,
	IMUI_SizeKind_ChildrenSum,
} IMUI_SizeKind;

typedef enum IMUI_Axis {
	IMUI_Axis_X,
	IMUI_Axis_Y,
	IMUI_Axis_count,
} IMUI_Axis;

typedef struct imui_size {
	IMUI_SizeKind kind;
	float       value;
	float       strictness;
} imui_size;

typedef struct imui_layout_s {
	imui_size semantic_size[IMUI_Axis_count];
} imui_layout_t;

imui_ctx_t *IMUI_NewContext (struct canvas_system_s canvas_sys,
							 const char *font, float fontsize);
void IMUI_DestroyContext (imui_ctx_t *ctx);

void IMUI_SetVisible (imui_ctx_t *ctx, bool visible);
void IMUI_SetSize (imui_ctx_t *ctx, int xlen, int ylen);
void IMUI_Draw (imui_ctx_t *ctx);

bool IMUI_Button (imui_ctx_t *ctx, const char *label);
bool IMUI_Checkbox (imui_ctx_t *ctx, bool *flag, const char *label);
void IMUI_Radio (imui_ctx_t *ctx, int *state, int value, const char *label);
void IMUI_Slider (imui_ctx_t *ctx, float *value, float minval, float maxval,
				  const char *label);

// #define IMUI_context to an imui_ctx_t * variable

#define UI_Button(label) \
	IMUI_Button(IMUI_context, label)

#define UI_Checkbox(flag, label) \
	IMUI_Checkbox(IMUI_context, flag, label)

#define UI_Radio(state, value, label) \
	IMUI_Radio(IMUI_context, state, value, label)

#define UI_Slider(value, minval, maxval, label) \
	IMUI_Slider(IMUI_context, value, minval, maxval, label)

#endif//__QF_ui_imui_h
