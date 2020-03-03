#include <draw.h>
#include <gui/InputLine.h>
#include <gui/Rect.h>

inputline_t InputLine_Create (int lines, int size, int prompt) = #0;
void InputLine_SetPos (inputline_t il, int x, int y) = #0;
void InputLine_SetCursor (inputline_t il, int cursorr) = #0;
@overload void InputLine_SetEnter (inputline_t il, void (f)(string, void*), void *data) = #0;
@overload void InputLine_SetEnter (inputline_t il, IMP imp, id obj, SEL sel) = #0;
void InputLine_SetWidth (inputline_t il, int width) = #0;
void InputLine_Destroy (inputline_t il) = #0;
void InputLine_Clear (inputline_t il, int size) = #0;
void InputLine_Process (inputline_t il, int ch) = #0;
void InputLine_Draw (inputline_t il) = #0;
void InputLine_SetText (inputline_t il, string str) = #0;
string InputLine_GetText (inputline_t il) = #0;

@implementation InputLine

- (id) initWithBounds: (Rect)aRect promptCharacter: (int)char
{
	self = [super initWithComponents:aRect.origin.x :aRect.origin.y :aRect.size.width * 8 :8];

	il = InputLine_Create (aRect.size.height, aRect.size.width, char);
	InputLine_SetPos (il, xabs, yabs);
	InputLine_SetCursor (il, NO);

	return self;
}

- (void) dealloc
{
	InputLine_Destroy (il);
	[super dealloc];
}

- (void) setBasePosFromView: (View *) view
{
	[super setBasePosFromView: view];
	InputLine_SetPos (il, xabs, yabs);
}

- (void) setWidth: (int)width
{
	InputLine_SetWidth (il, width);
}

- (void) setEnter: obj message:(SEL) msg
{
	IMP imp = [obj methodForSelector: msg];
	InputLine_SetEnter (il, imp, obj, msg);
}

- (void) processInput: (int)key
{
	InputLine_Process (il, key);
}

- (void) cursor: (BOOL)cursor
{
	InputLine_SetCursor (il, cursor);
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
- (id) initWithBounds: (Rect)aRect promptCharacter: (int)char
{
	local int xp, yp, xl, yl;
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

- (void) setWidth: (int)width
{
	[input_line setWidth:width];
}

- (void) setEnter: obj message:(SEL) msg
{
	[input_line setEnter:obj message: msg];
}

- (void) cursor: (BOOL)cursor
{
	[input_line cursor:cursor];
}

- (void) processInput: (int)key
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

- (void) setBasePosFromView: (View *) view
{
	[super setBasePosFromView: view];
	[input_line setBasePosFromView: self];
}

- (void) draw
{
	[super draw];
	text_box (xabs, yabs, xlen / 8, ylen / 24);
	[input_line draw];
}
@end
