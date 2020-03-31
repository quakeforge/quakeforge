#ifndef __qwaq_ui_tableview_h
#define __qwaq_ui_tableview_h

#include "ui/view.h"

@class DrawBuffer;
@class TableView;
@class TableViewColumn;
@class Array;

@protocol TableViewDataSource
-(int)numberOfRows:(TableView *)tableview;
-(View *)tableView:(TableView *)tableView
		 forColumn:(TableViewColumn *)column
		       row:(int)row;
-retain;
-release;
@end

@interface TableViewColumn : Object
{
	string      name;
	int         width;
}
+(TableViewColumn *)named:(string)name;
+(TableViewColumn *)named:(string)name width:(int)width;
-(string)name;
-(int)width;
@end

@interface TableView : View
{
	Array      *columns;
	DrawBuffer *buffer;
	int         columns_dirty;
	id<TableViewDataSource> dataSource;
	Point       base;
}
+(TableView *)withRect:(Rect)rect;
-addColumn:(TableViewColumn *)column;
-setDataSource:(id<TableViewDataSource>)dataSource;
@end

#endif//__qwaq_ui_tableview_h
