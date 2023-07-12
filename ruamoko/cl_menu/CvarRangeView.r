#include "key.h"
#include "sound.h"
#include "legacy_string.h"
#include "string.h"

#include "gui/Text.h"
#include "gui/Slider.h"
#include "CvarRangeView.h"
#include "CvarRange.h"

@implementation CvarRangeView

-(void)update
{
	[slider setIndex:[range percentage]];
	[value setText:ftos ([range value])];
}

-(id)initWithBounds:(Rect)aRect title:(string)_title sliderWidth:(int)width :(CvarRange *)_range
{
	local Rect rect;

	self = [super initWithBounds:aRect];

	range = _range;
	
	rect = makeRect (0, 0, strlen (_title) * 8, 8);
	title = [[Text alloc] initWithBounds:rect text:_title];

	rect.origin.x += rect.size.width + 8;
	rect.size.width = width;
	if (rect.origin.x + rect.size.width > xlen)
		rect.size.width = xlen - rect.origin.x;
	slider = [[Slider alloc] initWithBounds:rect size:100];

	rect.origin.x += rect.size.width + 8;
	rect.size.width = xlen - rect.origin.x;
	value = [[Text alloc] initWithBounds:rect];

	[self addView:title];
	[self addView:slider];
	[self addView:value];

	[self update];

	return self;
}

-(void)inc
{
	[range inc];
	[self update];
	S_LocalSound ("misc/menu3.wav");
}

-(void)dec
{
	[range dec];
	[self update];
	S_LocalSound ("misc/menu3.wav");
}

- (int) keyEvent:(int)key unicode:(int)unicode down:(int)down
{
	switch (key) {
		case QFK_RIGHT:
			[self inc];
			return 1;
		case QFK_LEFT:
			[self dec];
			return 1;
	}
	return 0;
}

@end
