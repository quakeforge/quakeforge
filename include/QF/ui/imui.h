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

#include "QF/ecs/component.h"

typedef struct imui_ctx_s imui_ctx_t;
struct canvas_system_s;
struct IE_event_s;

enum {
	imui_percent_x,	///< int
	imui_percent_y,	///< int

	imui_comp_count
};

extern const component_t imui_components[imui_comp_count];

typedef enum {
	imui_size_none,
	imui_size_pixels,
	imui_size_fittext,
	imui_size_percent,
	imui_size_fitchildren,
	imui_size_expand,
} imui_size_t;

typedef union imui_color_s {
	struct {
		uint32_t    normal;
		uint32_t    hot;
		uint32_t    active;
	};
	uint32_t    color[3];
} imui_color_t;

typedef struct imui_style_s {
	imui_color_t background;
	imui_color_t foreground;
	imui_color_t text;
} imui_style_t;

typedef struct imui_window_s {
	const char *name;
	int         xpos;
	int         ypos;
	int         xlen;
	int         ylen;
	int         mode;
	bool        is_open;
	bool        is_collapsed;
} imui_window_t;

imui_ctx_t *IMUI_NewContext (struct canvas_system_s canvas_sys,
							 const char *font, float fontsize);
void IMUI_DestroyContext (imui_ctx_t *ctx);

void IMUI_SetVisible (imui_ctx_t *ctx, bool visible);
void IMUI_SetSize (imui_ctx_t *ctx, int xlen, int ylen);
void IMUI_ProcessEvent (imui_ctx_t *ctx, const struct IE_event_s *ie_event);
void IMUI_BeginFrame (imui_ctx_t *ctx);
void IMUI_Draw (imui_ctx_t *ctx);

void IMUI_PushLayout (imui_ctx_t *ctx, bool vertical);
void IMUI_PopLayout (imui_ctx_t *ctx);
void IMUI_Layout_SetXSize (imui_ctx_t *ctx, imui_size_t size, int value);
void IMUI_Layout_SetYSize (imui_ctx_t *ctx, imui_size_t size, int value);

void IMUI_PushStyle (imui_ctx_t *ctx, const imui_style_t *style);
void IMUI_PopStyle (imui_ctx_t *ctx);
void IMUI_Style_Update (imui_ctx_t *ctx, const imui_style_t *style);
void IMUI_Style_Fetch (const imui_ctx_t *ctx, imui_style_t *style);

void IMUI_Label (imui_ctx_t *ctx, const char *label);
void IMUI_Labelf (imui_ctx_t *ctx, const char *fmt, ...)__attribute__((format(PRINTF,2,3)));
bool IMUI_Button (imui_ctx_t *ctx, const char *label);
bool IMUI_Checkbox (imui_ctx_t *ctx, bool *flag, const char *label);
void IMUI_Radio (imui_ctx_t *ctx, int *curvalue, int value, const char *label);
void IMUI_Slider (imui_ctx_t *ctx, float *value, float minval, float maxval,
				  const char *label);
void IMUI_Spacer (imui_ctx_t *ctx,
				  imui_size_t xsize, int xvalue,
				  imui_size_t ysize, int yvalue);
void IMUI_FlexibleSpace (imui_ctx_t *ctx);

void IMUI_StartPanel (imui_ctx_t *ctx, imui_window_t *panel);
void IMUI_EndPanel (imui_ctx_t *ctx);

void IMUI_StartWindow (imui_ctx_t *ctx, imui_window_t *window);
void IMUI_EndWindow (imui_ctx_t *ctx);

#define IMUI_DeferLoop(begin, end) \
	for (int _i_ = ((begin), 0); !_i_; _i_++, (end))

// #define IMUI_context to an imui_ctx_t * variable

#define UI_Label(label) \
	IMUI_Label(IMUI_context, label)

#define UI_Labelf(fmt...) \
	IMUI_Labelf(IMUI_context, fmt)

#define UI_Button(label) \
	IMUI_Button(IMUI_context, label)

#define UI_Checkbox(flag, label) \
	IMUI_Checkbox(IMUI_context, flag, label)

#define UI_Radio(state, value, label) \
	IMUI_Radio(IMUI_context, state, value, label)

#define UI_Slider(value, minval, maxval, label) \
	IMUI_Slider(IMUI_context, value, minval, maxval, label)

#define UI_FlexibleSpace() \
	IMUI_Spacer(IMUI_context, imui_size_expand, 100, imui_size_expand, 100)

#define UI_Layout(vertical) \
	IMUI_DeferLoop (IMUI_PushLayout (IMUI_context, vertical), \
					IMUI_PopLayout (IMUI_context ))

#define UI_Panel(panel) \
	IMUI_DeferLoop (IMUI_StartPanel (IMUI_context, panel), \
					IMUI_EndPanel (IMUI_context))

#define UI_Window(window) \
	IMUI_DeferLoop (IMUI_StartWindow (IMUI_context, window), \
					IMUI_EndWindow (IMUI_context))

#define UI_Style(style) \
	IMUI_DeferLoop (IMUI_PushStyle (IMUI_context, style), \
					IMUI_PopStyle (IMUI_context ))

#define UI_Horizontal UI_Layout(false)
#define UI_Vertical UI_Layout(true)

#endif//__QF_ui_imui_h
