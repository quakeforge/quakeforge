#include <Array.h>

#include <stdlib.h>
#include <dirent.h>
#include <imui.h>
#include <string.h>
#include "filewindow.h"

void printf(string, ...);

@implementation FileItem

-initWithDirent:(dirent_t)dirent owner:(FileWindow *)owner ctx:(imui_ctx_t)ctx
{
	if (!(self = [super init])) {
		return nil;
	}
	name = str_hold (dirent.name);
	ext = name != ".." ? strrchr (name, '.') : -1;
	name_len = strlen (name);
	if (ext == 0) {
		// dot-file with no extension
		ext = -1;
	}
	isdir = dirent.type == 1;
	self.owner = owner;
	isselected = false;
	IMUI_context = ctx;
	return self;
}

+(FileItem *) fromDirent:(dirent_t)dirent owner:(FileWindow *)owner ctx:(imui_ctx_t)ctx
{
	return [[[self alloc] initWithDirent:dirent owner:owner ctx:ctx] autorelease];
}

-(void)dealloc
{
	str_free (name);
	[super dealloc];
}

-draw
{
	UI_Labelf ("%s%s", name, name != ".." && isdir ? "/" : "");
	int mode = IMUI_UpdateHotActive (IMUI_context);
	int but = IMUI_CheckButtonState (IMUI_context);
	if (isselected) {
		UI_SetFill (1);
	}
	item_size = IMUI_State_GetLen (IMUI_context, nil);

	if (but) {
		[owner itemClicked:self];
	}
	return self;
}

-select:(bool) select
{
	isselected = select;
	return self;
}

-(int) height
{
	return item_size.y;
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
	int card_len = strlen (wildcard);
	int name_pos = 0;
	int card_pos = 0;
	bool match = true;
	bool match_any = false;
	while (name_pos < name_len) {
		if (!match_any) {
			while (card_pos < card_len
				   && str_char (wildcard, card_pos) == '*') {
				card_pos++;
				match_any = true;
			}
		}
		if (card_pos >= card_len) {
			break;
		}
		if (match_any) {
			if (str_char (name, name_pos) == str_char (wildcard, card_pos)) {
				match_any = false;
				card_pos++;
			}
			name_pos++;
		} else {
			if (str_char (name, name_pos) != str_char (wildcard, card_pos)) {
				match = false;
				break;
			}
			name_pos++;
			card_pos++;
		}
	}
	if (card_pos < card_len) {
		match = false;
	}
	return match;
}

-selected:(bool)selected
{
	isselected = selected;
	return self;
}

-(string)name
{
	return name;
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
					FileItem *item = [FileItem fromDirent:dirent owner:self
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
	selected_item = -1;

	items = [[Array array] retain];

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
			UI_ScrollBox("FileWindow:scroller") {
				auto sblen = IMUI_State_GetLen (IMUI_context, nil);
				UI_Scroller () {
					ivec2 pos = IMUI_State_GetPos (IMUI_context, nil);
					ivec2 len = IMUI_State_GetLen (IMUI_context, nil);
					int height = [[items objectAtIndex:0] height];
					len.y = [items count] * height;
					IMUI_State_SetLen (IMUI_context, nil, len);
					if (!height) {
						height = 1;
					}
					IMUI_SetViewPos (IMUI_context, { 0, -pos.y % height });
					len = sblen;
					len.y = (len.y + height - 1) / height + 1;
					for (uint i = pos.y / height;
						 len.y-- > 0 && i < [items count]; i++) {
						[[items objectAtIndex:i] draw];
					}
				}
			}
			UI_ScrollBar ("FileWindow:scroller");
		}
	}
	return self;
}

-itemClicked:(FileItem *)item
{
	if (selected_item >= 0 && [items objectAtIndex:selected_item] == item) {
		printf ("item accepted:%s\n", [item name]);
	} else {
		if (selected_item >= 0) {
			[[items objectAtIndex:selected_item] select:false];
		}
		[item select:true];
		selected_item = [items indexOfObject:item];
		printf ("item selected:%d:%s\n", selected_item, [item name]);
	}
	return self;
}

@end
