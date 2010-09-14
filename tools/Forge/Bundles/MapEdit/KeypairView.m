
#include "KeypairView.h"
#include "Map.h"
#include "Entity.h"
#include "Things.h"

id          keypairview_i;

@implementation KeypairView
/*
==================
initWithFrame:
==================
*/
- initWithFrame:(NSRect) frameRect
{
	[super initWithFrame:frameRect];
	keypairview_i = self;
	return self;
}


-calcViewSize
{
	NSRect      b;
	NSPoint     pt;
	int         count;
	id          ent;

	ent =[map_i currentEntity];
	count =[ent numPairs];

	//XXX[[self superview] setFlipped: YES];

	b =[[self superview] bounds];
	b.size.height = LINEHEIGHT * count + SPACING;
	[self setBounds:b];
	pt.x = pt.y = 0;
	[self scrollPoint:pt];
	return self;
}

-drawSelf: (const NSRect *) rects:(int) rectCount
{
	epair_t    *pair;
	int         y;

	PSsetgray(NSLightGray);
	PSrectfill (0, 0, _bounds.size.width, _bounds.size.height);

	[[NSFont systemFontOfSize: FONTSIZE] set];
	PSrotate (0);
	PSsetgray (0);

	pair =[[map_i currentEntity] epairs];
	y = _bounds.size.height - LINEHEIGHT;
	for (; pair; pair = pair->next) {
		PSmoveto (SPACING, y);
		PSshow (pair->key);
		PSmoveto (100, y);
		PSshow (pair->value);
		y -= LINEHEIGHT;
	}
	PSstroke ();

	return self;
}

-(void) mouseDown:(NSEvent *) theEvent
{
	NSPoint     loc;
	int         i;
	epair_t    *p;

	loc =[theEvent locationInWindow];
	loc =[self convertPoint: loc fromView:NULL];

	i = (_bounds.size.height - loc.y - 4) / LINEHEIGHT;

	p =[[map_i currentEntity] epairs];
	while (i) {
		p = p->next;
		if (!p)
			return;
		i--;
	}
	if (p)
		[things_i setSelectedKey:p];

	return;
}

@end
