#ifndef __qwaq_gui_editview_h
#define __qwaq_gui_editview_h

#include <Object.h>
#include <dirent.h>

#include "../editor/editbuffer.h"

typedef @handle imui_ctx_h imui_ctx_t;
typedef struct imui_window_s imui_window_t;

@interface EditView : Object
{
	string name;
	imui_ctx_t IMUI_context;

	ivec2       X_size;
	ivec2       scroll_pos;

	EditBuffer *buffer;
	uint        line_count;
	string      filename;
	string      filepath;

	bvec2       override_scroll;
	uvec2       size;
	uvec2       cursor;
	uvec2       base;			// top left corner (cell)
	uint        base_index;		// top left corner
	uint        line_index;		// current line
	uint        char_index;		// current character
}
+(EditView *) edit:(string)name file:(string)filepath ctx:(imui_ctx_t)ctx;
-draw;
-(bool)modified;
@end

#endif//__qwaq_gui_editview_h
