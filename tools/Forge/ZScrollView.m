#import "qedefs.h"

@implementation ZScrollView

/*
====================
initFrame: button:

Initizes a scroll view with a button at it's lower right corner
====================
*/

- initFrame:(const NSRect *)frameRect button1:b1
{
	[super  initFrame: frameRect];	

	[self addSubview: b1];

	button1 = b1;

	[self setHorizScrollerRequired: YES];
	[self setVertScrollerRequired: YES];

	[self setBorderType: NX_BEZEL];
		
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
	[hScroller getFrame: &scrollerframe];
	[button1 setFrame: &scrollerframe];
	
	scrollerframe.size.width = 0;
	[hScroller setFrame: &scrollerframe];

	return self;
}



-(BOOL) acceptsFirstResponder
{
    return YES;
}

- superviewSizeChanged:(const NSSize *)oldSize
{
	[super superviewSizeChanged: oldSize];
	
	[[self docView] newSuperBounds];
	
	return self;
}



@end

