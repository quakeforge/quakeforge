#include "InputLine.h"

inputline_t (integer lines, integer size, integer prompt) InputLine_Create = #0;
void (inputline_t il, void [] data) InputLine_SetUserData = #0;
void (inputline_t il, integer width) InputLine_SetWidth = #0;
void (inputline_t il) InputLine_Destroy = #0;
void (inputline_t il) InputLine_Clear = #0;
void (inputline_t il, integer ch) InputLine_Process = #0;
#ifdef OLD_API
void (inputline_t il, integer x, integer y, integer cursor) InputLine_Draw = #0;
#else
void (inputline_t il, integer cursor) InputLine_Draw = #0;
#endif
void (inputline_t il, string str) InputLine_SetText = #0;
string (inputline_t il) InputLine_GetText = #0;

@implementation InputLine

- (id) initWithBounds: (Rect)aRect promptCharacter: (integer)char
{
	id (self) = [super init];
	id (frame) = [aRect copy];

	il = InputLine_Create (frame.size.height, frame.size.width, char);
	InputLine_SetUserData (il, frame);

	return self;
}

- (void) free
{
	[frame free];
	InputLine_Destroy (il);
	[super free];
}

- (void) setWidth: (integer)visibleWidth
{
	InputLine_SetWidth (il, width);
}

- (void) processInput: (integer)key
{
	InputLine_Process (il, key);
}

- (void) draw: (BOOL)cursor
{
#ifdef OLD_API
	InputLine_Draw (il, frame.origin.x, frame.origin.y, cursor);
#else
	InputLine_Draw (il, cursor);
#endif
}

- (void) setText: (string)text
{
	InputLine_SetText (il, text);
}

- (string) text
{
	return InputLine_GetText (il);
}

@end
