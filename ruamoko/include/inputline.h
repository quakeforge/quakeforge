#ifndef __ruamoko_inputline_h
#define __ruamoko_inputline_h

#include "point.h"

#define OLD_API		//FIXME update the input line api

struct _inputline_t = {};	// opaque type :)
typedef _inputline_t [] inputline_t;

@extern inputline_t (integer lines, integer size, integer prompt) InputLine_Create;
@extern void (inputline_t il, void [] data) InputLine_SetUserData;
@extern void (inputline_t il, integer width) InputLine_SetWidth;
@extern void (inputline_t il) InputLine_Destroy;
@extern void (inputline_t il) InputLine_Clear;
@extern void (inputline_t il, integer ch) InputLine_Process;
#ifdef OLD_API
@extern void (inputline_t il, integer x, integer y, integer cursor) InputLine_Draw;
#else
@extern void (inputline_t il, integer cursor) InputLine_Draw;
#endif
@extern void (inputline_t il, string str) InputLine_SetText;
@extern string (inputline_t il) InputLine_GetText;

@interface InputLine : Object
{
	Point at;
	inputline_t il;
}
-free;
-initAt:(Point)p HistoryLines:(integer)l LineSize:(integer)s PromptChar:(integer)prompt;
-setWidth:(integer)visibleWidth;
-process:(integer)key;
-draw:(BOOL)cursor;
-setText:(string)text;
-(string)getText;
@end

#endif//__ruamoko_inputline_h
