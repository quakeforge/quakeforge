#include "View.h"

@implementation View

-(id) initWithComponents: (integer)x : (integer)y : (integer)w : (integer)h
{
	xpos = xabs = x;
	ypos = yabs = y;
	xlen = w;
	ylen = y;
	parent = NIL;
	return self;
}

-(void) draw
{
}

@end
