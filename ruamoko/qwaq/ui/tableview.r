#include <Array.h>
#include "ruamoko/qwaq/ui/listener.h"
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
	return self;
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

@end
