#ifndef __qwaq_ui_table_h
#define __qwaq_ui_table_h

#include <Object.h>

@class TableColumn;
@class ListenerGroup;

@protocol TableCell
-(string) format:(int)width;
@end

@protocol TableDataSource <Object>
-(ListenerGroup *)onRowCountChanged;
-(int)numberOfRows;
-(id<TableCell>)cellForColumn:(TableColumn *)column row:(int)row;
@end

@interface TableColumn : Object
{
	string      name;
	int         width;
	bool        growWidth;
}
+(TableColumn *)named:(string)name;
+(TableColumn *)named:(string)name width:(int)width;

-setGrowMode: (bool) mode;
-(bool)growMode;
-grow:(ivec2)delta;

-(string)name;
-(int)width;
@end

@protocol Table
-addColumn:(TableColumn *)column;
-setDataSource:(id<TableDataSource>)dataSource;
@end


#endif//__qwaq_ui_table_h
