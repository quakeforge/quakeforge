#ifndef __CvarToggleView_h
#define __CvarToggleView_h

#include "gui/Group.h"

@class Text;
@class CvarToggle;

@interface CvarToggleView : Group
{
	Text *title;
	Text *value;
	CvarToggle *toggle;
}
-(void)update;
-(id)initWithBounds:(Rect)aRect title:(string)_title :(CvarToggle *)_toggle;
-(void)toggle;
@end

#endif//__CvarToggleView_h
