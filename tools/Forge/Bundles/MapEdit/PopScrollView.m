#include "PopScrollView.h"

@implementation PopScrollView
/*
====================
initWithFrame: button:

Initizes a scroll view with a button at it's lower right corner
====================
*/
- (id) initWithFrame: (NSRect)frameRect
   button1: b1
   button2: b2
{
	[super initWithFrame: frameRect];

	[self addSubview: b1];
	[self addSubview: b2];

	button1 = b1;
	button2 = b2;

	[self setHasHorizontalScroller: YES];
	[self setHasVerticalScroller: YES];

	[self setBorderType: NSBezelBorder];

	return self;
}

/*
================
tile

Adjust the size for the pop up scale menu
=================
*/
- (id) tile
{
	NSRect  scrollerframe;
	NSRect  buttonframe, buttonframe2;
	NSRect  newframe;

	[super tile];
	buttonframe = [button1 frame];
	buttonframe2 = [button2 frame];
	scrollerframe = [_horizScroller frame];

	newframe.origin.y = scrollerframe.origin.y;
	newframe.origin.x = scrollerframe.origin.x + scrollerframe.size.width -
	                    buttonframe.size.width;
	newframe.size.width = buttonframe.size.width;
	newframe.size.height = scrollerframe.size.height;
	scrollerframe.size.width -= newframe.size.width;
	[button1 setFrame: newframe];
	newframe.size.width = buttonframe2.size.width;
	newframe.origin.x -= newframe.size.width;
	[button2 setFrame: newframe];
	scrollerframe.size.width -= newframe.size.width;

	[_horizScroller setFrame: scrollerframe];

	return self;
}

/*
- (id) superviewSizeChanged: (const NSSize *)oldSize
{
    [super superviewSizeChanged: oldSize];

    [[self docView] newSuperBounds];

    return self;
}
*/

- (BOOL) acceptsFirstResponder
{
	return YES;
}

@end
