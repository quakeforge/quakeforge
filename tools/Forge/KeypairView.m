
#import "qedefs.h"

id	keypairview_i;

@implementation KeypairView

/*
==================
initFrame:
==================
*/
- (id) initWithFrame: (NSRect) frameRect
{
	[super initWithFrame: frameRect];
	keypairview_i = self;
	return self;
}


- (void) calcViewSize
{
	NSRect		b = [[self superview] bounds];
	NSRect		newFrame;
	NSPoint 	pt;
	id			ent = [map_i currentEntity];
	int			count = [ent numPairs];

//	[[self superview] setFlipped: YES];

	newFrame = b;
	newFrame.size.height = LINEHEIGHT * count + SPACING;

	[[self superview] setNeedsDisplayInRect: newFrame];
	[self setFrame: newFrame];
	[self setNeedsDisplay: YES];
	
	pt.x = pt.y = 0;
	[self scrollPoint: pt];
	return;
}

- (void) drawSelf: (NSRect) aRect
{
	epair_t	*pair;
	int		y;

	PSsetgray (NSLightGray);
	NSRectFill (aRect);
		
	PSselectfont ("Helvetica-Bold", FONTSIZE);
	PSrotate (0);
	PSsetgray (0);
	
	pair = [[map_i currentEntity] epairs];
	y = [self bounds].size.height - LINEHEIGHT;
	for (; pair; pair = pair->next) {
		PSmoveto (SPACING, y);
		PSshow (pair->key);
		PSmoveto (100, y);
		PSshow (pair->value);
		y -= LINEHEIGHT;
	}
	PSstroke ();
	
	return;
}

- (void) mouseDown: (NSEvent *) theEvent
{
	NSPoint loc = [theEvent locationInWindow];
	NSRect	bounds = [self bounds];
	int		i;
	epair_t	*p;

	[self convertPoint: loc	fromView: NULL];
	
	i = (bounds.size.height - loc.y - 4) / LINEHEIGHT;

	p = [[map_i currentEntity] epairs];
	while (	i )	{
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
