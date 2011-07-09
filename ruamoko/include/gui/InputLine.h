#ifndef __ruamoko_gui_InputLine_h
#define __ruamoko_gui_InputLine_h

#include "View.h"

struct _inputline_t {};	// opaque type :)
typedef struct _inputline_t *inputline_t;

@extern inputline_t (int lines, int size, int prompt) InputLine_Create;
@extern void InputLine_SetUserData (inputline_t il, void *data);
@extern @overload void InputLine_SetEnter (inputline_t il, void (f)(string, void*), void *data);
@extern @overload void InputLine_SetEnter (inputline_t il, IMP imp, id obj, SEL sel);
@extern void (inputline_t il, int width) InputLine_SetWidth;
@extern void (inputline_t il) InputLine_Destroy;
@extern void (inputline_t il, int save) InputLine_Clear;
@extern void (inputline_t il, int ch) InputLine_Process;
@extern void (inputline_t il) InputLine_Draw;
@extern void (inputline_t il, string str) InputLine_SetText;
@extern string (inputline_t il) InputLine_GetText;

struct il_data_t {
	int		x, y;
	BOOL		cursor;
};

@interface InputLine: View
{
	struct il_data_t   control;
	inputline_t	il;
}

- (id) initWithBounds: (Rect)aRect promptCharacter: (int)char;

- (void) setBasePosFromView: (View *) view;
- (void) setWidth: (int)width;
- (void) setEnter: obj message:(SEL) msg;
- (void) cursor: (BOOL)cursor;
- (void) draw;

- (void) processInput: (int)key;

- (id) setText: (string)text;
- (string) text;

@end

@interface InputLineBox: View
{
	InputLine *input_line;
}
- (id) initWithBounds: (Rect)aRect promptCharacter: (int)char;

- (void) setWidth: (int)width;
- (void) setEnter: obj message:(SEL) msg;
- (void) cursor: (BOOL)cursor;

- (void) processInput: (int)key;

- (id) setText: (string)text;
- (string) text;
@end

#endif //__ruamoko_gui_InputLine_h
