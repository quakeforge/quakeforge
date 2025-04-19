#include <Array.h>

#include <stdlib.h>
#include <dirent.h>
#include <imui.h>
#include <string.h>
#include "filewindow.h"

void printf(string, ...);

@implementation FileItem

-initWithDirent:(dirent_t)dirent ctx:(imui_ctx_t)ctx
{
	if (!(self = [super initWithName:dirent.name ctx:ctx])) {
		return nil;
	}
	ext = name != ".." ? strrchr (name, '.') : -1;
	name_len = strlen (name);
	if (ext == 0) {
		// dot-file with no extension
		ext = -1;
	}
	isdir = dirent.type == 1;
	return self;
}

+(FileItem *) fromDirent:(dirent_t)dirent ctx:(imui_ctx_t)ctx
{
	return [[[self alloc] initWithDirent:dirent ctx:ctx] autorelease];
}

-draw
{
	UI_Labelf ("%s%s", name, name != ".." && isdir ? "/" : "");
	item_size = IMUI_State_GetLen (IMUI_context, nil);
	return [super checkInput];
}

-(int) cmp:(FileItem *) other
{
	return name != other.name;
}

-(bool) isdir
{
	return isdir;
}

-(bool) hidden
{
	return name != ".." && str_char (name, 0) == '.';
}

-(bool) match:(string)wildcard
{
	return fnmatch (wildcard, name, FNM_PATHNAME);
}

@end

@implementation Array (FileItem)

int file_item_cmp (void *a, void *b)
{
	return [*(FileItem **) a cmp: *(FileItem **) b];
}

-sort_file_items
{
	qsort (_objs, count, sizeof (FileItem *), file_item_cmp);
	return self;
}
@end

@implementation FileWindow

-readdir
{
	if (isdir (filePath)) {
		[items removeAllObjects];
		auto dir = opendir (filePath);
		if (dir) {
			auto dirent = readdir (dir);
			while (dirent.name) {
				if (dirent.type < 2 && dirent.name != ".") {
					FileItem *item = [FileItem fromDirent:dirent
													  ctx:IMUI_context];
					if (![item hidden]
						&& ([item isdir] || [item match:fileSpec])) {
						[items addObject: item];
					}
				}
				dirent = readdir (dir);
			}
			closedir (dir);
			[items sort_file_items];
		}
	}
	[listView setItems:items];
	return self;
}

-initWithSpec:(string)fileSpec at:(string)filePath forSave:(bool)forSave
		  ctx:(imui_ctx_t)ctx
{
	if (!(self = [super init])) {
		return nil;
	}
	self.fileSpec = str_hold (fileSpec);
	self.filePath = str_hold (filePath);
	self.forSave = forSave;
	IMUI_context = ctx;

	items = [[Array array] retain];
	listView = [[ListView list:"FileWindow:files" ctx:ctx] retain];

	[self readdir];

	window = IMUI_NewWindow (forSave ? "Save a File##FileWindow"
									 : "Open a File##FileWindow");
	IMUI_Window_SetSize (window, {400, 300});
	return self;
}

+(FileWindow *) openFile:(string)fileSpec at:(string)filePath
					 ctx:(imui_ctx_t)ctx
{
	return [[[self alloc] initWithSpec:fileSpec at:filePath
							   forSave:false ctx:ctx]
			autorelease];
}

-(void)dealloc
{
	str_free (fileSpec);
	str_free (filePath);
	[items release];
	IMUI_DeleteWindow (window);
	[super dealloc];
}

-draw
{
	imui_style_t style = {};//FIXME qfcc bug
	IMUI_Style_Fetch (IMUI_context, &style);
	UI_Window (window) {
		if (IMUI_Window_IsCollapsed (window)) {
			continue;
		}
		UI_Horizontal {
			IMUI_Layout_SetYSize (IMUI_context, imui_size_expand, 100);
			UI_SetFill (style.background.normal);
			[listView draw];
		}
	}
	return self;
}

@end
