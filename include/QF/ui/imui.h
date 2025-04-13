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

#include "QF/ui/view.h"

typedef struct imui_ctx_s imui_ctx_t;
struct canvas_system_s;
struct IE_event_s;
struct passage_s;

enum {
	imui_fraction_x,///< imui_frac_t
	imui_fraction_y,///< imui_frac_t
	imui_reference,	///< imui_reference_t

	imui_comp_count
};

extern const component_t imui_components[imui_comp_count];

typedef enum {
	imui_size_none,
	imui_size_pixels,
	imui_size_fittext,
	imui_size_fraction,
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

// on a root view, this points to a pseudo-parent
// on a non-root view, this points to a pseudo-child (layout info is taken from
// the reference to set the non-root view's size, then that is propagated back
// to the reference)
typedef struct imui_reference_s {
	uint32_t    ref_id;
	bool        update;
	struct imui_ctx_s *ctx;		// owns entity if not null
} imui_reference_t;

typedef struct imui_frac_s {
	int         num;
	int         den;
} imui_frac_t;

typedef struct imui_window_s {
	const char *name;
	uint32_t    self;
	int         xpos;
	int         ypos;
	int         xlen;
	int         ylen;
	int         group_offset;
	int         mode;
	bool        is_open;			// window/panel/menu
	bool        is_collapsed;		// for windows
	bool        no_collapse;		// for menus
	bool        auto_fit;			// for windows

	const char *reference;
	grav_t      reference_gravity;
	grav_t      anchor_gravity;
	uint32_t    parent;	// for submenus
} imui_window_t;

typedef struct imui_state_s {
	char       *label;
	uint32_t    self;
	uint32_t    label_len;
	int         key_offset;
	uint32_t    menu;
	int32_t		draw_order;	// for window canvases
	int32_t     draw_group;
	uint32_t    first_link;
	uint32_t    num_links;
	uint32_t    frame_count;
	uint32_t    old_entity;
	uint32_t    entity;
	uint32_t    content;
	view_pos_t  pos;
	view_pos_t  len;
	imui_frac_t fraction;
	bool        auto_fit;
} imui_state_t;

typedef struct imui_io_s {
	view_pos_t  mouse;
	uint32_t    buttons;
	uint32_t    pressed;
	uint32_t    released;
	uint32_t    hot;
	uint32_t    active;
} imui_io_t;

imui_ctx_t *IMUI_NewContext (struct canvas_system_s canvas_sys,
							 const char *font, float fontsize);
void IMUI_DestroyContext (imui_ctx_t *ctx);

uint32_t IMUI_RegisterWindow (imui_ctx_t *ctx, imui_window_t *window);
void IMUI_DeregisterWindow (imui_ctx_t *ctx, imui_window_t *window);
imui_window_t *IMUI_GetWindow (imui_ctx_t *ctx, uint32_t wid) __attribute__((pure));

imui_state_t *IMUI_CurrentState (imui_ctx_t *ctx);
imui_state_t *IMUI_FindState (imui_ctx_t *ctx, const char *label);

void IMUI_SetVisible (imui_ctx_t *ctx, bool visible);
void IMUI_SetSize (imui_ctx_t *ctx, int xlen, int ylen);
bool IMUI_ProcessEvent (imui_ctx_t *ctx, const struct IE_event_s *ie_event);
imui_io_t IMUI_GetIO (imui_ctx_t *ctx) __attribute__((pure));
void IMUI_BeginFrame (imui_ctx_t *ctx);
void IMUI_Draw (imui_ctx_t *ctx);

int IMUI_PushLayout (imui_ctx_t *ctx, bool vertical);
void IMUI_PopLayout (imui_ctx_t *ctx);
void IMUI_Layout_SetXSize (imui_ctx_t *ctx, imui_size_t size, int value);
void IMUI_Layout_SetYSize (imui_ctx_t *ctx, imui_size_t size, int value);

int IMUI_PushStyle (imui_ctx_t *ctx, const imui_style_t *style);
void IMUI_PopStyle (imui_ctx_t *ctx);
void IMUI_Style_Update (imui_ctx_t *ctx, const imui_style_t *style);
void IMUI_Style_Fetch (const imui_ctx_t *ctx, imui_style_t *style);

void IMUI_SetFill (imui_ctx_t *ctx, byte color);
void IMUI_Label (imui_ctx_t *ctx, const char *label);
void IMUI_Labelf (imui_ctx_t *ctx, const char *fmt, ...)__attribute__((format(PRINTF,2,3)));
void IMUI_Passage (imui_ctx_t *ctx, const char *name,
				   struct passage_s *passage);
bool IMUI_Button (imui_ctx_t *ctx, const char *label);
bool IMUI_Checkbox (imui_ctx_t *ctx, bool *flag, const char *label);
void IMUI_Radio (imui_ctx_t *ctx, int *curvalue, int value, const char *label);
void IMUI_Slider (imui_ctx_t *ctx, float *value, float minval, float maxval,
				  const char *label);
void IMUI_Spacer (imui_ctx_t *ctx,
				  imui_size_t xsize, int xvalue,
				  imui_size_t ysize, int yvalue);
view_pos_t IMUI_Dragable (imui_ctx_t *ctx,
						  imui_size_t xsize, int xvalue,
						  imui_size_t ysize, int yvalue,
						  const char *name);
int IMUI_StartPanel (imui_ctx_t *ctx, imui_window_t *panel);
int IMUI_ExtendPanel (imui_ctx_t *ctx, const char *panel_name);
void IMUI_EndPanel (imui_ctx_t *ctx);

int IMUI_StartMenu (imui_ctx_t *ctx, imui_window_t *menu, bool vertical);
void IMUI_EndMenu (imui_ctx_t *ctx);
bool IMUI_MenuItem (imui_ctx_t *ctx, const char *label, bool collapse);

void IMUI_TitleBar (imui_ctx_t *ctx, imui_window_t *window);
void IMUI_CollapseButton (imui_ctx_t *ctx, imui_window_t *window);
void IMUI_CloseButton (imui_ctx_t *ctx, imui_window_t *window);

int IMUI_StartWindow (imui_ctx_t *ctx, imui_window_t *window);
void IMUI_EndWindow (imui_ctx_t *ctx);

int IMUI_StartScrollBox (imui_ctx_t *ctx, const char *name);
void IMUI_EndScrollBox (imui_ctx_t *ctx);

void IMUI_ScrollBar (imui_ctx_t *ctx, const char *name);
int IMUI_StartScroller (imui_ctx_t *ctx);
void IMUI_EndScroller (imui_ctx_t *ctx);

void IMUI_SetViewPos (imui_ctx_t *ctx, view_pos_t pos);
void IMUI_SetViewLen (imui_ctx_t *ctx, view_pos_t len);

#define IMUI_DeferLoop(begin, end) \
	for (int _i_ = (begin); !_i_; _i_++, (end))

// #define IMUI_context to an imui_ctx_t * variable

#define UI_SetFill(color) \
	IMUI_SetFill(IMUI_context, color)

#define UI_Label(label) \
	IMUI_Label(IMUI_context, label)

#define UI_Labelf(fmt...) \
	IMUI_Labelf(IMUI_context, fmt)

#define UI_Passage(name, psg) \
	IMUI_Passage(IMUI_context, name, psg)

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

#define UI_ExtendPanel(panel_name) \
	IMUI_DeferLoop (IMUI_ExtendPanel (IMUI_context, panel_name), \
					IMUI_EndPanel (IMUI_context))

#define UI_Menu(menu) \
	IMUI_DeferLoop (IMUI_StartMenu (IMUI_context, menu, true), \
					IMUI_EndMenu (IMUI_context))

#define UI_MenuBar(menu) \
	IMUI_DeferLoop (IMUI_StartMenu (IMUI_context, menu, false), \
					IMUI_EndMenu (IMUI_context))

#define UI_MenuItem(label) \
	IMUI_MenuItem (IMUI_context, label, true)

#define UI_Window(window) \
	IMUI_DeferLoop (IMUI_StartWindow (IMUI_context, window), \
					IMUI_EndWindow (IMUI_context))

#define UI_ScrollBox(name) \
	IMUI_DeferLoop (IMUI_StartScrollBox (IMUI_context, name), \
					IMUI_EndScrollBox (IMUI_context))
#define UI_ScrollBar(name) \
	IMUI_ScrollBar (IMUI_context, name)
#define UI_Scroller() \
	IMUI_DeferLoop (IMUI_StartScroller (IMUI_context), \
					IMUI_EndScroller (IMUI_context))

#define UI_Style(style) \
	IMUI_DeferLoop (IMUI_PushStyle (IMUI_context, style), \
					IMUI_PopStyle (IMUI_context ))

#define UI_Horizontal UI_Layout(false)
#define UI_Vertical UI_Layout(true)

#endif//__QF_ui_imui_h
