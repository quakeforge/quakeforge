#ifndef __qwaq_gui_listview_h
#define __qwaq_gui_listview_h

#include <Object.h>
#include <dirent.h>

typedef @handle imui_ctx_h imui_ctx_t;
typedef struct imui_window_s imui_window_t;

@class ListView;

@interface ListItem : Object
{
	string name;
	imui_ctx_t IMUI_context;
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

@interface ListView : Object
{
	Array *items;
	string name;

	imui_ctx_t IMUI_context;
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
