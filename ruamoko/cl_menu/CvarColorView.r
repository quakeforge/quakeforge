#include "draw.h"
#include "key.h"
#include "sound.h"

#include "CvarColorView.h"
#include "CvarColor.h"

@implementation CvarColorView
-(id)initWithBounds:(Rect)aRect :(CvarColor *)_color
{
	self = [self initWithBounds:aRect];
	color = _color;
	return self;
}

-(void)next
{
	[color next];
	S_LocalSound ("misc/menu2.wav");
}

-(void)prev
{
	[color prev];
	S_LocalSound ("misc/menu2.wav");
}

-(void)draw
{
	local integer xl;
	xl = xlen / 8 - 2;
	text_box (xabs, yabs, xl, ylen / 8 - 2);
	xl = (xl + 1) & ~1;		// text_box does only multiples of 2
	Draw_Fill (xabs + 8, yabs + 8, xl * 8, ylen - 16,
			   [color value] * 16 + 8);
}

- (integer) keyEvent:(integer)key unicode:(integer)unicode down:(integer)down
{
	switch (key) {
		case QFK_DOWN:
		case QFM_WHEEL_DOWN:
			[self next];
			return 1;
		case QFK_UP:
		case QFM_WHEEL_UP:
			[self prev];
			return 1;
	}
	return 0;
}
@end
