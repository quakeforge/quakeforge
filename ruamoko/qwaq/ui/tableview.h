#ifndef __qwaq_ui_tableview_h
#define __qwaq_ui_tableview_h

#include "ruamoko/qwaq/ui/table.h"
#include "ruamoko/qwaq/ui/view.h"

@class DrawBuffer;
@class Array;
@class ListenerGroup;

@interface TableView : View
{
	Array      *columns;
	DrawBuffer *buffer;
	int         columns_dirty;
	id<TableDataSource> dataSource;
	Point       base;
}
+(TableView *)withRect:(Rect)rect;
-addColumn:(TableColumn *)column;
-setDataSource:(id<TableDataSource>)dataSource;
@end

#endif//__qwaq_ui_tableview_h
