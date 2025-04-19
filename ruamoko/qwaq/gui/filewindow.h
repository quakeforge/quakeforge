#ifndef __qwaq_gui_filewindow_h
#define __qwaq_gui_filewindow_h

#include <Object.h>
#include <dirent.h>

#include "listview.h"

typedef @handle imui_ctx_h imui_ctx_t;
typedef struct imui_window_s imui_window_t;

@class FileWindow;

@interface FileItem : ListItem
{
	bool   isdir;
	int    ext;
	int    name_len;
}
+(FileItem *) fromDirent:(dirent_t)dirent ctx:(imui_ctx_t)ctx;
-draw;
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
	ListView *listView;

	imui_ctx_t IMUI_context;
	struct imui_window_s *window;
}
+(FileWindow *) openFile:(string)fileSpec at:(string)filePath
					 ctx:(imui_ctx_t)ctx;
-draw;
@end

#endif//__qwaq_gui_filewindow_h
