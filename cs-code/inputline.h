#ifndef __inputline_h
#define __inputline_h

struct _inputline_t = {};	// opaque type :)
typedef _inputline_t [] inputline_t;

@extern inputline_t (integer lines, integer size, integer prompt) InputLine_Create;
@extern void (inputline_t il, integer width) InputLine_SetWidth;
@extern void (inputline_t il) InputLine_Destroy;
@extern void (inputline_t il) InputLine_Clear;
@extern void (inputline_t il, integer ch) InputLine_Process;
@extern void (inputline_t il, integer x, integer y, integer cursor) InputLine_Draw;
@extern void (inputline_t il, string str) InputLine_SetText;
@extern string (inputline_t il) InputLine_GetText;

#endif//__inputline-h
