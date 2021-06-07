#include <QF/keys.h>
#include <string.h>
#include "ruamoko/qwaq/qwaq-app.h"
#include "ruamoko/qwaq/editor/editor.h"
#include "ruamoko/qwaq/ui/listener.h"
#include "ruamoko/qwaq/ui/scrollbar.h"

@implementation Editor

-initWithRect:(Rect) rect file:(string) filename
{
	if (!(self = [super initWithRect: rect])) {
		return nil;
	}
	self.filename = str_hold (filename);
	buffer = [[EditBuffer withFile:filename] retain];
	line_count = [buffer countLines: {0, [buffer textSize]}];
	linebuffer = [[DrawBuffer buffer: { xlen, 1 }] retain];
	growMode = gfGrowHi;
	options = ofCanFocus | ofRelativeEvents;
	[onViewScrolled addListener:self :@selector(onScroll:)];
	[self setCursorVisible:1];
	return self;
}

+(Editor *)withRect:(Rect)rect file:(string)filename
{
	return [[[self alloc] initWithRect:rect file:filename] autorelease];
}

-(void)dealloc
{
	str_free (filename);
	[vScrollBar release];
	[buffer release];
	[linebuffer release];
	[super dealloc];
}

-(string)filename
{
	return filename;
}

-draw
{
	[super draw];
	unsigned lind = base_index;
	int *lbuf = [linebuffer buffer];
	for (int y = 0; y < ylen; y++) {
		lind = [buffer formatLine:lind from:base.x into:lbuf width:xlen
				highlight:selection colors: {color_palette[047],
											 color_palette[007]}];
		[textContext blitFromBuffer: linebuffer to: {xpos, ypos + y}
							from: [linebuffer rect]];
	}
	return self;
}

-resize: (Extent) delta
{
	[super resize: delta];
	[linebuffer resizeTo: {xlen, 1}];
	return self;
}

static int
handleEvent (Editor *self, qwaq_event_t *event)
{
	if (event.what & qe_mouse) {
		if (event.what == qe_mouseclick) {
			if (event.mouse.buttons & (1 << 3)) {
				[self scrollUp: 1];
				return 1;
			}
			if (event.mouse.buttons & (1 << 4)) {
				[self scrollDown: 1];
				return 1;
			}
			if (event.mouse.buttons & (1 << 5)) {
				[self scrollLeft: 1];
				return 1;
			}
			if (event.mouse.buttons & (1 << 6)) {
				[self scrollRight: 1];
				return 1;
			}
		}
	} else if (event.what == qe_keydown) {
		switch (event.key.code) {
			case QFK_PAGEUP:
				[self scrollUp: self.ylen];
				return 1;
			case QFK_PAGEDOWN:
				[self scrollDown: self.ylen];
				return 1;
		}
	}
	return 0;
}

-handleEvent:(qwaq_event_t *) event
{
	[super handleEvent: event];

	if (handleEvent (self, event)) {
		event.what = qe_none;
	}
	return self;
}

-scrollUp:(unsigned) count
{
	if (count == 1) {
		base_index = [buffer prevLine: base_index];
	} else {
		base_index = [buffer prevLine: base_index :count];
	}
	[self redraw];
	return self;
}

-scrollDown:(unsigned) count
{
	if (count == 1) {
		base_index = [buffer nextLine: base_index];
	} else {
		base_index = [buffer nextLine: base_index :count];
	}
	[self redraw];
	return self;
}

-scrollLeft:(unsigned) count
{
	if (base.x > count) {
		base.x -= count;
	} else {
		base.x = 0;
	}
	[self redraw];
	return self;
}

-scrollRight:(unsigned) count
{
	if (1024 - base.x > count) {
		base.x += count;
	} else {
		base.x = 1024;
	}
	[self redraw];
	return self;
}

-scrollTo:(unsigned)target
{
	if (target > base.y) {
		base_index = [buffer nextLine:base_index :target - base.y];
	} else if (target < base.y) {
		base_index = [buffer prevLine:base_index :base.y - target];
	}
	base.y = target;
	[self moveCursor: {cursor.x - base.x, cursor.y - base.y}];
	return self;
}

-(void)onScroll:(id)sender
{
	base.x = scroll.x;
	[self scrollTo:scroll.y];
}

-setVerticalScrollBar:(ScrollBar *)scrollbar
{
	[super setVerticalScrollBar:scrollbar];
	[vScrollBar setRange:line_count];
	[vScrollBar setPageStep: ylen];
	return self;
}

-recenter:(int) force
{
	if (force || cursor.y < base.y || cursor.y - base.y >= ylen) {
		unsigned target;
		if (cursor.y < ylen / 2) {
			target = 0;
		} else {
			target = cursor.y - ylen / 2;
		}
		[self scrollTo:target];
	}
	return self;
}

static void
trackCursor (Editor *self)
{
	unsigned    cx = [self.buffer charPos:self.line_index at:self.char_index];
	if (self.cursor.x != cx) {
		if (self.char_index < [self.buffer getEOT]) {
			int c = [self.buffer getChar:self.char_index];
		}
	}
}

-gotoLine:(unsigned) line
{
	if (line > cursor.y) {
		line_index = [buffer nextLine:line_index :line - cursor.y];
	} else if (line < cursor.y) {
		line_index = [buffer prevLine:line_index :cursor.y - line];
	}
	cursor.y = line;
	char_index = [buffer charPtr:line_index at:cursor.x];
	[self recenter: 0];
	[self moveCursor: {cursor.x - base.x, cursor.y - base.y}];
	return self;
}

-highlightLine
{
	selection.start = line_index;
	selection.length = [buffer nextLine: line_index] - line_index;
	if (!selection.length) {
		selection.length = [buffer getEOL: line_index] - line_index;
	}
	return self;
}

-(string)getWordAt:(Point) pos
{
	if (pos.x < 0 || pos.y < 0 || pos.x >= xlen || pos.y >= ylen) {
		return nil;
	}
	pos.x += base.x;
	unsigned lind = [buffer nextLine:base_index :pos.y];
	unsigned cind = [buffer charPtr:lind at:pos.x];
	eb_sel_t word = [buffer getWord: cind];
	return [buffer readString:word];
}

@end
