#ifndef __qwaq_window_h
#define __qwaq_window_h

#include "Object.h"

@class Group;

#include "qwaq-draw.h"
#include "qwaq-rect.h"
#include "qwaq-view.h"

@interface Window: View
{
	struct panel_s *panel;
	Group      *objects;
	Point       point;	// FIXME can't be local :(
	DrawBuffer *buf;
}
+windowWithRect: (Rect) rect;
-setBackground: (int) ch;
-addView: (View *) view;
@end

#endif//__qwaq_window_h
