#ifndef __qwaq_editor_status_h
#define __qwaq_editor_status_h

#include "ruamoko/qwaq/ui/view.h"

typedef enum {
	cmInsert,
	cmTypeOver,
	cmVInsert,
	cmVTypeOver,
} CursorMode;

@interface EditStatus : View
{
	CursorMode  cursorMode;
	int         modified;
}
+(EditStatus *)withRect:(Rect)rect;
-setCursorMode:(CursorMode)mode;
-setModified:(int)modified;
@end

#endif//__qwaq_editor_status_h
