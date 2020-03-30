#include "ui/button.h"
#include "ui/group.h"
#include "ui/listener.h"
#include "ui/scrollbar.h"

@implementation ScrollBar

-initWithRect:(Rect)rect
{
	if (!(self = [super initWithRect:rect])) {
		return nil;
	}
	objects = [[Group withContext:textContext owner:self] retain];
	onScroll = [[ListenerGroup listener] retain];
	vertical = xlen == 1;
	DrawBuffer *icons[3] = {
		[DrawBuffer buffer:{1, 1}],
		[DrawBuffer buffer:{1, 1}],
		[DrawBuffer buffer:{1, 1}],
	};
	[icons[2] addch:acs_char (ACS_DIAMOND)];
	Point thumbPos;
	SEL thumbSel;
	growMode = gfGrowAll;
	if (vertical) {
		[icons[0]addch:acs_char (ACS_UARROW)];
		[icons[1]addch:acs_char (ACS_DARROW)];
		thumbPos = {0, 1};
		thumbSel = @selector(verticalSlide);
		growMode &= ~gfGrowLoY;
	} else {
		[icons[0]addch:acs_char (ACS_LARROW)];
		[icons[1]addch:acs_char (ACS_RARROW)];
		thumbPos = {1, 0};
		thumbSel = @selector(horizontalSlide);
		growMode &= ~gfGrowLoX;
	}
	bgchar = acs_char (ACS_CKBOARD);
	backButton = [Button withPos:{0, 0}
					releasedIcon:icons[0] pressedIcon:icons[0]];
	forwardButton = [Button withPos:{xlen - 1, ylen - 1}
					   releasedIcon:icons[1] pressedIcon:icons[1]];
	thumbTab = [Button withPos:thumbPos
				  releasedIcon:icons[2] pressedIcon:icons[2]];

	[[backButton onPress] addListener:self :@selector(scrollBack:)];
	[[forwardButton onPress] addListener:self :@selector(scrollForward:)];
	[[thumbTab onPress] addListener:self :thumbSel];

	singleStep = 1;

	[objects insert:backButton];
	[objects insert:forwardButton];
	[objects insert:thumbTab];
	return self;
}

+(ScrollBar *)horizontal:(unsigned)len at:(Point)pos
{
	if (len == 1) {
		[self error:"can't make scrollbar of length 1"];
	}
	return [[[self alloc] initWithRect:{pos, {len, 1}}] autorelease];
}

+(ScrollBar *)vertical:(unsigned)len at:(Point)pos
{
	if (len == 1) {
		[self error:"can't make scrollbar of length 1"];
	}
	return [[[self alloc] initWithRect:{pos, {1, len}}] autorelease];
}

-(ListenerGroup *)onScroll
{
	return onScroll;
}

-draw
{
	if (vertical) {
		[self mvvline:pos, bgchar, ylen];
	} else {
		[self mvhline:pos, bgchar, xlen];
	}
	[super draw];
	[objects draw];
	return self;
}

static void
position_tab (ScrollBar *self)
{
	Point       p = {0, 0};
	Point       o = [self.thumbTab origin];
	if (self.range > 0) {
		if (self.vertical) {
			p.y = 1 + self.index * (self.ylen - 2) / (self.range - 1);
		} else {
			p.x = 1 + self.index * (self.xlen - 2) / (self.range - 1);
		}
	}
	[self.thumbTab move:{p.x - o.x, p.y - o.y}];
}

-resize:(Extent)delta
{
	Extent size = self.size;
	[super resize:delta];
	delta = {self.size.width - size.width, self.size.height - size.height};
	[objects resize:delta];
	position_tab (self);
	return self;
}

-page:(unsigned)step dir:(unsigned) dir
{
	unsigned    oind = index;

	if (dir) {
		if (range - 1 - index < step) {
			step = range - 1 - index;
		}
		index += step;
	} else {
		if (index < step) {
			step = index;
		}
		index -= step;
	}

	if (index != oind) {
		position_tab (self);
		[onScroll respond:self];
	}
	return self;
}

static void
page (ScrollBar *self, Point pos)
{
	unsigned    pageDir = 0;
	if (self.vertical) {
		if (pos.y < [self.thumbTab origin].y) {
			pageDir = 0;
		} else {
			pageDir = 1;
		}
	} else {
		if (pos.x < [self.thumbTab origin].x) {
			pageDir = 0;
		} else {
			pageDir = 1;
		}
	}
	[self page:self.pageStep dir:pageDir];
}

-(void)scrollBack:(id)sender
{
	[self page:singleStep dir:0];
}

-(void)scrollForward:(id)sender
{
	[self page:singleStep dir:1];
}

-handleEvent:(qwaq_event_t *)event
{
	[super handleEvent: event];
	[objects handleEvent: event];
	if (event.what == qe_mousedown) {
		[self grabMouse];
		mouseStart = {event.mouse.x, event.mouse.y};
		page(self, mouseStart);
		event.what = qe_none;
	} else if (event.what==qe_mouseauto) {
		page(self, mouseStart);
		event.what = qe_none;
	} else if (event.what == qe_mouseup) {
		[self releaseMouse];
		event.what = qe_none;
	}
	return self;
}

-setRange:(unsigned)range
{
	self.range = range;
	return self;
}

-setPageStep:(unsigned)pageStep
{
	self.pageStep = pageStep;
	return self;
}

-setSingleStep:(unsigned)singleStep
{
	self.singleStep = singleStep;
	return self;
}

-setIndex:(unsigned)index
{
	if (index > self.index) {
		[self page:index - self.index dir:1];
	} else {
		[self page:self.index - index dir:0];
	}
	return self;
}

@end
