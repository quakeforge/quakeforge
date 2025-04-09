#include <QF/keys.h>
#include "draw.h"
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
	local int xl;
	xl = xlen / 8 - 2;
	text_box (xabs, yabs, xl, ylen / 8 - 2);
	xl = (xl + 1) & ~1;		// text_box does only multiples of 2
	Draw_Fill (xabs + 8, yabs + 8, xl * 8, ylen - 16,
			   [color value] * 16 + 8);
}

- (int) keyEvent:(int)key unicode:(int)unicode down:(int)down
{
	switch (key) {
		case QFK_RIGHT:
			[self next];
			return 1;
		case QFK_LEFT:
			[self prev];
			return 1;
	}
	return 0;
}
@end
