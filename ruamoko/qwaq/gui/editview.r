#include <QF/keys.h>
#include <QF/input/event.h>
#include <Array.h>

#include <stdlib.h>
#include <dirent.h>
#include <imui.h>
#include <msgbuf.h>
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
	//filepath is owned by the window
	self.filepath = filepath;
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

-selection:(int)sel_mode dir:(bool)dir
{
	if (!sel_mode) {
		sel = { char_index, char_index };
		old_char_index = char_index;
		return self;
	}
	if (old_char_index != char_index) {
		old_char_index = char_index;
		if (dir) {
			if (sel.y < char_index) {
				sel.y = char_index;
			} else {
				sel.x = char_index;
			}
		} else {
			if (sel.x > char_index) {
				sel.x = char_index;
			} else {
				sel.y = char_index;
			}
		}
	}
	return self;
}

-moveBOT:(int)sel_mode
{
	line_index = base_index = char_index = 0;
	cursor = nil;
	base = nil;
	override_scroll = { true, true };
	[self selection:sel_mode dir:false];
	return self;
}

-moveEOT:(int)sel_mode
{
	char_index = [buffer getEOT];
	line_index = [buffer getBOL:char_index];
	cursor.x = [buffer charPos:line_index at:char_index];
	cursor.y = line_count;
	if (cursor.y >= size.y) {
		[self scrollTo: cursor.y - size.y + 1];
	}
	override_scroll = { true, true };
	[self selection:sel_mode dir:true];
	return self;
}

-pageUp:(int)sel_mode
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
	[self selection:sel_mode dir:false];
	return self;
}

-pageDown:(int)sel_mode
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
	[self selection:sel_mode dir:true];
	return self;
}

-linesUp:(int)sel_mode
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
	[self selection:sel_mode dir:false];
	return self;
}

-linesDown:(int)sel_mode
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
	[self selection:sel_mode dir:true];
	return self;
}

-charUp:(int)sel_mode
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
	[self selection:sel_mode dir:false];
	return self;
}

-charDown:(int)sel_mode
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
	[self selection:sel_mode dir:true];
	return self;
}

-wordLeft:(int)sel_mode
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
	[self selection:sel_mode dir:false];

	return self;
}

-wordRight:(int)sel_mode
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
	[self selection:sel_mode dir:true];

	return self;
}

-charLeft:(int)sel_mode
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
	[self selection:sel_mode dir:false];
	return self;
}

-charRight:(int)sel_mode
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
	[self selection:sel_mode dir:true];
	return self;
}

-moveBOS:(int)sel_mode
{
	line_index = base_index;
	cursor.y = base.y;
	char_index = [buffer charPtr:line_index at:cursor.x];
	[self selection:sel_mode dir:false];
	return self;
}

-moveEOS:(int)sel_mode
{
	uint count = line_count - base.y;
	if (count > size.y - 1) {
		count = size.y - 1;
	}
	line_index = [buffer nextLine:base_index :count];
	cursor.y = base.y + count;
	char_index = [buffer charPtr:line_index at:cursor.x];
	[self selection:sel_mode dir:true];
	return self;
}

-moveBOL:(int)sel_mode
{
	char_index = line_index;
	cursor.x = 0;
	base.x = 0;
	override_scroll.x = true;
	[self selection:sel_mode dir:false];
	return self;
}

-moveEOL:(int)sel_mode
{
	char_index = [buffer getEOL:line_index];
	cursor.x = [buffer charPos:line_index at:char_index];
	[self recenter:0];
	[self selection:sel_mode dir:true];
	return self;
}

-typeChar:(int)chr
{
	[self recenter:false];
	if (false) {//typeover
	} else {
		[buffer insertChar:chr at:char_index];
		if (chr == '\n') {
			cursor.y++;
			line_count++;
			line_index = [buffer nextLine:line_index];
			char_index = line_index;
		} else {
			char_index = [buffer nextChar:char_index];
		}
		old_char_index = char_index;
		cursor.x = [buffer charPos:line_index at:char_index];
	}
	return self;
}

