#ifndef __qwaq_gui_filewindow_h
#define __qwaq_gui_filewindow_h

#include <Object.h>
#include <dirent.h>

typedef @handle imui_ctx_h imui_ctx_t;
typedef struct imui_window_s imui_window_t;

@class FileWindow;

@interface FileItem : Object
{
	string name;
	bool   isdir;
	int    ext;
	int    name_len;
	imui_ctx_t IMUI_context;
	ivec2  item_size;
	FileWindow *owner;

	bool   isselected;
}
+(FileItem *) fromDirent:(dirent_t)dirent owner:(FileWindow *) owner ctx:(imui_ctx_t)ctx;
-draw;
-selected:(bool)selected;
-(string)name;
-(bool)isdir;
-(bool)hidden;
-(bool)match:(string)wildcard;
@end

@class Array;

@interface Array (FileItem)
-sort_file_items;
@end

@interface FileWindow : Object
{
	string fileSpec;
	string filePath;
	bool forSave;

	Array *items;

	imui_ctx_t IMUI_context;
	struct imui_window_s *window;

	int    selected_item;
}
+(FileWindow *) openFile:(string)fileSpec at:(string)filePath
					 ctx:(imui_ctx_t)ctx;
-draw;

-itemClicked:(FileItem *) item;
@end

#endif//__qwaq_gui_filewindow_h
