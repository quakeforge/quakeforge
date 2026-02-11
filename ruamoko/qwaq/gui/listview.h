#ifndef __qwaq_gui_listview_h
#define __qwaq_gui_listview_h

#include "ui_object.h"

@class Array;
@class ListView;

@interface ListItem : UI_Object
{
	string name;
	ListView *owner;

	bool   isselected;
}
+(ListItem *) item:(string)name ctx:(imui_ctx_t)ctx;
-initWithName:(string)name ctx:(imui_ctx_t)ctx;
-draw;
-checkInput;
-selected:(bool)selected;
-(string)name;
@end

@protocol ListView
-(void)itemSelected:(int)item in:(Array *)items;
-(void)itemAccepted:(int)item in:(Array *)items;
@end

@interface ListView : UI_Object
{
	Array *items;
	string name;

	int    selected_item;
	id<ListView> target;
}
+(ListView *) list:(string)name ctx:(imui_ctx_t)ctx;
-setItems:(Array *)items;
-setTarget:(id<ListView>) target;
-draw;
-itemClicked:(ListItem *) item;
-(int)selected;
@end

#endif//__qwaq_gui_listview_h
