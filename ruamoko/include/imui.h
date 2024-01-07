#ifndef __ruamoko_imui_h
#define __ruamoko_imui_h

typedef @handle imui_ctx_h imui_ctx_t;

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
		unsigned    normal;
		unsigned    hot;
		unsigned    active;
	};
	unsigned    color[3];
} imui_color_t;

typedef struct imui_style_s {
	imui_color_t background;
	imui_color_t foreground;
	imui_color_t text;
} imui_style_t;

typedef struct imui_reference_s {
	unsigned    ref_id;
} imui_reference_t;

// opaque due to having C pointers, but allocated in progs space
typedef struct imui_window_s imui_window_t;

imui_window_t *IMUI_NewWindow (string name);
void IMUI_DeleteWindow (imui_window_t *window);
int IMUI_Window_IsOpen (imui_window_t *window);
int IMUI_Window_IsCollapsed (imui_window_t *window);
void IMUI_Window_SetSize (imui_window_t *window, int xlen, int ylen);

imui_ctx_t IMUI_NewContext (string font, float fontsize);
void IMUI_DestroyContext (imui_ctx_t ctx);

void IMUI_SetVisible (imui_ctx_t ctx, int visible);
void IMUI_SetSize (imui_ctx_t ctx, int xlen, int ylen);
/*bool*/int IMUI_ProcessEvent (imui_ctx_t ctx, /*const*/ struct IE_event_s *ie_event);
void IMUI_BeginFrame (imui_ctx_t ctx);
void IMUI_Draw (imui_ctx_t ctx);

int IMUI_PushLayout (imui_ctx_t ctx, int vertical);
void IMUI_PopLayout (imui_ctx_t ctx);
void IMUI_Layout_SetXSize (imui_ctx_t ctx, imui_size_t size, int value);
void IMUI_Layout_SetYSize (imui_ctx_t ctx, imui_size_t size, int value);

int IMUI_PushStyle (imui_ctx_t ctx, imui_style_t *style);
void IMUI_PopStyle (imui_ctx_t ctx);
void IMUI_Style_Update (imui_ctx_t ctx, imui_style_t *style);
void IMUI_Style_Fetch (imui_ctx_t ctx, imui_style_t *style);

void IMUI_Label (imui_ctx_t ctx, string label);
void IMUI_Labelf (imui_ctx_t ctx, string fmt, ...);
void IMUI_Passage (imui_ctx_t ctx, string name, int passage);
int IMUI_Button (imui_ctx_t ctx, string label);
int IMUI_Checkbox (imui_ctx_t ctx, int *flag, string label);
void IMUI_Radio (imui_ctx_t ctx, int *curvalue, int value, string label);
void IMUI_Slider (imui_ctx_t ctx, float *value, float minval, float maxval,
				  string label);
void IMUI_Spacer (imui_ctx_t ctx,
				  imui_size_t xsize, int xvalue,
				  imui_size_t ysize, int yvalue);

int IMUI_StartPanel (imui_ctx_t ctx, imui_window_t *panel);
int IMUI_ExtendPanel (imui_ctx_t ctx, string panel_name);
void IMUI_EndPanel (imui_ctx_t ctx);

int IMUI_StartMenu (imui_ctx_t ctx, imui_window_t *menu, int vertical);
void IMUI_EndMenu (imui_ctx_t ctx);
int IMUI_MenuItem (imui_ctx_t ctx, string label, int collapse);

int IMUI_StartWindow (imui_ctx_t ctx, imui_window_t *window);
void IMUI_EndWindow (imui_ctx_t ctx);

int IMUI_StartScrollBox (imui_ctx_t ctx, string name);
void IMUI_EndScrollBox (imui_ctx_t ctx);

void IMUI_ScrollBar (imui_ctx_t ctx, string name);

#define IMUI_DeferLoop(begin, end) \
	for (int _i_ = (begin); !_i_; _i_++, (end))

// #define IMUI_context to an imui_ctx_t * variable

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


#define UI_Style(style) \
	IMUI_DeferLoop (IMUI_PushStyle (IMUI_context, style), \
					IMUI_PopStyle (IMUI_context ))

#define UI_Horizontal UI_Layout(false)
#define UI_Vertical UI_Layout(true)

#endif//__ruamoko_imui_h
