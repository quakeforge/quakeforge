#ifndef __qwaq_window_h
#define __qwaq_window_h

#include "Object.h"

@class Array;

#include "qwaq-draw.h"
#include "qwaq-rect.h"
#include "qwaq-view.h"
#include "qwaq-group.h"

@interface Window: Group
{
	Point       point;	// FIXME can't be local :(
	Array      *views;
	View       *focusedView;
	struct window_s *window;
	struct panel_s *panel;
}
+windowWithRect: (Rect) rect;
-addView: (View *) view;
-setBackground: (int) ch;
@end

#endif//__qwaq_window_h
