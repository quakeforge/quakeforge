#include <Array.h>

#include <stdlib.h>
#include <dirent.h>
#include <imui.h>
#include <string.h>
#include "editview.h"

void printf(string, ...);

@implementation EditView

-initWithName:(string)name file:(string)filepath ctx:(imui_ctx_t)ctx
{
	if (!(self = [super init])) {
		return nil;
	}
	self.name = str_hold (name);
	IMUI_context = ctx;

	buffer = [[EditBuffer withFile:filepath] retain];
	line_count = [buffer countLines: {0, [buffer textSize]}];
	return self;
}

+(EditView *) edit:(string)name file:(string)filepath ctx:(imui_ctx_t)ctx
{
	return [[[self alloc] initWithName:name file:filepath ctx:ctx] autorelease];
}

-(void)dealloc
{
	str_free (name);
	[buffer release];
	[super dealloc];
}

-draw
{
	UI_ScrollBox(name + "##EditView:scroller") {
		auto sblen = IMUI_State_GetLen (IMUI_context, nil);
		UI_Scroller () {
			uint count = line_count;
			uint lind = 0;
			if (count) {
				int buf[1024];
				ivec2 pos = IMUI_State_GetPos (IMUI_context, nil);
				ivec2 len = IMUI_State_GetLen (IMUI_context, nil);
				//FIXME assumes fixed-width
				ivec2 ts = IMUI_TextSize (IMUI_context, "X");
				len.y = count * ts.y;
				IMUI_State_SetLen (IMUI_context, nil, len);
				IMUI_SetViewPos (IMUI_context, { 0, -pos.y % ts.y });
				len = sblen;
				len.y = (len.y + ts.y - 1) / ts.y + 1;
				//FIXME utf-8
				int width = len.x / ts.x + 2;
				for (uint i = pos.y / ts.y; len.y-- > 0 && i < count; i++) {
					lind = [buffer formatLine:lind from:0 into:buf width:width
							highlight:{0,0} colors:{ 15, 0 }];
					IMUI_IntLabel (IMUI_context, buf, width);
				}
			}
		}
	}
	UI_ScrollBar (name + "##EditView:scroller");
	return self;
}

@end