-insertMsgBuf:(msgbuf_t)buf
{
	uint len = MsgBuf_CurSize (buf);
	// FIXME assumes a nul-terminated string
	if (len-- < 2) {
		printf ("too short\n");
		return self;
	}
	uint lines = [buffer insertMsgBuf:buf at:char_index];
	cursor.y += lines;
	line_count += lines;
	line_index = [buffer nextLine:line_index :lines];
	char_index += len;
	old_char_index = char_index;
	cursor.x = [buffer charPos:line_index at:char_index];
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
	//FIXME this prevents breaking a utf-8 encoded char by deleting its first
	//byte, but it would be better to more effectively hide such details
	uint next = [buffer nextChar:char_index];
	uint count = next - char_index;
	if (count < 1) {
		count = 1;
	}
	[buffer deleteText:{char_index, count}];
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
	old_char_index = char_index;
	cursor.x = [buffer charPos:line_index at:char_index];
	if (!nodelete) {
		uint lines = [buffer countLines:{char_index, len}];
		line_count -= lines;
		override_scroll.y = lines;
		[buffer deleteText:{char_index, len}];
	}
	return self;
}

-jumpTo:(uvec2)pos mode:(int)sel_mode
{
	uint oci = char_index;
	if (pos.y > line_count) {
		pos.y = line_count;
	}
	if (cursor.y < pos.y) {
		line_index = [buffer nextLine:line_index :pos.y - cursor.y];
	} else if (cursor.y > pos.y) {
		line_index = [buffer prevLine:line_index :cursor.y - pos.y];
	}
	char_index = [buffer charPtr:line_index at:pos.x];
	cursor = pos;
	[self recenter:false];
	[self selection:sel_mode dir:char_index > oci];
	return self;
}

