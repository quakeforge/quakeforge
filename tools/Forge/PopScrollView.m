
#import "qedefs.h"

@implementation PopScrollView

/*
====================
initFrame: button:

Initizes a scroll view with a button at it's lower right corner
====================
*/

- (id) initWithFrame: (NSRect) frameRect button1: (NSButton *) b1 button2: (NSButton *) b2
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

- (void) tile
{
	NSRect	scrollerframe;
	NSRect	buttonframe, buttonframe2;
	NSRect	newframe;

	[super tile];
	[button1 setFrame: buttonframe];
	[button2 setFrame: buttonframe2];
	[[self horizontalScroller] setFrame: scrollerframe];

	newframe.origin.y = scrollerframe.origin.y;
	newframe.origin.x = [self frame].size.width - buttonframe.size.width;
	newframe.size.width = buttonframe.size.width;
	newframe.size.height = scrollerframe.size.height;
	scrollerframe.size.width -= newframe.size.width;
	[button1 setFrame: newframe];
	newframe.size.width = buttonframe2.size.width;
	newframe.origin.x -= newframe.size.width;
	[button2 setFrame: newframe];
	scrollerframe.size.width -= newframe.size.width;

	[[self horizontalScroller] setFrame: scrollerframe];

	return;
}


- (void) resizeWithOldSuperviewSize: (NSSize) oldFrameSize
{
	[super resizeWithOldSuperviewSize: oldFrameSize];
	
	[[self documentView] newSuperBounds];
	
	return;
}


- (BOOL) acceptsFirstResponder
{
    return YES;
}

@end

