#include <QF/keys.h>
#include <QF/input/event.h>
#include <Array.h>

#include <stdlib.h>
#include <dirent.h>
#include <imui.h>
#include <string.h>
#include "editview.h"

void printf(string, ...);

@implementation EditView

static uint
center (uint v, uint len)
{
	return v > (len / 2) ? v - len / 2 : 0;
}

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

-scrollTo:(uint)target
{
	if (target > base.y) {
		base_index = [buffer nextLine:base_index :target - base.y];
	} else if (target < base.y) {
		base_index = [buffer prevLine:base_index :base.y - target];
	}
	base.y = target;
	return self;
}

-recenter:(bool)force
{
	if (force || cursor.y < base.y || cursor.y >= base.y + size.y) {
		[self scrollTo: center (cursor.y, size.y)];
	}
	if (force || cursor.x < base.x || cursor.x >= base.x + size.x) {
		base.x = center (cursor.x, size.x);
	}
	override_scroll = { true, true };
	return self;
}

-moveBOT
{
	line_index = base_index = char_index = 0;
	cursor = nil;
	base = nil;
	override_scroll = { true, true };
	return self;
}

-moveEOT
{
	char_index = [buffer getEOT];
	line_index = [buffer getBOL:char_index];
	cursor.x = [buffer charPos:line_index at:char_index];
	cursor.y = line_count;
	if (cursor.y >= size.y) {
		[self scrollTo: cursor.y - size.y + 1];
	}
	override_scroll = { true, true };
	return self;
}

-pageUp
{
	[self recenter:false];
	uint count = cursor.y;
	if (count > size.y) {
		count = size.y;
	}
	if (count) {
		cursor.y -= count;
		line_index = [buffer prevLine:line_index :count];
		char_index = [buffer charPtr:line_index at:cursor.x];
	}
	if (base.y > cursor.y) {
		[self scrollTo:cursor.y];
		override_scroll.y = true;
	}
	return self;
}

-pageDown
{
	[self recenter:false];
	uint count = line_count - cursor.y;
	if (count > size.y) {
		count = size.y;
	}
	if (count) {
		cursor.y += count;
		line_index = [buffer nextLine:line_index :count];
		char_index = [buffer charPtr:line_index at:cursor.x];
	}
	if (base.y + size.y - 1 < cursor.y) {
		[self scrollTo:cursor.y - size.y + 1];
		override_scroll.y = true;
	}
	return self;
}

-linesUp
{
	while (line_index > 0) {
		line_index = [buffer prevLine:line_index];
		cursor.y--;
		uint p = [buffer nextNonSpace:line_index];
		if ([buffer getChar:p] == '\n') {
			continue;
		}
		uint x = [buffer charPos:line_index at:p];
		if (x <= cursor.x) {
			break;
		}
	}
	char_index = [buffer charPtr:line_index at:cursor.x];
	[self recenter:false];
	return self;
}

-linesDown
{
	uint end = [buffer getEOT];
	uint last = [buffer getBOL:end];
	while (line_index < last) {
		line_index = [buffer nextLine:line_index];
		cursor.y++;
		uint p = [buffer nextNonSpace:line_index];
		if (p >= end || [buffer getChar:p] == '\n') {
			continue;
		}
		uint x = [buffer charPos:line_index at:p];
		if (x <= cursor.x) {
			break;
		}
	}
	char_index = [buffer charPtr:line_index at:cursor.x];
	[self recenter:false];
	return self;
}

-charUp
{
	[self recenter:false];
	if (cursor.y < 1) {
		return self;
	}
	cursor.y--;
	line_index = [buffer prevLine:line_index];
	char_index = [buffer charPtr:line_index at:cursor.x];
	if (base.y > cursor.y) {
		[self scrollTo:cursor.y];
		override_scroll.y = true;
	}
	return self;
}

-charDown
{
	[self recenter:false];
	if (cursor.y >= line_count) {
		return self;
	}
	cursor.y++;
	line_index = [buffer nextLine:line_index];
	char_index = [buffer charPtr:line_index at:cursor.x];
	if (base.y + size.y - 1 < cursor.y) {
		[self scrollTo:cursor.y - size.y + 1];
		override_scroll.y = true;
	}
	return self;
}

-wordLeft
{
	[self recenter:false];
	auto b = base;
	do {
		if (char_index && char_index == line_index) {
			line_index = [buffer prevLine:line_index];
			char_index = [buffer getEOL:line_index];
			if (--cursor.y < b.y) {
				b.y = cursor.y;
			}
		}
		char_index = [buffer prevWord:char_index];
	} while (char_index && char_index < [buffer getEOT]
			 && ![buffer isWord:char_index]);

	cursor.x = [buffer charPos:line_index at:char_index];
	if (cursor.x < b.x) {
		b.x = cursor.x;
	} else if (cursor.x >= b.x + size.x) {
		b.x = center (cursor.x, size.x);
	}
	[self scrollTo:b.y];
	base = b;
	override_scroll = { true, true };

	return self;
}

