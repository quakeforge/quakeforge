#include <QF/keys.h>
#include "menu.h"
#include "SubMenu.h"

@implementation SubMenu

-(id)initWithBounds:(Rect)aRect title:(View *)aTitle menu:(string)name
{
	self = [super initWithBounds:aRect];
	if (!self)
		return self;

	title = aTitle;
	menu_name = name;
	return self;
}

- (int) keyEvent:(int)key unicode:(int)unicode down:(int)down
{
	if (key == QFK_RETURN) {
		Menu_SelectMenu (menu_name);
		return 1;
	}
	return 0;
}

- (void) draw
{
	[title draw];
}

- (void) setBasePosFromView: (View *) aview
{
    [super setBasePosFromView:aview];
	[title setBasePosFromView:self];
}

@end
