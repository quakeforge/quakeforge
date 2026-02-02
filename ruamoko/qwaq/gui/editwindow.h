#ifndef __qwaq_gui_editwindow_h
#define __qwaq_gui_editwindow_h

#include "window.h"

@class EditView;

@interface EditWindow : Window
{
	EditView *editView;

	string window_name;
}
+(EditWindow *) openFile:(string)filePath ctx:(imui_ctx_t)ctx;
-draw;
@end

#endif//__qwaq_gui_editwindow_h
