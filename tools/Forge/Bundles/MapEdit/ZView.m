#include "QF/sys.h"

#include "ZView.h"
#include "ZScrollView.h"
#include "QuakeEd.h"
#include "Map.h"
#include "XYView.h"
#include "CameraView.h"

id          zview_i;

id          zscrollview_i, zscalemenu_i, zscalebutton_i;

float       zplane;
float       zplanedir;

extern NSBezierPath *path;

@implementation ZView
/*
==================
initWithFrame:
==================
*/
- (id) initWithFrame:(NSRect) frameRect
{
	NSPoint     pt;
	NSBezierPath *path;

	path = checker = [NSBezierPath new];
	[path setLineWidth: 0.3];
	[path moveToPoint: NSMakePoint (-16, -16)];
	[path relativeLineToPoint: NSMakePoint (32, 32)];
	[path moveToPoint: NSMakePoint (-16, 16)];
	[path relativeLineToPoint: NSMakePoint (32, -32)];

	origin[0] = 0.333;
	origin[1] = 0.333;

	[super initWithFrame:frameRect];
	[self allocateGState];
	[self clearBounds];

	zview_i = self;
	scale = 1;

//      
// initialize the pop up menus
//
	zscalebutton_i =[[NSPopUpButton alloc] init];
	[zscalebutton_i setTarget:self];
	[zscalebutton_i setAction: @selector (scaleMenuTarget:)];

	[zscalebutton_i addItemWithTitle:@"12.5%"];
	[zscalebutton_i addItemWithTitle:@"25%"];
	[zscalebutton_i addItemWithTitle:@"50%"];
	[zscalebutton_i addItemWithTitle:@"75%"];
	[zscalebutton_i addItemWithTitle:@"100%"];
	[zscalebutton_i addItemWithTitle:@"200%"];
	[zscalebutton_i addItemWithTitle:@"300%"];
	[zscalebutton_i selectItemAtIndex:4];

// initialize the scroll view
	zscrollview_i =[[ZScrollView alloc] initWithFrame: frameRect
											  button1: zscalebutton_i];
	[zscrollview_i setAutoresizingMask:NSViewWidthSizable |
									   NSViewHeightSizable];

	[zscrollview_i setDocumentView:self];

	[_super_view setBoundsOrigin: NSMakePoint (0, 0)];

	minheight = 0;
	maxheight = 64;

	pt.x = -[_super_view bounds].size.width / 2;
	pt.y = -128;

	[self newRealBounds];

	[self setOrigin: pt scale:1];

	return zscrollview_i;
}

-setXYOrigin:(NSPoint *) pt
{
	origin[0] = pt->x + 0.333;
	origin[1] = pt->y + 0.333;
	return self;
}

-(float) currentScale
{
	return scale;
}

/*
===================
setOrigin:scale:
===================
*/
-setOrigin:(NSPoint) pt scale:(float) sc
{
	NSRect      sframe;
	NSRect      bounds;
	NSRect      size;

	// calculate the area visible in the cliprect
	scale = sc;

	bounds =[_super_view bounds];
	bounds.origin = pt;
	bounds.size.width /= 1;
	bounds.size.height /= scale;

	size = NSMakeRect (0, oldminheight, 1, oldmaxheight - oldminheight);
	// union with the realbounds
	bounds = NSUnionRect (size, bounds);
	// redisplay everything
	bounds.origin = NSMakePoint (-bounds.size.width/2, bounds.origin.y);
	sframe = bounds;
	sframe.origin.x *= 1;
	sframe.origin.y *= scale;
	sframe.size.width *= 1;
	sframe.size.height *= scale;

	// size this view
	[quakeed_i disableFlushWindow];
	[self setFrame:sframe bounds:bounds scale:NSMakeSize (1, scale)];

	// scroll and scale the clip view
	pt.x *= 1;
	pt.y *= scale;
	[_super_view setBoundsOrigin: pt];

	[quakeed_i enableFlushWindow];
	[zscrollview_i display];

	return self;
}


