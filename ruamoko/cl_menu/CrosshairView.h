#ifndef __CrosshairView_h
#define __CrosshairView_h

#include "gui/View.h"
#include "CrosshairCvar.h"

@interface CrosshairView : View
{
	CrosshairCvar *crosshair;
}
-(id)initWithBounds:(Rect)aRect :(CrosshairCvar*)_crosshair;
-(void) next;
@end

#endif//__CrosshairView_h
