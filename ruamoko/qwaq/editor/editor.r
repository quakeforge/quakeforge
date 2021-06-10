#include <QF/keys.h>
#include <string.h>
#include "ruamoko/qwaq/qwaq-app.h"
#include "ruamoko/qwaq/editor/editor.h"
#include "ruamoko/qwaq/ui/listener.h"
#include "ruamoko/qwaq/ui/scrollbar.h"

@implementation Editor

-initWithRect:(Rect) rect file:(string) filename path:(string) filepath
{
	if (!(self = [super initWithRect: rect])) {
		return nil;
	}
	self.filename = str_hold (filename);
	if (filepath != filename) {
		self.filepath = str_hold (filepath);
	} else {
		self.filepath = filename;
	}
	buffer = [[EditBuffer withFile:filepath] retain];
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
	return [[[self alloc] initWithRect:rect
								  file:filename
								  path:filename] autorelease];
}

+(Editor *)withRect:(Rect)rect file:(string)filename path:(string)filepath
{
	return [[[self alloc] initWithRect:rect
								  file:filename
								  path:filepath] autorelease];
}

-(void)dealloc
{
	if (filepath != filename) {
		str_free (filepath);
	}
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

-(string)filepath
{
	return filepath;
}

-(Point)cursor
{
	return cursor;
}

-setStatusView:(EditStatus *)status
{
	self.status = status;
	[status setModified:modified];
	[status setCursorMode:cursorMode];
	[status redraw];
	return self;
}

-trackCursor:(int)fwd
{
	unsigned    tx = [buffer charPos:line_index at:char_index];

	cursorMode &= ~cmVInsert;
	if (tx != cursor.x) {
		if (char_index < [buffer getEOT]) {
			int         c = [buffer getChar:char_index];

			if (virtualInsert) {
				if (c != '\t' || cursorThroughTabs) {
					cursorMode |= cmVInsert;
					goto done;
				}
			}
			if (c == '\t' && fwd) {
				tx = [buffer charPos:line_index at:++char_index];
			}
		} else if (virtualInsert) {
			cursorMode |= cmVInsert;
			goto done;
		}
		cursor.x = tx;
	}
done:
	[status setCursorMode:cursorMode];
	[status redraw];
	return self;
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
				[self pageUp];
				return 1;
			case QFK_PAGEDOWN:
				[self pageDown];
				return 1;
			case QFK_UP:
				[self charUp];
				return 1;
			case QFK_DOWN:
				[self charDown];
				return 1;
			case QFK_LEFT:
				[self charLeft];
				return 1;
			case QFK_RIGHT:
				[self charRight];
				return 1;
			case QFK_HOME:
				[self moveBOL];
				return 1;
			case QFK_END:
				[self moveEOL];
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
	unsigned    index;
	unsigned    lines;
	if (count == 1) {
		index = [buffer prevLine: base_index];
	} else {
		index = [buffer prevLine: base_index :count];
	}
	lines = [buffer countLines: {index, base_index - index}];
	base.y -= lines;
	base_index = index;
	[self redraw];
	[self moveCursor: {cursor.x - base.x, cursor.y - base.y}];
	return self;
}

-scrollDown:(unsigned) count
{
	unsigned    index;
	unsigned    lines;
	if (count == 1) {
		index = [buffer nextLine: base_index];
	} else {
		index = [buffer nextLine: base_index :count];
	}
	lines = [buffer countLines: {base_index, index - base_index}];
	base.y += lines;
	base_index = index;
	[self redraw];
	[self moveCursor: {cursor.x - base.x, cursor.y - base.y}];
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
	[self moveCursor: {cursor.x - base.x, cursor.y - base.y}];
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
	[self moveCursor: {cursor.x - base.x, cursor.y - base.y}];
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
	[vScrollBar setIndex:base.y];
	[self moveCursor: {cursor.x - base.x, cursor.y - base.y}];
	return self;
}

-pageUp
{
	[self recenter:0];
	unsigned count = cursor.y;

	if (count > ylen) {
		count = ylen;
	}
	if (count) {
		cursor.y -= count;
		[vScrollBar setIndex:[vScrollBar index] - count];
		line_index = [buffer prevLine:line_index :count];
		char_index = [buffer charPtr:line_index at:cursor.x];
		[self trackCursor:1];

		[self moveCursor: {cursor.x - base.x, cursor.y - base.y}];
	}
	return self;
}

-pageDown
{
	[self recenter:0];
	unsigned count = line_count - cursor.y;

	if (count > ylen) {
		count = ylen;
	}
	if (count) {
		cursor.y += count;
		[vScrollBar setIndex:[vScrollBar index] + count];
		line_index = [buffer nextLine:line_index :count];
		char_index = [buffer charPtr:line_index at:cursor.x];
		[self trackCursor:1];

		[self moveCursor: {cursor.x - base.x, cursor.y - base.y}];
	}
	return self;
}

-charUp
{
	[self recenter:0];
	if (cursor.y < 1) {
		return self;
	}
	cursor.y--;
	line_index = [buffer prevLine:line_index :1];
	char_index = [buffer charPtr:line_index at:cursor.x];
	[self trackCursor:1];

	if (base.y > cursor.y) {
		[vScrollBar setIndex:cursor.y];
	}
	[self moveCursor: {cursor.x - base.x, cursor.y - base.y}];
	return self;
}

-charDown
{
	[self recenter:0];
	if (cursor.y >= line_count) {
		return self;
	}
	cursor.y++;
	line_index = [buffer nextLine:line_index :1];
	char_index = [buffer charPtr:line_index at:cursor.x];
	[self trackCursor:1];

	if (base.y + ylen - 1 < cursor.y) {
		[vScrollBar setIndex:cursor.y + 1 - ylen];
	}
	[self moveCursor: {cursor.x - base.x, cursor.y - base.y}];
	return self;
}

-charLeft
{
	[self recenter:0];
	if (cursor.x < 1) {
		return self;
	}
	cursor.x--;
	char_index = [buffer charPtr:line_index at:cursor.x];
	[self trackCursor:0];

	if (base.x > cursor.x) {
		[hScrollBar setIndex:cursor.x];
	}
	[self moveCursor: {cursor.x - base.x, cursor.y - base.y}];
	return self;
}

-charRight
{
	[self recenter:0];
	// FIXME handle horizontal scrolling and how to deal with max scroll
	cursor.x++;
	char_index = [buffer charPtr:line_index at:cursor.x];
	[self trackCursor:1];

	if (base.x + xlen - 1 < cursor.x) {
		[hScrollBar setIndex:cursor.x + 1 - xlen];
	}
	if (base.x + xlen - 1 < cursor.x) {
		cursor.x = base.x + xlen - 1;
	}
	[self moveCursor: {cursor.x - base.x, cursor.y - base.y}];
	return self;
}

-moveBOL
{
	char_index = line_index;
	cursor.x = 0;
	base.x = 0;
	[self recenter:0];
	[self moveCursor: {cursor.x - base.x, cursor.y - base.y}];
	return self;
}

-moveEOL
{
	char_index = [buffer getEOL:line_index];
	cursor.x = [buffer charPos:line_index at:char_index];
	[self recenter:0];
	[self moveCursor: {cursor.x - base.x, cursor.y - base.y}];
	return self;
}

-(void)onScroll:(id)sender
{
	base.x = scroll.x;
	if (base.y != scroll.y) {
		[self scrollTo:scroll.y];
	}
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
	[self recenter:0];
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