/*
====================
scaleMenuTarget:

Called when the scaler popup on the window is used
====================
*/
-scaleMenuTarget:sender
{
	char const *item;
	NSRect      rect;
	NSPoint     mid, org, orig;
	float       nscale;

	item =[[sender titleOfSelectedItem] cString];
	sscanf (item, "%f", &nscale);
	nscale /= 100;

	if (nscale == scale)
		return NULL;

// keep the center of the view constant
	rect =[_super_view bounds];
	mid.x = rect.size.width / 2;
	mid.y = rect.size.height / 2;

	org.x = (rect.origin.x + mid.x) / 1;
	org.y = (rect.origin.y + mid.y) / scale;

	orig.x = org.x - mid.x / 1;
	orig.y = org.y - mid.y / nscale;

	[self setOrigin: orig scale:nscale];

	return self;
}


-clearBounds
{
	topbound = 999999;
	bottombound = -999999;

	return self;
}

-getBounds: (float *) top:(float *) bottom;
{
	*top = topbound;
	*bottom = bottombound;
	return self;
}


/*
==================
addToHeightRange:
==================
*/
-addToHeightRange:(float) height
{
	if (height < minheight)
		minheight = height;
	if (height > maxheight)
		maxheight = height;
	return self;
}


/*
==================
newSuperBounds

When _super_view is resized
==================
*/
-newSuperBounds
{
	oldminheight++;
	[self newRealBounds];

	return self;
}


/*
===================
newRealBounds

Should only change the scroll bars, not cause any redraws.
If realbounds has shrunk, nothing will change.
===================
*/
-newRealBounds
{
	NSRect      bounds;
	NSRect      sframe;
	NSRect      size;

	NSClipView *cv = (NSClipView *) _super_view;

	if (minheight == oldminheight && maxheight == oldmaxheight)
		return self;

	oldminheight = minheight;
	oldmaxheight = maxheight;

	minheight -= 16;
	maxheight += 16;

	// calculate the area visible in the cliprect
	bounds =[cv bounds];
	bounds.size.width /= 1;
	bounds.size.height /= scale;

	size = NSMakeRect (0, minheight, 1, maxheight - minheight);

	bounds = NSUnionRect (size, bounds);

	sframe = bounds;
	sframe.size.width *= 1;
	sframe.size.height *= scale;

	// size this view
	[quakeed_i disableFlushWindow];
	[self setFrame:sframe bounds:bounds scale:NSMakeSize (1, scale)];

	[[_super_view superview] reflectScrolledClipView: cv];

	[quakeed_i enableFlushWindow];
	[[zscrollview_i verticalScroller] display];

	return self;
}



/*
============
drawGrid

Draws tile markings every 64 units, and grid markings at the grid scale if
the grid lines are >= 4 pixels apart

Rect is in global world (unscaled) coordinates
============
*/

