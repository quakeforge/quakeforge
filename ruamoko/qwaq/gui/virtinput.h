#ifndef __qwaq_gui_virtinput_h
#define __qwaq_gui_virtinput_h

#include <dirent.h>
#include <input.h>

#include "listview.h"

@interface VirtualInput : ListItem
{
	string description;
}
+(VirtualInput *) button:(string)name ctx:(imui_ctx_t)ctx;
+(VirtualInput *) axis:(string)name ctx:(imui_ctx_t)ctx;
-draw;
@end

#endif//__qwaq_gui_virtinput_h
