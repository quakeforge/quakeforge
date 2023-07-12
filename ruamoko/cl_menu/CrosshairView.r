#include "draw.h"
#include "key.h"
#include "sound.h"

#include "CrosshairView.h"

@implementation CrosshairView
{
	CrosshairCvar *crosshair;
}

-(id)initWithBounds:(Rect)aRect :(CrosshairCvar*)_crosshair
{
	self = [self initWithBounds:aRect];
	crosshair = _crosshair;
	return self;
}

-(void) next
{
	[crosshair next];
	S_LocalSound ("misc/menu2.wav");
}

-(void) draw
{
	Draw_Fill (xabs, yabs, xlen, ylen, 0);
	Draw_Crosshair ([crosshair crosshair], xabs + xlen / 2, yabs + ylen / 2);
}

- (int) keyEvent:(int)key unicode:(int)unicode down:(int)down
{
	switch (key) {
		case QFK_RETURN:
		//case QFM_BUTTON1:
			[self next];
			return 1;
		default:
			return 0;
	}
}
@end