-drawGrid:(NSRect) rect
{
	int         y, stopy;
	float       top, bottom;
	int         left, right;
	int         gridsize;
	BOOL        showcoords;
	NSMutableDictionary *attribs = [NSMutableDictionary dictionary];

	showcoords =[quakeed_i showCoordinates];

	gridsize =[xyview_i gridsize];

	left = _bounds.origin.x;
	right = 24;
	bottom = rect.origin.y - 1;
	top = rect.origin.y + rect.size.height + 2;
//
// grid
//
// can't just divide by grid size because of negetive coordinate
// truncating direction
//
	if (gridsize >= 4 / scale) {
		y = floor (bottom / gridsize);
		stopy = floor (top / gridsize);

		y *= gridsize;
		stopy *= gridsize;
		if (y < bottom)
			y += gridsize;

		[path removeAllPoints];

		for (; y <= stopy; y += gridsize)
			if (y & 31) {
				[path moveToPoint:NSMakePoint (left, y)];
				[path lineToPoint:NSMakePoint (right, y)];
			}
		// endUserPath (upath, dps_ustroke);
		[[NSColor colorWithCalibratedRed: 0.8 green: 0.8 blue: 1.0 alpha: 1.0]
		 set];	// thin grid color
		[path stroke];
	}
//
// half tiles
//
	y = floor (bottom / 32);
	stopy = floor (top / 32);

	if (!(((int) y + 4096) & 1))
		y++;
	y *= 32;
	stopy *= 32;
	if (stopy >= top)
		stopy -= 32;

	[path removeAllPoints];

	for (; y <= stopy; y += 64) {
		[path moveToPoint:NSMakePoint (left, y)];
		[path lineToPoint:NSMakePoint (right, y)];
	}

	// endUserPath (upath, dps_ustroke);
	[[NSColor colorWithCalibratedWhite: 12.0 / 16.0 alpha: 1.0]
	 set];
	[path stroke];

//
// tiles
//
	y = floor (bottom / 64);
	stopy = floor (top / 64);

	y *= 64;
	stopy *= 64;
	if (y < bottom)
		y += 64;
	if (stopy >= top)
		stopy -= 64;

	[path removeAllPoints];
	[[NSColor colorWithCalibratedWhite: 0.0 / 16.0 alpha: 1.0]
	 set];						// for text
	[[NSFont systemFontOfSize: 10] set];

	for (; y <= stopy; y += 64) {
		if (showcoords) {
			NSString *s = [NSString stringWithFormat: @"%i", y];
			[s drawAtPoint: NSMakePoint (left, y) withAttributes: attribs];
		}
		[path moveToPoint:NSMakePoint (left + 24, y)];
		[path lineToPoint:NSMakePoint (right, y)];
	}

// divider
	[path moveToPoint:NSMakePoint (0, _bounds.origin.y)];
	[path lineToPoint:NSMakePoint (0,
				 _bounds.origin.y + _bounds.size.height)];

	// endUserPath (upath, dps_ustroke);
	[[NSColor colorWithCalibratedWhite: 10.0 / 16.0 alpha: 1.0]
	 set];
	[path stroke];

//
// origin
//
	[[NSColor colorWithCalibratedWhite: 4.0 / 16.0 alpha: 1.0]
	 set];
	[path removeAllPoints];
	[path setLineWidth: 5];
	[path moveToPoint: NSMakePoint (right, 0)];
	[path lineToPoint: NSMakePoint (left, 0)];
	[path stroke];
	[path setLineWidth: 1];

	return self;
}


-drawZplane
{
	[[NSColor colorWithCalibratedRed: 0.2 green: 0.2 blue: 0.0 alpha: 1.0]
	 set];
	[path appendBezierPathWithArcWithCenter: NSMakePoint (0, zplane)
									 radius: 4
								 startAngle: 0
								   endAngle: 180];
	[path fill];
	return self;
}

/*
===============================================================================
drawSelf
===============================================================================
*/

-drawRect: (NSRect) rects
{
	//NSRect      visRect;
//Sys_Printf("ZView:drawRect\n");
	minheight = 999999;
	maxheight = -999999;

// allways draw the entire bar  
	//visRect =[self visibleRect];
	rects = [self visibleRect];

// erase window
	NSEraseRect (rects);

// draw grid
	[self drawGrid:rects];

// draw zplane
//	[self drawZplane]; FIXME zplane doesn't do anything yet

// draw all entities
	[map_i makeUnselectedPerform:@selector (ZDrawSelf)];

// possibly resize the view
	[self newRealBounds];

	return self;
}

/*
==============
XYDrawSelf
==============
*/
-XYDrawSelf
{
	NSBezierPath *path;
	NSAffineTransform *trans;

	[[NSColor colorWithCalibratedRed: 0 green: 0.5 blue: 1.0 alpha: 1.0] set];

	trans = [NSAffineTransform transform];
	[trans translateXBy: origin[0] yBy: origin[1]];

	path = [checker copy];
	[path transformUsingAffineTransform: trans];
	[path stroke];
	[path release];

	return self;
}


/*
==============
getPoint: (NSPoint *)pt
==============
*/
-getPoint:(NSPoint *) pt
{
	pt->x = origin[0] + 0.333;			// offset a bit to avoid edge cases
	pt->y = origin[1] + 0.333;
	return self;
}

-setPoint:(NSPoint *) pt
{
	origin[0] = pt->x;
	origin[1] = pt->y;
	return self;
}


