#import "qedefs.h"

@implementation ZScrollView

/*
====================
initWithFrame: button:

Initizes a scroll view with a button at it's lower right corner
====================
*/

- initWithFrame:(const NSRect *)frameRect button1:b1
{
	[super  initWithFrame: frameRect];	

	[self addSubview: b1];

	button1 = b1;

	[self setHorizScrollerRequired: YES];
	[self setVertScrollerRequired: YES];

	[self setBorderType: NS_BEZEL];
		
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
	[_horizScroller getFrame: &scrollerframe];
	[button1 setFrame: &scrollerframe];
	
	scrollerframe.size.width = 0;
	[_horizScroller setFrame: &scrollerframe];

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

