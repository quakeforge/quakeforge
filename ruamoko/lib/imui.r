#include "imui.h"

imui_window_t *IMUI_NewWindow (string name) = #0;
void IMUI_DeleteWindow (imui_window_t *window) = #0;

imui_ctx_t IMUI_NewContext (string font, float fontsize) = #0;
void IMUI_DestroyContext (imui_ctx_t ctx) = #0;
int IMUI_Window_IsOpen (imui_window_t *window) = #0;
int IMUI_Window_IsCollapsed (imui_window_t *window) = #0;
void IMUI_Window_SetSize (imui_window_t *window, int xlen, int ylen) = #0;

void IMUI_SetVisible (imui_ctx_t ctx, int visible) = #0;
void IMUI_SetSize (imui_ctx_t ctx, int xlen, int ylen) = #0;
int IMUI_ProcessEvent (imui_ctx_t ctx, /*const*/ struct IE_event_s *ie_event) = #0;
void IMUI_BeginFrame (imui_ctx_t ctx) = #0;
void IMUI_Draw (imui_ctx_t ctx) = #0;

int IMUI_PushLayout (imui_ctx_t ctx, int vertical) = #0;
void IMUI_PopLayout (imui_ctx_t ctx) = #0;
void IMUI_Layout_SetXSize (imui_ctx_t ctx, imui_size_t size, int value) = #0;
void IMUI_Layout_SetYSize (imui_ctx_t ctx, imui_size_t size, int value) = #0;

int IMUI_PushStyle (imui_ctx_t ctx, imui_style_t *style) = #0;
void IMUI_PopStyle (imui_ctx_t ctx) = #0;
void IMUI_Style_Update (imui_ctx_t ctx, imui_style_t *style) = #0;
void IMUI_Style_Fetch (imui_ctx_t ctx, imui_style_t *style) = #0;

void IMUI_Label (imui_ctx_t ctx, string label) = #0;
void IMUI_Labelf (imui_ctx_t ctx, string fmt, ...) = #0;
void IMUI_Passage (imui_ctx_t ctx, string name, int passage) = #0;
int IMUI_Button (imui_ctx_t ctx, string label) = #0;
int IMUI_Checkbox (imui_ctx_t ctx, int *flag, string label) = #0;
void IMUI_Radio (imui_ctx_t ctx, int *curvalue, int value, string label) = #0;
void IMUI_Slider (imui_ctx_t ctx, float *value, float minval, float maxval,
				  string label) = #0;
void IMUI_Spacer (imui_ctx_t ctx,
				  imui_size_t xsize, int xvalue,
				  imui_size_t ysize, int yvalue) = #0;

int IMUI_StartPanel (imui_ctx_t ctx, imui_window_t *panel) = #0;
int IMUI_ExtendPanel (imui_ctx_t ctx, string panel_name) = #0;
void IMUI_EndPanel (imui_ctx_t ctx) = #0;

int IMUI_StartMenu (imui_ctx_t ctx, imui_window_t *menu, int vertical) = #0;
void IMUI_EndMenu (imui_ctx_t ctx) = #0;
int IMUI_MenuItem (imui_ctx_t ctx, string label, int collapse) = #0;

int IMUI_StartWindow (imui_ctx_t ctx, imui_window_t *window) = #0;
void IMUI_EndWindow (imui_ctx_t ctx) = #0;

int IMUI_StartScrollBox (imui_ctx_t ctx, string name) = #0;
void IMUI_EndScrollBox (imui_ctx_t ctx) = #0;

void IMUI_ScrollBar (imui_ctx_t ctx, string name) = #0;
