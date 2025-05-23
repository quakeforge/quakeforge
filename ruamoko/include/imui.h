#ifndef __ruamoko_imui_h
#define __ruamoko_imui_h

typedef @handle imui_ctx_h imui_ctx_t;

//FIXME should be in view.h but I need to create it
typedef enum {
	grav_center,	///< +ve X right, +ve Y down, -X left,  -ve Y up
	grav_north,		///< +ve X right, +ve Y down, -X left,  -ve Y up
	grav_northeast,	///< +ve X left,  +ve Y down, -X right, -ve Y up
	grav_east,		///< +ve X left,  +ve Y down, -X right, -ve Y up
	grav_southeast,	///< +ve X left,  +ve Y up,   -X right, -ve Y down
	grav_south,		///< +ve X right, +ve Y up,   -X left,  -ve Y down
	grav_southwest,	///< +ve X right, +ve Y up,   -X left,  -ve Y down
	grav_west,		///< +ve X right, +ve Y down, -X left,  -ve Y up
	grav_northwest,	///< +ve X right, +ve Y down, -X left,  -ve Y up
	grav_flow,		///< controlled by view_flow
} grav_t;

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

typedef struct imui_key_s {
	int         code;
	int         unicode;
	uint        shift;
} imui_key_t;

// opaque due to having C pointers, but allocated in progs space
typedef struct imui_window_s imui_window_t;

imui_window_t *IMUI_NewWindow (string name);
void IMUI_DeleteWindow (imui_window_t *window);

void IMUI_Window_SetPos (imui_window_t *window, ivec2 pos);
void IMUI_Window_SetSize (imui_window_t *window, ivec2 size);
void IMUI_Window_SetOpen (imui_window_t *window, bool isopen);
void IMUI_Window_SetCollapsed (imui_window_t *window, bool iscollapsed);
void IMUI_Window_SetNoCollapse (imui_window_t *window, bool nocollapse);
void IMUI_Window_SetAutoFit (imui_window_t *window, bool autofit);
void IMUI_Window_SetReference (imui_window_t *window, string reference);
void IMUI_Window_SetReferenceGravity (imui_window_t *window, grav_t gravity);
void IMUI_Window_SetAnchorGravity (imui_window_t *window, grav_t gravity);
void IMUI_Window_SetParent (imui_window_t *window, uint parent);

ivec2 IMUI_Window_GetPos (imui_window_t *window);
ivec2 IMUI_Window_GetSize (imui_window_t *window);
bool IMUI_Window_IsOpen (imui_window_t *window);
bool IMUI_Window_IsCollapsed (imui_window_t *window);
bool IMUI_Window_GetNoCollapse (imui_window_t *window);
bool IMUI_Window_GetAutoFit (imui_window_t *window);
string IMUI_Window_GetReference (imui_window_t *window);
grav_t IMUI_Window_GetReferenceGravity (imui_window_t *window);
grav_t IMUI_Window_GetAnchorGravity (imui_window_t *window);
uint IMUI_Window_GetParent (imui_window_t *window);

void IMUI_State_SetPos (imui_ctx_t ctx, string state, ivec2 pos);
void IMUI_State_SetLen (imui_ctx_t ctx, string state, ivec2 pos);
ivec2 IMUI_State_GetPos (imui_ctx_t ctx, string state);
ivec2 IMUI_State_GetLen (imui_ctx_t ctx, string state);

bool IMUI_GetKey (imui_ctx_t ctx, imui_key_t *key);

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

int IMUI_CheckButtonState (imui_ctx_t ctx);
int IMUI_UpdateHotActive (imui_ctx_t ctx);

ivec2 IMUI_TextSize (imui_ctx_t ctx, string str);

void IMUI_SetFocus (imui_ctx_t ctx, bool focus);
void IMUI_SetFill (imui_ctx_t ctx, uint color);
void IMUI_Label (imui_ctx_t ctx, string label);
void IMUI_Labelf (imui_ctx_t ctx, string fmt, ...);
void IMUI_IntLabel (imui_ctx_t ctx, int *istr, int len);
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
int IMUI_StartScroller (imui_ctx_t ctx);
void IMUI_EndScroller (imui_ctx_t ctx);

void IMUI_SetViewPos (imui_ctx_t ctx, ivec2 pos);
void IMUI_SetViewLen (imui_ctx_t ctx, ivec2 len);
void IMUI_SetViewFree (imui_ctx_t ctx, bvec2 free);

#define IMUI_DeferLoop(begin, end) \
	for (int _i_ = (begin); !_i_; _i_++, (end))

// #define IMUI_context to an imui_ctx_t * variable

#define UI_SetFill(color) \
	IMUI_SetFill(IMUI_context, color)

#define UI_Label(label) \
	IMUI_Label(IMUI_context, label)

#define UI_Labelf(fmt, ...) \
	IMUI_Labelf(IMUI_context, fmt __VA_OPT__(, __VA_ARGS__))

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

#endif//__ruamoko_imui_h
