#include <Array.h>

#include <dirent.h>
#include <imui.h>
#include <string.h>
#include "filewindow.h"

@implementation FileItem

-initWithDirent:(dirent_t)dirent ctx:(imui_ctx_t)ctx
{
	if (!(self = [super init])) {
		return nil;
	}
	self.name = str_hold (dirent.name);
	self.isdir = dirent.type == 1;
	IMUI_context = ctx;
	return self;
}

+(FileItem *) fromDirent:(dirent_t)dirent ctx:(imui_ctx_t)ctx
{
	return [[[self alloc] initWithDirent:dirent ctx:ctx] autorelease];
}

-(void)dealloc
{
	str_free (name);
	[super dealloc];
}

-draw
{
	UI_Labelf ("%s%s", name, name != ".." && isdir ? "/" : "");
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
					[items addObject: [FileItem fromDirent:dirent
													   ctx:IMUI_context]];
				}
				dirent = readdir (dir);
			}
			closedir (dir);
		}
	}
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

	[self readdir];

	window = IMUI_NewWindow (forSave ? "Save a File##FileWindow"
									 : "Open a File##FileWindow");
	IMUI_Window_SetSize (window, 400, 300);
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
			UI_ScrollBox("FileWindow:scroller") {
				[items makeObjectsPerformSelector: @selector (draw)];
			}
			UI_ScrollBar ("FileWindow:scroller");
		}
	}
	return self;
}

@end
