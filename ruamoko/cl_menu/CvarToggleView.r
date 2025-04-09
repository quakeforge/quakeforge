#include <QF/keys.h>
#include "sound.h"
#include "string.h"

#include "gui/Text.h"
#include "CvarToggleView.h"
#include "CvarToggle.h"

@implementation CvarToggleView

-(void)update
{
	[value setText:[toggle value] ? "On" : "Off"];
}

-(id)initWithBounds:(Rect)aRect title:(string)_title :(CvarToggle *)_toggle
{
	local Rect rect;

	self = [super initWithBounds:aRect];

	toggle = _toggle;
	
	rect = makeRect (0, 0, strlen (_title) * 8, 8);
	title = [[Text alloc] initWithBounds:rect text:_title];

	rect.size.width = 3 * 8;
	rect.origin.x = xlen - rect.size.width;
	value = [[Text alloc] initWithBounds:rect];

	[self addView:title];
	[self addView:value];

	[self update];

	return self;
}

-(void)toggle
{
	[toggle toggle];
	[self update];
	S_LocalSound ("misc/menu3.wav");
}

- (int) keyEvent:(int)key unicode:(int)unicode down:(int)down
{
	switch (key) {
		case QFK_RETURN:
		//case QFM_BUTTON1:
			[self toggle];
			return 1;
		default:
			return 0;
	}
}

@end
