#ifndef __qwaq_ui_scrollbar_h
#define __qwaq_ui_scrollbar_h

#include "ui/view.h"

@class Button;
@class Group;
@class ListenerGroup;

@interface ScrollBar : View
{
	int         vertical;
	int         bgchar;
	Point       mouseStart;
	Button     *backButton;
	Button     *forwardButton;
	Button     *thumbTab;
	Group      *objects;
	ListenerGroup *onScroll;

	unsigned    pageStep;
	unsigned    singleStep;
	unsigned    range;
	unsigned    index;
}
+(ScrollBar *)horizontal:(unsigned)len at:(Point)pos;
+(ScrollBar *)vertical:(unsigned)len at:(Point)pos;
-(ListenerGroup *)onScroll;
-setRange:(unsigned)range;
-setPageStep:(unsigned)pageStep;
-setSingleStep:(unsigned)singleStep;
-setIndex:(unsigned)index;
@end

#endif//__qwaq_ui_scrollbar_h
