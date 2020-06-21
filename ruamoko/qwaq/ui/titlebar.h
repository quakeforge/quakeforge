#ifndef __qwaq_ui_titlebar_h
#define __qwaq_ui_titlebar_h

#include "ruamoko/qwaq/ui/view.h"

@interface TitleBar : View
{
	string      title;
	int         length;
}
// title always centered at top of owner
+(TitleBar *)withTitle:(string)title;
-initWithTitle:(string)title;
-setTitle:(string)newTitle;
@end

#endif//__qwaq_ui_titlebar_h
