#include "ZScrollView.h"

@implementation ZScrollView

/*
====================
initWithFrame: button:

Initizes a scroll view with a button at it's lower right corner
====================
*/

- initWithFrame:(NSRect)frameRect button1:b1
{
	[super  initWithFrame: frameRect];	

	[self addSubview: b1];

	button1 = b1;

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

- tile
{
	NSRect	scrollerframe;
	
	[super tile];
	scrollerframe = [_horizScroller frame];
	[button1 setFrame: scrollerframe];
	
	scrollerframe.size.width = 0;
	[_horizScroller setFrame: scrollerframe];

	return self;
}



-(BOOL) acceptsFirstResponder
{
    return YES;
}
/*
- superviewSizeChanged:(const NSSize *)oldSize
{
	[super superviewSizeChanged: oldSize];
	
	[[self documentView] newSuperBounds];
	
	return self;
}
*/


@end

