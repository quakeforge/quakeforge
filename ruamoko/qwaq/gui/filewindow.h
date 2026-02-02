#ifndef __qwaq_gui_filewindow_h
#define __qwaq_gui_filewindow_h

#include <dirent.h>

#include "window.h"
#include "listview.h"

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

@protocol FileWindow
-(void)openFile:(string) path forSave:(bool)forSave;
@end

@interface FileWindow : Window <ListView>
{
	string fileSpec;
	string filePath;
	bool forSave;
	FileItem *accepted_item;
	id<FileWindow> target;

	Array *items;
	ListView *listView;
}
+(FileWindow *) openFile:(string)fileSpec at:(string)filePath
					 ctx:(imui_ctx_t)ctx;
-setTarget:(id<FileWindow>)target;
-draw;
@end

#endif//__qwaq_gui_filewindow_h
