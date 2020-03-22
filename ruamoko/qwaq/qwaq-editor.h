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
	unsigned    x_index;		// horizontal scrolling
	unsigned    char_index;		// current character
	unsigned    old_cind;		// previous character
	Point       cursor;
	unsigned    line_count;
}
-initWithRect:(Rect) rect file:(string) filename;
-scrollUp:(unsigned) count;
-scrollDown:(unsigned) count;
-scrollLeft:(unsigned) count;
-scrollRight:(unsigned) count;
@end

#endif//__qwaq_editor_h
