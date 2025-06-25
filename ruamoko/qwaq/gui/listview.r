#include <Array.h>

#include <stdlib.h>
#include <dirent.h>
#include <imui.h>
#include <string.h>
#include "listview.h"

void printf(string, ...);

@implementation ListItem

-initWithName:(string)name ctx:(imui_ctx_t)ctx
{
	if (!(self = [super init])) {
		return nil;
	}
	self.name = str_hold (name);
	self.owner = nil;
	isselected = false;
	IMUI_context = ctx;
	return self;
}

+(ListItem *) item:(string)name ctx:(imui_ctx_t)ctx
{
	return [[[self alloc] initWithName:name ctx:ctx] autorelease];
}

-(void)setOwner:(ListView *) owner
{
	self.owner = owner;
}

-(void)dealloc
{
	str_free (name);
	[super dealloc];
}

-draw
{
	UI_Label (name);
	return [self checkInput];
}

-checkInput
{
	int mode = IMUI_UpdateHotActive (IMUI_context);
	int but = IMUI_CheckButtonState (IMUI_context);
	if (isselected) {
		UI_SetFill (1);
	}

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

-(int) cmp:(ListItem *) other
{
	return name != other.name;
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

@implementation ListView

-initWithName:(string)name ctx:(imui_ctx_t)ctx
{
	if (!(self = [super init])) {
		return nil;
	}
	self.name = str_hold (name);
	selected_item = -1;
	IMUI_context = ctx;
	return self;
}

+(ListView *) list:(string)name ctx:(imui_ctx_t)ctx
{
	return [[[self alloc] initWithName:name ctx:ctx] autorelease];
}

-setItems:(Array *)items
{
	[items retain];
	[self.items release];
	self.items = items;
	selected_item = -1;

	[items makeObjectsPerformSelector:@selector(setOwner:) withObject:self];
	return self;
}

-setTarget:(id<ListView>) target
{
	self.target = target;
	return self;
}

-(void)dealloc
{
	str_free (name);
	[items release];
	[super dealloc];
}

-draw
{
	UI_ScrollBox(name + "##ListView:scroller") {
		auto sblen = IMUI_State_GetLen (IMUI_context, nil);
		UI_Scroller () {
			uint count = [items count];
			if (count) {
				ivec2 pos = IMUI_State_GetPos (IMUI_context, nil);
				ivec2 len = IMUI_State_GetLen (IMUI_context, nil);
				int height = IMUI_TextSize (IMUI_context, "X").y;
				len.y = count * height;
				IMUI_State_SetLen (IMUI_context, nil, len);
				IMUI_SetViewPos (IMUI_context, { 0, -pos.y % height });
				len = sblen;
				len.y = (len.y + height - 1) / height + 1;
				for (uint i = pos.y / height; len.y-- > 0 && i < count; i++) {
					[[items objectAtIndex:i] draw];
				}
			}
		}
	}
	UI_ScrollBar (name + "##ListView:scroller");
	return self;
}

-itemClicked:(ListItem *)item
{
	if (selected_item >= 0 && [items objectAtIndex:selected_item] == item) {
		[target itemAccepted:selected_item in:items];
	} else {
		if (selected_item >= 0) {
			[[items objectAtIndex:selected_item] select:false];
		}
		[item select:true];
		selected_item = [items indexOfObject:item];
		[target itemSelected:selected_item in:items];
	}
	return self;
}

-(int)selected
{
	return selected_item;
}

@end
