#ifndef __ruamoko_InputLine_h
#define __ruamoko_InputLine_h

#include "Rect.h"

#define OLD_API		// FIXME update the input line api

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

@interface InputLine: Object
{
	Rect		frame;
	inputline_t	il;
}

- (id) initWithBounds: (Rect)aRect promptCharacter: (integer)char;
//-initAt:(Point)p HistoryLines:(integer)l LineSize:(integer)s PromptChar:(integer)prompt;
- (void) free;

- (void) setWidth: (integer)width;
- (void) draw: (BOOL)cursor;

- (void) processInput: (integer)key;

- (id) setText: (string)text;
- (string) text;

@end

#endif //__ruamoko_inputline_h
