#ifndef __CvarColorView_h
#define __CvarColorView_h

#include "gui/View.h"

@class CvarColor;

@interface CvarColorView : View
{
	CvarColor  *color;
}
-(id)initWithBounds:(Rect)aRect :(CvarColor *)_color;
-(void)next;
-(void)prev;
@end

#endif//__CvarColorView_h
