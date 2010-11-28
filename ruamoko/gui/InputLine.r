#include "draw.h"
#include "gui/InputLine.h"
#include "gui/Rect.h"

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
	self = [super initWithComponents:aRect.origin.x :aRect.origin.y :aRect.size.width * 8 :8];
	control.x = xpos;
	control.y = ypos;
	control.xbase = control.ybase = 0;
	control.cursor = NO;

	il = InputLine_Create (aRect.size.height, aRect.size.width, char);
	InputLine_SetUserData (il, &control);

	return self;
}

- (void) dealloc
{
	InputLine_Destroy (il);
	[super dealloc];
}

- (void) setBasePos: (Point)pos
{
	[super setBasePos:pos];
	control.xbase = pos.x;
	control.ybase = pos.y;
}

- (void) setWidth: (integer)width
{
	InputLine_SetWidth (il, width);
}

- (void) processInput: (integer)key
{
	InputLine_Process (il, key);
}

- (void) cursor: (BOOL)cursor
{
	control.cursor = cursor;
}

- (void) draw
{
	InputLine_Draw (il);
}

- (id) setText: (string)text
{
	InputLine_SetText (il, text);
	return self;
}

- (string) text
{
	return InputLine_GetText (il);
}

@end

@implementation InputLineBox
- (id) initWithBounds: (Rect)aRect promptCharacter: (integer)char
{
	local integer xp, yp, xl, yl;
	local Rect r;

	xp = aRect.origin.x;
	yp = aRect.origin.y;
	xl = (aRect.size.width - 2) * 8;
	yl = 24;
	self = [self initWithComponents:xp :yp :xl :yl];

	xp = 0;
	yp = 8;
	xl = aRect.size.width;
	yl = aRect.size.height;
	r = makeRect (xp, yp, xl, yl);
	input_line = [[InputLine alloc] initWithBounds:r promptCharacter:char];
	return self;
}

- (void) setWidth: (integer)width
{
	[input_line setWidth:width];
}

- (void) cursor: (BOOL)cursor
{
	[input_line cursor:cursor];
}

- (void) processInput: (integer)key
{
	[input_line processInput:key];
}

- (id) setText: (string)text
{
	return [input_line setText:text];
}

- (string) text
{
	return [input_line text];
}

- (void) setBasePos:(Point)pos
{
	[super setBasePos:pos];
	[input_line setBasePos:xabs y:yabs];
}

- (void) draw
{
	[super draw];
	text_box (xabs, yabs, xlen / 8, ylen / 24);
	[input_line draw];
}
@end
