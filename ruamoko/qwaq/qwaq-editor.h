#ifndef __qwaq_editor_h
#define __qwaq_editor_h

#include "qwaq-editbuffer.h"
#include "qwaq-view.h"

@class Editor;
@class EditBuffer;
@class ListenerGroup;

@interface Editor : View
{
	EditBuffer *buffer;
	DrawBuffer *linebuffer;
	eb_sel_t    selection;
	unsigned    base_index;		// top left corner
	unsigned    line_index;		// current line
	unsigned    char_index;		// current character
	unsigned    old_cind;		// previous character
	Point       scroll;
	Point       cursor;
	unsigned    line_count;
	string      filename;
}
-initWithRect:(Rect) rect file:(string) filename;
-(string)filename;
-scrollUp:(unsigned) count;
-scrollDown:(unsigned) count;
-scrollLeft:(unsigned) count;
-scrollRight:(unsigned) count;

-recenter:(int) force;
-gotoLine:(unsigned) line;
-highlightLine;
-(string)getWordAt:(Point) pos;	// view relative coordinates
@end

#endif//__qwaq_editor_h
