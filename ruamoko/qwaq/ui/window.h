#ifndef __qwaq_ui_window_h
#define __qwaq_ui_window_h

#include "Object.h"

@class Group;
@class Button;
@class TitleBar;

#include "ui/draw.h"
#include "ui/rect.h"
#include "ui/view.h"

@interface Window: View
{
	struct panel_s *panel;
	Group      *objects;
	Point       point;	// FIXME can't be local :(
	DrawBuffer *buf;

	Button     *topDrag;		// move-only
	Button     *topLeftDrag;
	Button     *topRightDrag;
	Button     *leftDrag;
	Button     *rightDrag;
	Button     *bottomLeftDrag;
	Button     *bottomRightDrag;
	Button     *bottomDrag;
	TitleBar   *titleBar;
}
+windowWithRect: (Rect) rect;
-setTitle:(string) title;
-setBackground: (int) ch;
-insert: (View *) view;
-insertDrawn: (View *) view;
-insertSelected: (View *) view;
@end

#endif//__qwaq_ui_window_h
