#ifndef __qwaq_gui_editview_h
#define __qwaq_gui_editview_h

#include <dirent.h>

#include "ui_object.h"
#include "../editor/editbuffer.h"

@interface EditView : UI_Object
{
	string name;

	uvec2       X_size;
	ivec2       scroll_pos;

	EditBuffer *buffer;
	uint        line_count;
	string      filename;
	string      filepath;

	uvec2       sel;
	bvec2       override_scroll;
	uvec2       size;
	uvec2       cursor;
	uvec2       base;			// top left corner (cell)
	uint        base_index;		// top left corner
	uint        line_index;		// current line
	uint        char_index;		// current character
	uint        old_char_index;
}
+(EditView *) edit:(string)name file:(string)filepath ctx:(imui_ctx_t)ctx;
-draw;
-(bool)modified;
@end

#endif//__qwaq_gui_editview_h