/*
==============================================================================

MOUSE CLICKING

==============================================================================
*/


/*
================
dragLoop:
================
*/
static NSPoint oldreletive;

-dragFrom: (NSEvent *) startevent useGrid: (BOOL) ug callback:(void (*)(float dy)) callback
{
	NSEvent    *event;
	NSPoint     startpt, newpt;
	NSPoint     reletive, delta;
	int         gridsize;

	gridsize =[xyview_i gridsize];

	startpt =[startevent locationInWindow];
	startpt =[self convertPoint: startpt fromView:NULL];

	oldreletive.x = oldreletive.y = 0;
	reletive.x = 0;

	while (1) {
		unsigned    eventMask = NSLeftMouseUpMask | NSLeftMouseDraggedMask
			| NSRightMouseUpMask | NSRightMouseDraggedMask
			| NSApplicationDefinedMask;
		event =[NSApp nextEventMatchingMask: eventMask untilDate:[NSDate
		 distantFuture]
		inMode: NSEventTrackingRunLoopMode dequeue:YES];

		if ([event type] == NSLeftMouseUp ||[event type] == NSRightMouseUp)
			break;

		newpt =[event locationInWindow];
		newpt =[self convertPoint: newpt fromView:NULL];

		reletive.y = newpt.y - startpt.y;

		if (ug) {						// we want truncate towards 0 behavior
										// here
			reletive.y = gridsize * (int) (reletive.y / gridsize);
		}

		if (reletive.y == oldreletive.y)
			continue;

		delta.y = reletive.y - oldreletive.y;
		oldreletive = reletive;
		callback (delta.y);
	}

	return self;
}

//============================================================================


void
ZDragCallback (float dy)
{
	sb_translate[0] = 0;
	sb_translate[1] = 0;
	sb_translate[2] = dy;

	[map_i makeSelectedPerform:@selector (translate)];

	[quakeed_i redrawInstance];
}

-selectionDragFrom:(NSEvent *) theEvent
{
	Sys_Printf ("dragging selection\n");
	[self dragFrom: theEvent useGrid: YES callback:ZDragCallback];
	[quakeed_i updateCamera];
	return self;

}

//============================================================================

void
ZScrollCallback (float dy)
{
	NSRect      basebounds;
	NSPoint     neworg;
	float       scale;

	basebounds =[[zview_i superview] bounds];
	basebounds =[zview_i convertRect: basebounds fromView:[zview_i
	 superview]];

	neworg.y = basebounds.origin.y - dy;

	scale =[zview_i currentScale];

	oldreletive.y -= dy;
	[zview_i setOrigin: neworg scale:scale];
}

-scrollDragFrom:(NSEvent *) theEvent
{
	Sys_Printf ("scrolling view\n");
	[self dragFrom: theEvent useGrid: YES callback:ZScrollCallback];
	return self;
}

//============================================================================

void
ZControlCallback (float dy)
{
	int         i;

	for (i = 0; i < numcontrolpoints; i++)
		controlpoints[i][2] += dy;

	[[map_i selectedBrush] calcWindings];
	[quakeed_i redrawInstance];
}

-(BOOL) planeDragFrom:(NSEvent *) theEvent
{
	NSPoint     pt;
	vec3_t      dragpoint;

	if ([map_i numSelected] != 1)
		return NO;

	pt =[theEvent locationInWindow];
	pt =[self convertPoint: pt fromView:NULL];

	dragpoint[0] = origin[0];
	dragpoint[1] = origin[1];
	dragpoint[2] = pt.y;

	[[map_i selectedBrush] getZdragface:dragpoint];
	if (!numcontrolpoints)
		return NO;

	Sys_Printf ("dragging brush plane\n");

	pt =[theEvent locationInWindow];
	pt =[self convertPoint: pt fromView:NULL];

	[self dragFrom: theEvent useGrid: YES callback:ZControlCallback];

	[[map_i selectedBrush] removeIfInvalid];

	[quakeed_i updateCamera];
	return YES;
}


//============================================================================