-wordRight
{
	[self recenter:0];
	auto b = base;

	if (char_index >= [buffer getEOT]) {
		return self;
	}
	if ([buffer getChar:char_index] == '\n') {
		while ([buffer getChar:char_index] == '\n') {
			char_index = line_index = [buffer nextLine:line_index];
			if (++cursor.y >= b.y + size.y) {
				b.y = cursor.y + 1 - size.y;
			}
			if (char_index >= [buffer getEOT]) {
				break;
			}
			if (![buffer isWord:char_index]) {
				char_index = [buffer nextWord: char_index];
			}
		}
	} else {
		char_index = [buffer nextWord: char_index];
	}

	cursor.x = [buffer charPos:line_index at:char_index];
	if (cursor.x < b.x) {
		b.x = cursor.x;
	} else if (cursor.x >= b.x + size.x) {
		b.x = center (cursor.x, size.x);
	}
	[self scrollTo:b.y];
	base = b;
	override_scroll = { true, true };

	return self;
}

-charLeft
{
	[self recenter:false];
	if (cursor.x < 1) {
		return self;
	}
	if (true) {
		char_index = [buffer prevChar:char_index];
		cursor.x = [buffer charPos:line_index at:char_index];
	} else {
		cursor.x--;
		char_index = [buffer charPtr:line_index at:cursor.x];
	}

	if (base.x > cursor.x) {
		base.x = cursor.x;
		override_scroll.x = true;
	}
	return self;
}

-charRight
{
	[self recenter:false];
	if (char_index == [buffer getEOL:line_index]) {
		return self;
	}
	if (true) {
		char_index = [buffer nextChar:char_index];
		cursor.x = [buffer charPos:line_index at:char_index];
	} else {
		cursor.x++;
		char_index = [buffer charPtr:line_index at:cursor.x];
	}
	if (base.x + size.x - 1 < cursor.x) {
		base.x = cursor.x - size.x + 1;
		override_scroll.x = true;
	}
	return self;
}

-moveBOS
{
	line_index = base_index;
	cursor.y = base.y;
	char_index = [buffer charPtr:line_index at:cursor.x];
	return self;
}

-moveEOS
{
	uint count = line_count - base.y;
	if (count > size.y - 1) {
		count = size.y - 1;
	}
	line_index = [buffer nextLine:base_index :count];
	cursor.y = base.y + count;
	char_index = [buffer charPtr:line_index at:cursor.x];
	return self;
}

-moveBOL
{
	char_index = line_index;
	cursor.x = 0;
	base.x = 0;
	override_scroll.x = true;
	return self;
}

-moveEOL
{
	char_index = [buffer getEOL:line_index];
	cursor.x = [buffer charPos:line_index at:char_index];
	[self recenter:0];
	return self;
}

-typeChar:(int)chr
{
	[self recenter:false];
	if (false) {//typeover
	} else {
		[buffer insertChar:chr at:char_index];
		char_index++;
		if (chr == '\n') {
			cursor.y++;
			line_count++;
			line_index = [buffer nextLine:line_index];
		}
		cursor.x = [buffer charPos:line_index at:char_index];
	}
	return self;
}

-deleteChar
{
	if (char_index >= [buffer getEOT]) {
		return self;
	}
	uint lines = [buffer countLines:{char_index, 1}];
	line_count -= lines;
	override_scroll.y = lines;
	[buffer deleteText:{char_index, 1}];
	return self;
}

-backspace
{
	bool nodelete = false;
	uint len = 1;
	if (cursor.x > 0) {
		uint lind = line_index;
		uint ptr = [buffer nextNonSpace:lind];
		if (ptr == char_index) {
			uint x = cursor.x;
			if (lind > 0) {
				do {
					lind = [buffer prevLine:lind];
					ptr = [buffer nextNonSpace:lind];
					x = [buffer charPos:lind at:ptr];
				} while (lind > 0 && (ptr == [buffer getEOL:ptr]
									  || cursor.x < x));
			}
			len = [buffer charPtr:line_index at:cursor.x];
			if (cursor.x <= x) {
				x = 0;
			}
			cursor.x = x;
		} else {
			cursor.x--;
			len = char_index;
		}
		char_index = [buffer charPtr:line_index at:cursor.x];
		len -= char_index;
		// if char_index points at a new-line, then the cursor is beyond the
		// end of the line thus there is nothing to delete
		if (char_index >= [buffer getEOT]
			|| [buffer getChar:char_index] == '\n') {
			nodelete = true;
		}
	} else if (cursor.y > 0) {
		cursor.y--;
		line_index = [buffer prevLine:line_index];
		char_index = [buffer getEOL:line_index];
	} else {
		return self;
	}
	cursor.x = [buffer charPos:line_index at:char_index];
	if (!nodelete) {
		uint lines = [buffer countLines:{char_index, len}];
		line_count -= lines;
		override_scroll.y = lines;
		[buffer deleteText:{char_index, len}];
	}
	return self;
}

