#ifndef __qwaq_editor_editor_h
#define __qwaq_editor_editor_h

#include "ruamoko/qwaq/editor/editbuffer.h"
#include "ruamoko/qwaq/editor/status.h"
#include "ruamoko/qwaq/ui/view.h"

@class Editor;
@class EditBuffer;
@class ListenerGroup;
@class EditStatus;

@interface Editor : View
{
	EditBuffer *buffer;
	EditStatus *status;
	DrawBuffer *linebuffer;
	eb_sel_t    selection;
	unsigned    base_index;		// top left corner
	unsigned    line_index;		// current line
	unsigned    char_index;		// current character
	unsigned    old_cind;		// previous character
	Point       base;
	Point       cursor;
	unsigned    line_count;
	string      filename;
	string      filepath;
	int         modified;
	CursorMode  cursorMode;

	int         virtualInsert;
	int         cursorThroughTabs;
}
+(Editor *)withRect:(Rect)rect file:(string)filename;
+(Editor *)withRect:(Rect)rect file:(string)filename path:(string)filepath;
-(string)filename;
-(string)filepath;
-(Point)cursor;
-setStatusView:(EditStatus *)status;
-scrollUp:(unsigned) count;
-scrollDown:(unsigned) count;
-scrollLeft:(unsigned) count;
-scrollRight:(unsigned) count;
-pageUp;
-pageDown;
-charUp;
-charDown;
-charLeft;
-charRight;

-recenter:(int) force;
-gotoLine:(unsigned) line;
-highlightLine;
-(string)getWordAt:(Point) pos;	// view relative coordinates
@end

#endif//__qwaq_editor_editor_h
