#ifndef __qwaq_ui_scrollbar_h
#define __qwaq_ui_scrollbar_h

#include "ruamoko/qwaq/ui/view.h"

@class Button;
@class DrawBuffer;
@class Group;
@class ListenerGroup;

@interface ScrollBar : View
{
	bool        vertical;
	int         bgchar;
	double      mouseTime;
	Point       mouseStart;
	Point       tabStart;
	DrawBuffer *buffer;
	Button     *backButton;
	Button     *forwardButton;
	Button     *thumbTab;
	Group      *objects;
	ListenerGroup *onScrollBarModified;

	unsigned    pageStep;
	unsigned    singleStep;
	unsigned    range;
	unsigned    index;
}
+(ScrollBar *)horizontal:(unsigned)len at:(Point)pos;
+(ScrollBar *)vertical:(unsigned)len at:(Point)pos;
-(ListenerGroup *)onScrollBarModified;
-setRange:(unsigned)range;
-setPageStep:(unsigned)pageStep;
-setSingleStep:(unsigned)singleStep;
-setIndex:(unsigned)index;
-(unsigned)index;
-page:(unsigned)step dir:(unsigned) dir;
@end

#endif//__qwaq_ui_scrollbar_h
