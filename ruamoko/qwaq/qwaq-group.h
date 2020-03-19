#ifndef __qwaq_group_h
#define __qwaq_group_h

#include "qwaq-view.h"

@interface Group : View
{
	Array      *views;
	int         focused;
	id<TextContext> buffer;
}
-initWithContext: (id<TextContext>) context;
-insert: (View *) view;
-remove: (View *) view;
@end

#endif//__qwaq_group_h
