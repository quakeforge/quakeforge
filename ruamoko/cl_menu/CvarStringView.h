#ifndef __CvarStringView_h
#define __CvarStringView_h

#include "gui/Group.h"

@class Text;
@class InputLineBox;
@class CvarString;

@interface CvarStringView : Group
{
	Text *title;
	InputLineBox *ilb;
	CvarString *cvstring;
	int active;
}
-(void)update;
-(id)initWithBounds:(Rect)aRect title:(string)_title inputLength: (int)length :(CvarString *)_cvstring;
@end

#endif//__CvarStringView_h
