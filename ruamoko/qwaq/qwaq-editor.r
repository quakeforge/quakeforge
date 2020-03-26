#include <QF/keys.h>
#include "qwaq-app.h"
#include "qwaq-editor.h"
#include "qwaq-listener.h"

@implementation Editor

-initWithRect:(Rect) rect file:(string) filename
{
	if (!(self = [super initWithRect: rect])) {
		return nil;
	}
	self.filename = filename;
	buffer = [[EditBuffer alloc] initWithFile: filename];
	line_count = [buffer countLines: {0, [buffer textSize]}];
	linebuffer = [DrawBuffer buffer: { xlen, 1 }];
	growMode = gfGrowHi;
	options = ofCanFocus;
	onEvent = [[ListenerGroup alloc] init];
	return self;
}

-(ListenerGroup *)onEvent
{
	return onEvent;
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
		lind = [buffer formatLine:lind from:scroll.x into:lbuf width:xlen
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

static int handleEvent (Editor *self, qwaq_event_t *event)
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
	// give any listeners a chance to override or extend event handling
	_event.editor = self;
	_event.event = event;
	if (event.what & qe_positional) {
		event.mouse.x -= xpos;
		event.mouse.y -= ypos;
	}
	[onEvent respond: &_event];
	if (handleEvent (self, event)) {
		event.what = qe_none;
	}
	if (event.what & qe_positional) {
		event.mouse.x += xpos;
		event.mouse.y += ypos;
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
	if (scroll.x > count) {
		scroll.x -= count;
	} else {
		scroll.x = 0;
	}
	[self redraw];
	return self;
}

-scrollRight:(unsigned) count
{
	if (1024 - scroll.x > count) {
		scroll.x += count;
	} else {
		scroll.x = 1024;
	}
	[self redraw];
	return self;
}

-recenter:(int) force
{
	if (!force) {
		if (cursor.y >= scroll.y && cursor.y - scroll.y < ylen) {
			return self;
		}
	}
	unsigned target;
	if (cursor.y < ylen / 2) {
		target = 0;
	} else {
		target = cursor.y - ylen / 2;
	}
	if (target > scroll.y) {
		base_index = [buffer nextLine:base_index :target - scroll.y];
	} else if (target < scroll.y) {
		base_index = [buffer prevLine:base_index :scroll.y - target];
	}
	scroll.y = target;
	return self;
}

-gotoLine:(unsigned) line
{
	if (line > cursor.y) {
		line_index = [buffer nextLine:line_index :line - cursor.y];
	} else if (line < cursor.y) {
		line_index = [buffer prevLine:line_index :cursor.y - line];
	}
	cursor.y = line;
	[self recenter: 0];
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
	pos.x += scroll.x;
	unsigned lind = [buffer nextLine:base_index :pos.y];
	unsigned cind = [buffer charPtr:lind at:pos.x];
	eb_sel_t word = [buffer getWord: cind];
	return [buffer readString:word];
}

@end
