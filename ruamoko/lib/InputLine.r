#include "InputLine.h"

#include "Rect.h"

inputline_t (integer lines, integer size, integer prompt) InputLine_Create = #0;
void (inputline_t il, void [] data) InputLine_SetUserData = #0;
void (inputline_t il, integer width) InputLine_SetWidth = #0;
void (inputline_t il) InputLine_Destroy = #0;
void (inputline_t il, integer size) InputLine_Clear = #0;
void (inputline_t il, integer ch) InputLine_Process = #0;
void (inputline_t il) InputLine_Draw = #0;
void (inputline_t il, string str) InputLine_SetText = #0;
string (inputline_t il) InputLine_GetText = #0;

@implementation InputLine

- (id) initWithBounds: (Rect)aRect promptCharacter: (integer)char
{
	id (self) = [super init];
	control.x = aRect.origin.x;
	control.y = aRect.origin.y;
	control.cursor = NO;

	il = InputLine_Create (aRect.size.height, aRect.size.width, char);
	InputLine_SetUserData (il, &control);

	return self;
}

- (void) free
{
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
	control.cursor = cursor;
	InputLine_Draw (il);
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
