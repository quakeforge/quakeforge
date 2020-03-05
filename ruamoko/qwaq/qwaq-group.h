#ifndef __qwaq_group_h
#define __qwaq_group_h

#include "qwaq-view.h"

@class Array;

@interface Group : View
{
	Array      *views;
	int         focused;
}
-insert: (View *) view;
-remove: (View *) view;
@end

#endif//__qwaq_group_h
