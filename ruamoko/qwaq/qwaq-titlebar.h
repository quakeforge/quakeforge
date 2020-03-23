#ifndef __qwaq_titlebar_h
#define __qwaq_titlebar_h

#include "qwaq-view.h"

@interface TitleBar : View
{
	string      title;
	int         length;
}
// title always centered at top of owner
-initWithTitle:(string) title;
-setTitle:(string) newTitle;
@end

#endif//__qwaq_titlebar_h
