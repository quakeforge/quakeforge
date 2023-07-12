#include <gui/Text.h>
#include <string.h>
#include <draw.h>

@implementation Text
- (id) init
{
	text = str_new ();
	return self;
}

- (void) dealloc
{
	str_free (text);
	[super dealloc];
}

- (id) initWithBounds: (Rect)aRect
{
	return [self initWithBounds:aRect text:""];
}

- (id) initWithBounds: (Rect)aRect text:(string)str
{
	self = [super initWithBounds:aRect];
	str_copy (text, str);
	return self;
}

- (void) setText: (string)str
{
	str_copy (text, str);
}

- (void) draw
{
	local int maxlen = xlen / 8;
	Draw_nString (xabs, yabs, text, maxlen);
}

@end
