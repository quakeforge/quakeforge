#ifndef __qwaq_gui_filewindow_h
#define __qwaq_gui_filewindow_h

#include <Object.h>
#include <dirent.h>

typedef @handle imui_ctx_h imui_ctx_t;
typedef struct imui_window_s imui_window_t;

@interface FileItem : Object
{
	string name;
	bool   isdir;
	imui_ctx_t IMUI_context;
	ivec2  item_size;
}
+(FileItem *) fromDirent:(dirent_t)dirent ctx:(imui_ctx_t)ctx;
-draw;
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
}
+(FileWindow *) openFile:(string)fileSpec at:(string)filePath
					 ctx:(imui_ctx_t)ctx;
-draw;
@end

#endif//__qwaq_gui_filewindow_h