-handleKey:(imui_key_t)key
{
	switch (key.code) {
	case QFK_PAGEUP:
		if (key.shift & ies_control) {
			[self moveBOT];
		} else {
			[self pageUp];
		};
		break;
	case QFK_PAGEDOWN:
		if (key.shift & ies_control) {
			[self moveEOT];
		} else {
			[self pageDown];
		};
		break;
	case QFK_UP:
		if (key.shift & ies_control) {
			[self linesUp];
		} else {
			[self charUp];
		};
		break;
	case QFK_DOWN:
		if (key.shift & ies_control) {
			[self linesDown];
		} else {
			[self charDown];
		};
		break;
	case QFK_LEFT:
		if (key.shift & ies_control) {
			[self wordLeft];
		} else {
			[self charLeft];
		};
		break;
	case QFK_RIGHT:
		if (key.shift & ies_control) {
			[self wordRight];
		} else {
			[self charRight];
		};
		break;
	case QFK_HOME:
		if (key.shift & ies_control) {
			[self moveBOS];
		} else {
			[self moveBOL];
		}
		break;
	case QFK_END:
		if (key.shift & ies_control) {
			[self moveEOS];
		} else {
			[self moveEOL];
		}
		break;
	case QFK_BACKSPACE:
		[self backspace];
		break;
	case QFK_DELETE:
		[self deleteChar];
		break;
	case QFK_RETURN:
		[self typeChar:'\n'];
		break;
	case QFK_TAB:
		[self typeChar:'\t'];
		break;
	default:
		if (!(key.shift & (ies_control | ies_alt))
			&& key.unicode && key.unicode >= ' ') {
			[self typeChar:key.unicode];
		}
		break;
	}
	return self;
}

-draw_text:(ivec2)sblen
{
	//FIXME assumes fixed-width
	ivec2 ts = IMUI_TextSize (IMUI_context, "X");

	uint count = line_count + 1;
	ivec2 pos = IMUI_State_GetPos (IMUI_context, nil);
	if (override_scroll.x) {
		pos.x = base.x * ts.x;
	}
	if (override_scroll.y) {
		pos.y = base.y * ts.y;
	}
	IMUI_State_SetPos (IMUI_context, nil, pos);
	override_scroll = nil;
	ivec2 len = IMUI_State_GetLen (IMUI_context, nil);
	len.y = count * ts.y;
	IMUI_State_SetLen (IMUI_context, nil, len);
	IMUI_SetViewPos (IMUI_context, { 0, -pos.y % ts.y });
	len = sblen;
	size = len / ts;
	len.y = (len.y + ts.y - 1) / ts.y + 1;
	//FIXME utf-8
	int width = len.x / ts.x + 2;
	[self scrollTo:pos.y / ts.y];
	uint lind = base_index;
	for (uint i = base.y; len.y-- > 0 && i < count; i++) {
		int buf[1024];
		lind = [buffer formatLine:lind from:base.x into:buf width:width
				highlight:{0,0} colors:{ 15, 0 }];
		IMUI_IntLabel (IMUI_context, buf, width);
	}
	if (cursor.y >= base.y && cursor.y <= base.y + size.y + 1) {
		UI_Layout(false) {
			IMUI_Layout_SetXSize (IMUI_context, imui_size_none, 0);
			IMUI_Layout_SetYSize (IMUI_context, imui_size_none, 0);
			ivec2 cpos = (cursor - base) * ts - ivec2(1,0);
			//FIXME with this call *before* IMUI_SetViewPos (use of cpos)
			//qfcc ices
			//printf ("%d,%d %d,%d %d,%d\n", cursor.x, cursor.y, base.x, base.y,
			//		cpos.x, cpos.y);
			IMUI_SetViewPos (IMUI_context, cpos);
			IMUI_SetViewLen (IMUI_context, {3, ts.y});
			IMUI_SetViewFree (IMUI_context, {true, true});
			IMUI_SetFill (IMUI_context, 0x0e);
		}
	}
	return self;
}

-draw
{
	UI_ScrollBox(name + "##EditView:scroller") {
		IMUI_SetFocus (IMUI_context, true);
		imui_key_t key = {};
		if (IMUI_GetKey (IMUI_context, &key)) {
			[self handleKey:key];
		}
		auto sblen = IMUI_State_GetLen (IMUI_context, nil);
		UI_Scroller () {
			[self draw_text:sblen];
		}
	}
	UI_ScrollBar (name + "##EditView:scroller");
	return self;
}

@end
