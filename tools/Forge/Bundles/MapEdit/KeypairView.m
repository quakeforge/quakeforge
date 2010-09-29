#include "KeypairView.h"
#include "Map.h"
#include "Entity.h"
#include "Things.h"

id  keypairview_i;

@implementation KeypairView
/*
==================
initWithFrame:
==================
*/
- (id) initWithFrame: (NSRect)frameRect
{
	[super initWithFrame: frameRect];
	keypairview_i = self;
	return self;
}

- (BOOL) isFlipped
{
	return YES;
}

- (id) calcViewSize
{
	NSRect      b;
	NSPoint     pt;
	int         count;
	id          ent;

	ent = [map_i currentEntity];
	count = [ent numPairs];

	b = [[self superview] bounds];
	b.size.height = LINEHEIGHT * count + SPACING;
	[self setFrameSize: b.size];
	pt.x = pt.y = 0;
	[self scrollPoint: pt];
	return self;
}

- (id) drawRect: (NSRect)rects
{
	epair_t     *pair;
	int         y;
	NSMutableDictionary  *attribs = [NSMutableDictionary dictionary];

	[[NSColor lightGrayColor] set];
	NSRectFill (NSMakeRect (0, 0, _bounds.size.width, _bounds.size.height));

	[[NSFont systemFontOfSize: FONTSIZE] set];
	[[NSColor blackColor] set];

	pair = [[map_i currentEntity] epairs];
	y = _bounds.size.height - LINEHEIGHT;
	for ( ; pair; pair = pair->next) {
		NSString    *key = [NSString stringWithCString: pair->key];
		NSString    *value = [NSString stringWithCString: pair->value];

		[key drawAtPoint: NSMakePoint (SPACING, y) withAttributes: attribs];
		[value drawAtPoint: NSMakePoint (100, y) withAttributes: attribs];
		y -= LINEHEIGHT;
	}

	return self;
}

- (void) mouseDown: (NSEvent *)theEvent
{
	NSPoint     loc;
	int         i;
	epair_t     *p;

	loc = [theEvent locationInWindow];
	loc = [self convertPoint: loc fromView: NULL];

	i = (_bounds.size.height - loc.y - 4) / LINEHEIGHT;

	p = [[map_i currentEntity] epairs];
	while (i) {
		p = p->next;
		if (!p)
			return;
		i--;
	}
	if (p)
		[things_i setSelectedKey: p];

	return;
}

@end
