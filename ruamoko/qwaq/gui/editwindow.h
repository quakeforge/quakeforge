#ifndef __qwaq_gui_editwindow_h
#define __qwaq_gui_editwindow_h

#include <Object.h>
#include <dirent.h>

#include "editview.h"

typedef @handle imui_ctx_h imui_ctx_t;
typedef struct imui_window_s imui_window_t;

@interface EditWindow : Object
{
	EditView *editView;

	string window_name;
	imui_ctx_t IMUI_context;
	struct imui_window_s *window;
}
+(EditWindow *) openFile:(string)filePath ctx:(imui_ctx_t)ctx;
-draw;
@end

#endif//__qwaq_gui_editwindow_h
