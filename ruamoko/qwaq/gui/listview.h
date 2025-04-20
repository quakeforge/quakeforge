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
	ivec2  item_size;
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

@interface ListView : Object
{
	Array *items;
	string name;

	imui_ctx_t IMUI_context;
	int    selected_item;
}
+(ListView *) list:(string)name ctx:(imui_ctx_t)ctx;
-setItems:(Array *)items;
-draw;
-itemClicked:(ListItem *) item;
-(int)selected;
@end

#endif//__qwaq_gui_listview_h
