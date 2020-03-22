#ifndef __qwaq_editor_h
#define __qwaq_editor_h

#include "qwaq-editbuffer.h"
#include "qwaq-view.h"

@class EditBuffer;

@interface Editor : View
{
	EditBuffer *buffer;
	DrawBuffer *linebuffer;
	eb_sel_t    selection;
	unsigned    base_index;		// top left corner
	unsigned    line_index;		// current line
	unsigned    char_index;		// current character
	unsigned    old_cind;		// previous character
	Point       cursor;
	unsigned    line_count;
}
-initWithRect:(Rect) rect file:(string) filename;
@end

#endif//__qwaq_editor_h
