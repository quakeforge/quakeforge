#include <QF/keys.h>
#include "sound.h"
#include "string.h"

#include "gui/Text.h"
#include "gui/InputLine.h"
#include "CvarStringView.h"
#include "CvarString.h"

@implementation CvarStringView

-enter: (string) line
{
	[cvstring setString: line];
	return self;
}

-(void)update
{
	[ilb setText: [cvstring value]];
}

-(id)initWithBounds:(Rect)aRect title:(string)_title inputLength: (int)length :(CvarString *)_cvstring
{
	Rect rect;

	self = [super initWithBounds:aRect];

	cvstring = _cvstring;

	rect = makeRect (0, 0, strlen (_title) * 8, 8);
	title = [[Text alloc] initWithBounds:rect text:_title];

	rect.origin.x += rect.size.width + 8;
	rect.origin.y = -8;
	rect.size.width = (aRect.size.width - rect.size.width) / 8 - 2;
	rect.size.height = 4;	// history lines (stupid interface:P)
	ilb = [[InputLineBox alloc] initWithBounds:rect promptCharacter:' '];
	[ilb setEnter: self message:@selector(enter:)];

	[self addView:title];
	[self addView:ilb];

	[self update];

	return self;
}

- (int) keyEvent:(int)key unicode:(int)unicode down:(int)down
{
	if (active) {
		if (key == QFK_ESCAPE) {
			[self update];
			active = 0;
			[ilb cursor: NO];
		} else {
			[ilb processInput:(key >= 256 ? key : unicode)];
			if (key == QFK_RETURN) {
				[self update];
				active = 0;
				[ilb cursor: NO];
			}
		}
		return 1;
	} else {
		if (key == QFK_RETURN) {
			active = 1;
			[ilb cursor: YES];
		}
	}
	return 0;
}

@end
