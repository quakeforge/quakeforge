#ifndef __ruamoko_InputLine_h
#define __ruamoko_InputLine_h

#include "Object.h"

struct _inputline_t = {};	// opaque type :)
typedef struct _inputline_t [] inputline_t;

@extern inputline_t (integer lines, integer size, integer prompt) InputLine_Create;
@extern void (inputline_t il, void [] data) InputLine_SetUserData;
@extern void (inputline_t il, integer width) InputLine_SetWidth;
@extern void (inputline_t il) InputLine_Destroy;
@extern void (inputline_t il, integer save) InputLine_Clear;
@extern void (inputline_t il, integer ch) InputLine_Process;
@extern void (inputline_t il) InputLine_Draw;
@extern void (inputline_t il, string str) InputLine_SetText;
@extern string (inputline_t il) InputLine_GetText;

struct il_data_t = {
	integer		x, y;
	integer		xbase, ybase;
	BOOL		cursor;
};

@class Rect;

@interface InputLine: Object
{
	struct il_data_t   control;
	inputline_t	il;
}

- (id) initWithBounds: (Rect)aRect promptCharacter: (integer)char;
//-initAt:(Point)p HistoryLines:(integer)l LineSize:(integer)s PromptChar:(integer)prompt;
- (void) free;

- (void) setBasePos: (integer) x y: (integer) y;
- (void) setWidth: (integer)width;
- (void) draw: (BOOL)cursor;

- (void) processInput: (integer)key;

- (id) setText: (string)text;
- (string) text;

@end

#endif //__ruamoko_inputline_h
