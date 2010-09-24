#include "QF/sys.h"

#include "KeypairView.h"
#include "TextureView.h"
#include "TexturePalette.h"

#include "Storage.h"

/*

NOTE: I am specifically not using cached image reps, because the data is also needed for texturing the views, and a cached rep would waste tons of space.

*/

@implementation TextureView

-init
{
	deselectIndex = -1;
	return self;
}

-setParent:(id) from
{
	parent_i = from;
	return self;
}

-(BOOL) acceptsFirstMouse
{
	return YES;
}

-drawRect: (NSRect) rects
{
	int         i;
	int         max;
	id          list_i;
	texpal_t   *t;
	int         x;
	int         y;
	NSPoint     p;
	NSRect      r;
	int         selected;
	NSMutableDictionary *attribs = [NSMutableDictionary dictionary];

	selected =[parent_i getSelectedTexture];
	list_i =[parent_i getList];
	[[NSFont systemFontOfSize: FONTSIZE] set];

	[[NSColor lightGrayColor] set];
	NSRectFill (rects);

	if (!list_i)						// WADfile didn't init
		return self;

	if (deselectIndex != -1) {
		t =[list_i elementAt:deselectIndex];
		r = t->r;
		r.origin.x -= TEX_INDENT;
		r.origin.y -= TEX_INDENT;
		r.size.width += TEX_INDENT * 2;
		r.size.height += TEX_INDENT * 2;

		[[NSColor lightGrayColor] set];
		NSRectFill (r);
		p = t->r.origin;
		p.y += TEX_SPACING;
		[t->image drawAtPoint: p];
		[[NSColor blackColor] set];
		x = t->r.origin.x;
		y = t->r.origin.y + 7;
		[[NSString stringWithCString: t->name]
			drawAtPoint: NSMakePoint (x, y) withAttributes: attribs];
		deselectIndex = -1;
	}

	max =[list_i count];
	[[NSColor blackColor] set];

	for (i = 0; i < max; i++) {
		t =[list_i elementAt:i];
		r = t->r;
		r.origin.x -= TEX_INDENT / 2;
		r.size.width += TEX_INDENT;
		r.origin.y += 4;
		if (NSIntersectsRect (rects, r) == YES && t->display) {
			if (selected == i) {
				[[NSColor whiteColor] set];
				NSRectFill (r);
				[[NSColor redColor] set];
				NSFrameRect(r);
				[[NSColor blackColor] set];
			}

			p = t->r.origin;
			p.y += TEX_SPACING;
			[t->image drawAtPoint: p];
			x = t->r.origin.x;
			y = t->r.origin.y + 7;
			[[NSString stringWithCString: t->name]
				drawAtPoint: NSMakePoint (x, y) withAttributes: attribs];
		}
	}
	return self;
}

-deselect
{
	deselectIndex =[parent_i getSelectedTexture];
	return self;
}

-mouseDown:(NSEvent *) theEvent
{
	NSPoint     loc;
	int         i;
	int         max;

	// int oldwindowmask;
	texpal_t   *t;
	id          list;
	NSRect      r;

	// oldwindowmask = [window addToEventMask:NSLeftMouseDraggedMask];
	loc =[theEvent locationInWindow];
	[self convertPoint: loc fromView:NULL];

	list =[parent_i getList];
	max =[list count];
	for (i = 0; i < max; i++) {
		t =[list elementAt:i];
		r = t->r;
		if (NSPointInRect (loc, r) == YES) {
			[self deselect];
			[parent_i setSelectedTexture:i];
			break;
		}
	}

	// [window setEventMask:oldwindowmask];
	return self;
}

@end
