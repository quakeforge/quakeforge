#include "gui/Text.h"
#include "string.h"
#include "draw.h"

@implementation Text
- (id) initWithBounds: (Rect)aRect
{
	return [self initWithBounds:aRect text:""];
}

- (id) initWithBounds: (Rect)aRect text:(string)str
{
	self = [super initWithBounds:aRect];
	text = str_new ();
	str_copy (text, str);
	return self;
}

- (void) dealloc
{
	str_free (text);
}

- (void) setText: (string)str
{
	str_copy (text, str);
}

- (void) draw
{
	local integer maxlen = xlen / 8;
	Draw_nString (xabs, yabs, text, maxlen);
}

@end