-handleKey:(imui_key_t)key
{
	switch (key.code) {
	case QFK_F2:
		[buffer saveFile:filepath];
		break;
	case QFK_STRING:
		[self insertMsgBuf:IMUI_GetKeyString (IMUI_context)];
		break;
	case QFK_PAGEUP:
		if (key.shift & ies_control) {
			[self moveBOT:key.shift & ies_shift];
		} else {
			[self pageUp:key.shift & ies_shift];
		};
		break;
	case QFK_PAGEDOWN:
		if (key.shift & ies_control) {
			[self moveEOT:key.shift & ies_shift];
		} else {
			[self pageDown:key.shift & ies_shift];
		};
		break;
	case QFK_UP:
		if (key.shift & ies_control) {
			[self linesUp:key.shift & ies_shift];
		} else {
			[self charUp:key.shift & ies_shift];
		};
		break;
	case QFK_DOWN:
		if (key.shift & ies_control) {
			[self linesDown:key.shift & ies_shift];
		} else {
			[self charDown:key.shift & ies_shift];
		};
		break;
	case QFK_LEFT:
		if (key.shift & ies_control) {
			[self wordLeft:key.shift & ies_shift];
		} else {
			[self charLeft:key.shift & ies_shift];
		};
		break;
	case QFK_RIGHT:
		if (key.shift & ies_control) {
			[self wordRight:key.shift & ies_shift];
		} else {
			[self charRight:key.shift & ies_shift];
		};
		break;
	case QFK_HOME:
		if (key.shift & ies_control) {
			[self moveBOS:key.shift & ies_shift];
		} else {
			[self moveBOL:key.shift & ies_shift];
		}
		break;
	case QFK_END:
		if (key.shift & ies_control) {
			[self moveEOS:key.shift & ies_shift];
		} else {
			[self moveEOL:key.shift & ies_shift];
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

-handleMouse:(imui_io_t)io
{
	if (io.pressed & 1) {
		ivec2 c = (io.mouse_hot + scroll_pos + (X_size >> ivec2(1, 31)));
		//ivec2 d = c / X_size;//FIXME bug in qfcc when X_size is uvec2
		c /= X_size;
		[self jumpTo:c mode:0];
	} else if (io.buttons & 1) {
		ivec2 c = (io.mouse_hot + scroll_pos + (X_size >> ivec2(1, 31)));
		//ivec2 d = c / X_size;//FIXME bug in qfcc when X_size is uvec2
		c /= X_size;
		if (c.x < 0) c.x = 0;
		if (c.y < 0) c.y = 0;
		[self jumpTo:c mode:1];
	}
	return self;
}

-draw_text:(ivec2)sblen
{
	uint count = line_count + 1;
	ivec2 pos = scroll_pos;
	if (override_scroll.x) {
		pos.x = base.x * X_size.x;
	}
	if (override_scroll.y) {
		pos.y = base.y * X_size.y;
	}
	IMUI_State_SetPos (IMUI_context, nil, pos);
	override_scroll = nil;
	ivec2 len = IMUI_State_GetLen (IMUI_context, nil);
	len.y = count * X_size.y;
	IMUI_State_SetLen (IMUI_context, nil, len);
	IMUI_SetViewPos (IMUI_context, { 0, -pos.y % X_size.y });
	len = sblen;
	size = len / X_size;
	len.y = (len.y + X_size.y - 1) / X_size.y + 1;
	//FIXME utf-8
	int width = len.x / X_size.x + 2;
	[self scrollTo:pos.y / X_size.y];
	uint lind = base_index;
	for (uint i = base.y; len.y-- > 0 && i < count; i++) {
		int buf[1024];
		lind = [buffer formatLine:lind from:base.x into:buf width:width
						highlight:{sel.x, sel.y - sel.x}
						   colors:{ 61 << 21, 0100 << 21 }];
		IMUI_IntLabel (IMUI_context, buf, width);
		IMUI_SetActive (IMUI_context, false);
	}
	if (cursor.y >= base.y && cursor.y <= base.y + size.y + 1) {
		UI_Layout(false) {
			IMUI_Layout_SetXSize (IMUI_context, imui_size_none, 0);
			IMUI_Layout_SetYSize (IMUI_context, imui_size_none, 0);
			ivec2 cpos = (cursor - base) * X_size - ivec2(1,0);
			//FIXME with this call *before* IMUI_SetViewPos (use of cpos)
			//qfcc ices
			//printf ("%d,%d %d,%d %d,%d\n", cursor.x, cursor.y, base.x, base.y,
			//		cpos.x, cpos.y);
			IMUI_SetViewPos (IMUI_context, cpos);
			IMUI_SetViewLen (IMUI_context, {3, X_size.y});
			IMUI_SetViewFree (IMUI_context, {true, true});
			IMUI_SetFill (IMUI_context, 0x0e);
		}
	}
	return self;
}

-draw
{
	//FIXME assumes fixed-width
	X_size = IMUI_TextSize (IMUI_context, "X");

	UI_ScrollBox(name + "##EditView:scroller") {
		IMUI_SetActive (IMUI_context, true);
		IMUI_SetFocus (IMUI_context, true);
		int mode = IMUI_UpdateHotActive (IMUI_context);
		int but = IMUI_CheckButtonState (IMUI_context);
		auto io = IMUI_GetIO (IMUI_context);
		imui_key_t key = {};
		if (IMUI_GetKey (IMUI_context, &key)) {
			[self handleKey:key];
		}
		auto sblen = IMUI_State_GetLen (IMUI_context, nil);
		UI_Scroller () {
			scroll_pos = IMUI_State_GetPos (IMUI_context, nil);
			if (io.active == io.self || io.hot == io.self) {
				[self handleMouse:io];
			}
			[self draw_text:sblen];
		}
	}
	UI_ScrollBar (name + "##EditView:scroller");
	return self;
}

-(bool)modified
{
	return [buffer modified];
}

@end
