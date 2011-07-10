#ifndef __SubMenu_h
#define __SubMenu_h

#include "gui/View.h"

@interface SubMenu : View
{
	View *title;
	string menu_name;
}
-(id)initWithBounds:(Rect)aRect title:(View*)aTitle menu:(string)name;
@end

#endif//__SubMenu_h
