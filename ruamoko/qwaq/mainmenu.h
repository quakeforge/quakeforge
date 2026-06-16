#ifndef __mainmenu_h
#define __mainmenu_h

#include "gui/filewindow.h"

@interface MainMenu : UI_Object<FileWindow>
{
	imui_window_t *main_menu;
	imui_window_t *file_menu;
	imui_window_t *window_menu;
	FileWindow *file_window;
}
+(imui_window_t *)create_menu:(string)name;
+(MainMenu *) menu:(imui_ctx_t)ctx;
-draw;
@end

extern bool quit_editor;

#endif//__mainmenu_h
