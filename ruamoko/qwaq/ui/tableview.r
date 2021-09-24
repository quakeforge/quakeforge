#include <Array.h>
#include "ruamoko/qwaq/ui/listener.h"
#include "ruamoko/qwaq/ui/scrollbar.h"
#include "ruamoko/qwaq/ui/tableview.h"

@implementation TableViewColumn
-initWithName:(string)name width:(int)width
{
	if (!(self = [super init])) {
		return nil;
	}
	self.name = name;
	self.width = width;
	return self;
}

+(TableViewColumn *)named:(string)name
{
	return [[[self alloc] initWithName:name width:-1] autorelease];
}

+(TableViewColumn *)named:(string)name width:(int)width
{
	return [[[self alloc] initWithName:name width:width] autorelease];
}

-setGrowMode: (int) mode
{
	growMode = mode;
	return self;
}

-(int)growMode
{
	return growMode;
}

-(string)name
{
	return name;
}

-(int)width
{
	return width;
}

-setWidth:(int)width
{
	self.width = width;
	return self;
}

-grow:(Extent)delta
{
	if (growMode & gfGrowHiX) {
		width += delta.width;
	}
	return self;
}
@end

@implementation TableView
-initWithRect:(Rect)rect
{
	if (!(self = [super initWithRect:rect])) {
		return nil;
	}
	options = ofCanFocus | ofRelativeEvents;
	columns = [[Array array] retain];
	buffer = [[DrawBuffer buffer:size] retain];
	[buffer bkgd:' '];
	[onViewScrolled addListener:self :@selector(onScroll:)];
	growMode = gfGrowHi;
	return self;
}

-(void)dealloc
{
	[columns release];
	[buffer release];
	[dataSource release];
	[super dealloc];
}

+(TableView *)withRect:(Rect)rect
{
	return [[[self alloc] initWithRect:rect] autorelease];
}

-addColumn:(TableViewColumn *)column
{
	[columns addObject:column];
	columns_dirty = 1;
	return self;
}

-setDataSource:(id<TableViewDataSource>)dataSource
{
	self.dataSource = [dataSource retain];
	[[dataSource onRowCountChanged] addListener:self
											   :@selector(onRowCountChanged:)];
	return self;
}

-(void)onRowCountChanged:(id)sender
{
	[vScrollBar setRange:[sender numberOfRows:self]];
}

-resize:(Extent)delta
{
	Extent size = self.size;
	[super resize:delta];
	[buffer resizeTo:self.size];
	for (int i = [columns count]; i-- > 0; ) {
		[[columns objectAtIndex: i] grow: delta];
	}
	return self;
}

-draw
{
	View       *cell;
	TableViewColumn *col;
	[super draw];
	int         numCols = [columns count];
	int         numRows = [dataSource numberOfRows:self];
	[buffer clear];
	for (int y = 0; y < ylen; y++) {
		for (int i = 0, x = 0; i < numCols; i++) {
			int         row = base.y + y;
			if (row >= numRows) {
				break;
			}
			col = [columns objectAtIndex:i];
			cell = [dataSource tableView:self forColumn:col row:row];
			[[[cell setContext:buffer] moveTo:{x, y}] draw];
			x += [col width];
		}
	}
	[textContext blitFromBuffer:buffer to:pos from:[buffer rect]];
	return self;
}

-(void)onScroll:(id)sender
{
	if (base.y != scroll.y) {
		base.y = scroll.y;
		[self redraw];
	}
}

static int
handleEvent (TableView *self, qwaq_event_t *event)
{
	if (event.what & qe_mouse) {
		if (event.what == qe_mouseclick) {
			if (event.mouse.buttons & (1 << 3)) {
				[self.vScrollBar page:1 dir:0];
				return 1;
			}
			if (event.mouse.buttons & (1 << 4)) {
				[self.vScrollBar page:1 dir:1];
				return 1;
			}
#if 0
			if (event.mouse.buttons & (1 << 5)) {
				[self scrollLeft: 1];
				return 1;
			}
			if (event.mouse.buttons & (1 << 6)) {
				[self scrollRight: 1];
				return 1;
			}
#endif
		}
	} else if (event.what == qe_keydown) {
#if 0
		switch (event.key.code) {
			case QFK_PAGEUP:
				if (event.key.shift & qe_control) {
					[self moveBOT];
				} else {
					[self pageUp];
				}
				return 1;
			case QFK_PAGEDOWN:
				if (event.key.shift & qe_control) {
					[self moveEOT];
				} else {
					[self pageDown];
				}
				return 1;
			case QFK_UP:
				if (event.key.shift & qe_control) {
					[self linesUp];
				} else {
					[self charUp];
				}
				return 1;
			case QFK_DOWN:
				if (event.key.shift & qe_control) {
					[self linesDown];
				} else {
					[self charDown];
				}
				return 1;
			case QFK_LEFT:
				if (event.key.shift & qe_control) {
					[self wordLeft];
				} else {
					[self charLeft];
				}
				return 1;
			case QFK_RIGHT:
				if (event.key.shift & qe_control) {
					[self wordRight];
				} else {
					[self charRight];
				}
				return 1;
			case QFK_HOME:
				if (event.key.shift & qe_control) {
					[self moveBOS];
				} else {
					[self moveBOL];
				}
				return 1;
			case QFK_END:
				if (event.key.shift & qe_control) {
					[self moveEOS];
				} else {
					[self moveEOL];
				}
				return 1;
		}
#endif
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

@end