/*
===================
mouseDown
===================
*/
-(void) mouseDown:(NSEvent *) theEvent
{
	NSPoint     pt;
	int         flags;
	vec3_t      p1;

	pt =[theEvent locationInWindow];
	pt =[self convertPoint: pt fromView:NULL];

	p1[0] = origin[0];
	p1[1] = origin[1];
	p1[2] = pt.y;

	flags =
		[theEvent modifierFlags] & (NSShiftKeyMask | NSControlKeyMask |
									NSAlternateKeyMask | NSCommandKeyMask);

//
// shift click to select / deselect a brush from the world
//
	if (flags == NSShiftKeyMask) {
		[map_i selectRay: p1: p1:NO];
		return;
	}
//
// alt click = set entire brush texture
//
	if (flags == NSAlternateKeyMask) {
		[map_i setTextureRay: p1: p1:YES];
		return;
	}
//
// control click = position view
//
	if (flags == NSControlKeyMask) {
		[cameraview_i setZOrigin:pt.y];
		[quakeed_i updateAll];
		[cameraview_i ZmouseDown: &pt flags:[theEvent modifierFlags]];
		return;
	}
//
// bare click to drag icons or new brush drag
//
	if (flags == 0) {
// check eye
		if ([cameraview_i ZmouseDown: &pt flags:[theEvent modifierFlags]])
			return;

		if ([map_i numSelected]) {
			if (pt.x > 0) {
				if ([self planeDragFrom:theEvent])
					return;
			}
			[self selectionDragFrom:theEvent];
			return;
		}

	}

	Sys_Printf ("bad flags for click %x %g %g\n", flags, pt.x, pt.y);
	NopSound ();
	return;
}

/*
===================
rightMouseDown
===================
*/
-(void) rightMouseDown:(NSEvent *) theEvent
{
	NSPoint     pt;
	int         flags;

	pt =[theEvent locationInWindow];
	pt =[self convertPoint: pt fromView:NULL];

	flags =
		[theEvent modifierFlags] & (NSShiftKeyMask | NSControlKeyMask |
									NSAlternateKeyMask | NSCommandKeyMask);


//
// click = scroll view
//
	if (flags == 0) {
		[self scrollDragFrom:theEvent];
	}

	Sys_Printf ("bad flags for click\n");
	NopSound ();
}


/*
===============================================================================

						XY mouse view methods

===============================================================================
*/

/*
================
modalMoveLoop
================
*/
-modalMoveLoop: (NSPoint *) basept: (vec3_t) movemod:converter
{
	vec3_t      originbase;
	NSEvent    *event;
	NSPoint     newpt;
	vec3_t      delta;

	int         i;

	VectorCopy (origin, originbase);

//
// modal event loop using instance drawing
//
	goto drawentry;

	while ([event type] != NSLeftMouseUp) {
		// 
		// calculate new point
		// 
		newpt =[event locationInWindow];
		newpt =[converter convertPoint: newpt fromView:NULL];

		delta[0] = newpt.x - basept->x;
		delta[1] = newpt.y - basept->y;
		delta[2] = delta[1];			// height change

		for (i = 0; i < 3; i++)
			origin[i] = originbase[i] + movemod[i] * delta[i];


		drawentry:
		// 
		// instance draw new frame
		// 
		[quakeed_i newinstance];
		[self display];

		unsigned    eventMask = NSLeftMouseUpMask | NSLeftMouseDraggedMask;

		event =[NSApp nextEventMatchingMask: eventMask untilDate:[NSDate
		 distantFuture]
		inMode: NSEventTrackingRunLoopMode dequeue:YES];
	}

//
// draw the brush back into the window buffer
//
//  [xyview_i display];

	return self;
}

/*
===============
XYmouseDown
===============
*/
-(BOOL) XYmouseDown:(NSPoint *) pt
{
	vec3_t      movemod;

	if (fabs (pt->x - origin[0]) > 16 || fabs (pt->y - origin[1]) > 16)
		return NO;

	movemod[0] = 1;
	movemod[1] = 1;
	movemod[2] = 0;

	[self modalMoveLoop: pt: movemod:xyview_i];

	return YES;
}

@end
