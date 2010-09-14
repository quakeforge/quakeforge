#include "QF/sys.h"

#include "XYView.h"
#include "ZView.h"
#include "CameraView.h"
#include "Clipper.h"
#include "QuakeEd.h"
#include "Map.h"
#include "Entity.h"
#include "PopScrollView.h"

id          xyview_i;

id          scrollview_i, gridbutton_i, scalebutton_i;

vec3_t      xy_viewnormal;				// v_forward for xy view
float       xy_viewdist;				// clip behind this plane

@implementation XYView
/*
==================
initWithFrame:
==================
*/
- initWithFrame:(NSRect) frameRect
{
	[super initWithFrame:frameRect];
	[self allocateGState];

	realbounds = NSMakeRect (0, 0, 0, 0);

	gridsize = 16;
	scale = 1.0;
	xyview_i = self;

	xy_viewnormal[2] = -1;
	xy_viewdist = -1024;

//      
// initialize the pop up menus
//
	scalebutton_i =[[NSPopUpButton alloc] init];
	[scalebutton_i setTarget:self];
	[scalebutton_i setAction: @selector (scaleMenuTarget:)];
	[scalebutton_i addItemWithTitle:@"12.5%"];
	[scalebutton_i addItemWithTitle:@"25%"];
	[scalebutton_i addItemWithTitle:@"50%"];
	[scalebutton_i addItemWithTitle:@"75%"];
	[scalebutton_i addItemWithTitle:@"100%"];
	[scalebutton_i addItemWithTitle:@"200%"];
	[scalebutton_i addItemWithTitle:@"300%"];
	[scalebutton_i selectItemAtIndex:4];


	gridbutton_i =[[NSPopUpButton alloc] init];
	[gridbutton_i setTarget:self];
	[gridbutton_i setAction: @selector (gridMenuTarget:)];

	[gridbutton_i addItemWithTitle:@"grid 1"];
	[gridbutton_i addItemWithTitle:@"grid 2"];
	[gridbutton_i addItemWithTitle:@"grid 4"];
	[gridbutton_i addItemWithTitle:@"grid 8"];
	[gridbutton_i addItemWithTitle:@"grid 16"];
	[gridbutton_i addItemWithTitle:@"grid 32"];
	[gridbutton_i addItemWithTitle:@"grid 64"];

	[gridbutton_i selectItemAtIndex:4];

// initialize the scroll view
	scrollview_i =[[PopScrollView alloc]
	initWithFrame: frameRect button1: scalebutton_i button2:gridbutton_i];
	[scrollview_i setLineScroll:64];
	[scrollview_i setAutoresizingMask:(NSViewWidthSizable |
	 NSViewHeightSizable)];

// link objects together
	[scrollview_i setDocumentView:self];
	return scrollview_i;

}

-(BOOL) acceptsFirstResponder
{
	return YES;
}

-setModeRadio:m
{						// this should be set from IB, but
										// because I toss myself in a
										// popscrollview
// the connection gets lost
	mode_radio_i = m;
	[mode_radio_i setTarget:self];
	[mode_radio_i setAction: @selector (drawMode:)];
	return self;
}

-drawMode:sender
{
	drawmode =[sender selectedColumn];
	[quakeed_i updateXY];
	return self;
}

-setDrawMode:(drawmode_t) mode
{
	drawmode = mode;
	[mode_radio_i selectCellAtRow: 0 column:mode];
	[quakeed_i updateXY];
	return self;
}


-(float) currentScale
{
	return scale;
}

/*
	v
*/
-setOrigin:(NSPoint *)
pt          scale:(float) sc
{
	NSRect      sframe;
	NSRect      newbounds;

//
// calculate the area visible in the cliprect
//
	scale = sc;

	sframe =[[self superview] frame];
	newbounds =[[self superview] frame];
	newbounds.origin = *pt;
	newbounds.size.width /= scale;
	newbounds.size.height /= scale;
	sframe.size.width /= scale;
	sframe.size.height /= scale;

//
// union with the realbounds
//
	newbounds = NSUnionRect (realbounds, newbounds);

//
// redisplay everything
//
	// XXX[quakeed_i disableDisplay];

//
// size this view
//
	[self setBoundsSize:newbounds.size];
	[self setBoundsOrigin:newbounds.origin];
	// XXX[self moveTo: newbounds.origin.x : newbounds.origin.y];

//
// scroll and scale the clip view
//
	[[self superview] setBoundsSize:sframe.size];
	[[self superview] setBoundsOrigin:*pt];

	// XXX[quakeed_i reenableDisplay];
	[scrollview_i display];

	return self;
}

-centerOn:(vec3_t) org
{
	NSRect      sbounds;
	NSPoint     mid, delta;

	sbounds =[[xyview_i superview] bounds];

	mid.x = sbounds.origin.x + sbounds.size.width / 2;
	mid.y = sbounds.origin.y + sbounds.size.height / 2;

	delta.x = org[0] - mid.x;
	delta.y = org[1] - mid.y;

	sbounds.origin.x += delta.x;
	sbounds.origin.y += delta.y;

	[self setOrigin: &sbounds.origin scale:scale];
	return self;
}

/*
==================
newSuperBounds

When superview is resized
==================
*/
-newSuperBounds
{
	NSRect      r;

	r =[[self superview] bounds];
	[self newRealBounds:&r];

	return self;
}

/*
===================
newRealBounds

Called when the realbounds rectangle is changed.
Should only change the scroll bars, not cause any redraws.
If realbounds has shrunk, nothing will change.
===================
*/
-newRealBounds:(NSRect *) nb
{
	NSRect      sbounds;

	realbounds = *nb;

//
// calculate the area visible in the cliprect
//
	sbounds =[[self superview] bounds];
	sbounds = NSUnionRect (*nb, sbounds);

//
// size this view
//
	// XXX[quakeed_i disableDisplay];

	[self setPostsBoundsChangedNotifications:NO];
	[self setBoundsSize:sbounds.size];
	[self setBoundsOrigin:sbounds.origin];
	// XXX[self moveTo: sbounds.origin.x : sbounds.origin.y];
	[self setPostsBoundsChangedNotifications:YES];

	[scrollview_i reflectScrolledClipView:[scrollview_i contentView]];
	// XXX[quakeed_i reenableDisplay];

	[[scrollview_i horizontalScroller] display];
	[[scrollview_i verticalScroller] display];

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
	NSRect      visrect, sframe;
	float       nscale;

	item =[[[sender selectedCell] title] cString];
	sscanf (item, "%f", &nscale);
	nscale /= 100;

	if (nscale == scale)
		return NULL;

// keep the center of the view constant
	visrect =[[self superview] bounds];
	sframe =[[self superview] frame];
	visrect.origin.x += visrect.size.width / 2;
	visrect.origin.y += visrect.size.height / 2;

	visrect.origin.x -= sframe.size.width / 2 / nscale;
	visrect.origin.y -= sframe.size.height / 2 / nscale;

	[self setOrigin: &visrect.origin scale:nscale];

	return self;
}

/*
==============
zoomIn
==============
*/
-zoomIn:(NSPoint *) constant
{
	id          itemlist, selectedItem;
	int         selected, numrows;

	NSRect      visrect;
	NSPoint     ofs, new;

//
// set the popup
//
	itemlist =[scalebutton_i itemArray];
	numrows =[itemlist count];

	selectedItem =[scalebutton_i selectedItem];
	selected =[itemlist indexOfObject:selectedItem] + 1;
	if (selected >= numrows)
		return NULL;

	[scalebutton_i selectItemAtIndex:selected];

//
// zoom the view
//
	visrect =[[self superview] bounds];
	ofs.x = constant->x - visrect.origin.x;
	ofs.y = constant->y - visrect.origin.y;

	new.x = constant->x - ofs.x / 2;
	new.y = constant->y - ofs.y / 2;

	[self setOrigin: &new scale:scale * 2];

	return self;
}


/*
==============
zoomOut
==============
*/
-zoomOut:(NSPoint *) constant
{
	id          itemlist, selectedItem;
	int         selected;

	NSRect      visrect;
	NSPoint     ofs, new;

//
// set the popup
//
	itemlist =[scalebutton_i itemArray];

	selectedItem =[scalebutton_i selectedItem];
	selected =[itemlist indexOfObject:selectedItem] - 1;
	if (selected < 0)
		return NULL;

	[scalebutton_i selectItemAtIndex:selected];

//
// zoom the view
//
	visrect =[[self superview] bounds];
	ofs.x = constant->x - visrect.origin.x;
	ofs.y = constant->y - visrect.origin.y;

	new.x = constant->x - ofs.x * 2;
	new.y = constant->y - ofs.y * 2;

	[self setOrigin: &new scale:scale / 2];

	return self;
}


/*
====================
gridMenuTarget:

Called when the scaler popup on the window is used
====================
*/

-gridMenuTarget:sender
{
	char const *item;
	int         grid;

	item =[[[sender selectedCell] title] cString];
	sscanf (item, "grid %d", &grid);

	if (grid == gridsize)
		return NULL;

	gridsize = grid;
	[quakeed_i updateAll];

	return self;
}


/*
====================
snapToGrid
====================
*/
-(float) snapToGrid:(float) f
{
	int         i;

	i = rint (f / gridsize);

	return i * gridsize;
}

-(int) gridsize
{
	return gridsize;
}



/*
===================
addToScrollRange::
===================
*/
-addToScrollRange: (float) x:(float) y;
{
	if (x < newrect.origin.x) {
		newrect.size.width += newrect.origin.x - x;
		newrect.origin.x = x;
	}

	if (y < newrect.origin.y) {
		newrect.size.height += newrect.origin.y - y;
		newrect.origin.y = y;
	}

	if (x > newrect.origin.x + newrect.size.width)
		newrect.size.width += x - (newrect.origin.x + newrect.size.width);

	if (y > newrect.origin.y + newrect.size.height)
		newrect.size.height += y - (newrect.origin.y + newrect.size.height);

	return self;
}

/*
===================
superviewChanged
===================
*/
-superviewChanged
{
	[self newRealBounds:&realbounds];

	return self;
}


/*
===============================================================================

						DRAWING METHODS

===============================================================================
*/

vec3_t      cur_linecolor;
NSBezierPath *path;

void
linestart (float r, float g, float b)
{
	[path removeAllPoints];
	VectorSet (r, g, b, cur_linecolor);
}

void
lineflush (void)
{
	if ([path isEmpty])
		return;
	// endUserPath (upath, dps_ustroke);
	PSsetrgbcolor (cur_linecolor[0], cur_linecolor[1], cur_linecolor[2]);
	[path stroke];
	[path removeAllPoints];
}

void
linecolor (float r, float g, float b)
{
	if (cur_linecolor[0] == r && cur_linecolor[1] == g && cur_linecolor[2] == b)
		return;							// do nothing
	lineflush ();
	VectorSet (r, g, b, cur_linecolor);
}

void
XYmoveto (vec3_t pt)
{
	NSPoint     point = { pt[0], pt[1] };
	if ([path elementCount] > 2048)
		lineflush ();
	[path moveToPoint:point];
}

void
XYlineto (vec3_t pt)
{
	NSPoint     point = { pt[0], pt[1] };
	[path lineToPoint:point];
}

/*
============
drawGrid

Draws tile markings every 64 units, and grid markings at the grid scale if
the grid lines are greater than or equal to 4 pixels apart

Rect is in global world (unscaled) coordinates
============
*/

-drawGrid:(NSRect) rect
{
	int         x, y, stopx, stopy;
	float       top, bottom, right, left;
	char        text[10];
	BOOL        showcoords;
	NSPoint     point;

	showcoords =[quakeed_i showCoordinates];

	left = rect.origin.x - 1;
	bottom = rect.origin.y - 1;
	right = rect.origin.x + rect.size.width + 2;
	top = rect.origin.y + rect.size.height + 2;

	PSsetlinewidth (0.15);

//
// grid
//
// can't just divide by grid size because of negetive coordinate
// truncating direction
//
	if (gridsize >= 4 / scale) {
		y = floor (bottom / gridsize);
		stopy = floor (top / gridsize);
		x = floor (left / gridsize);
		stopx = floor (right / gridsize);

		y *= gridsize;
		stopy *= gridsize;
		x *= gridsize;
		stopx *= gridsize;
		if (y < bottom)
			y += gridsize;
		if (x < left)
			x += gridsize;
		if (stopx >= right)
			stopx -= gridsize;
		if (stopy >= top)
			stopy -= gridsize;

		[path removeAllPoints];

		for (; y <= stopy; y += gridsize)
			if (y & 63) {
				point.x = left;
				point.y = y;
				[path moveToPoint:point];
				point.x = right;
				[path lineToPoint:point];
			}

		for (; x <= stopx; x += gridsize)
			if (x & 63) {
				point.x = x;
				point.y = top;
				[path moveToPoint:point];
				point.y = bottom;
				[path lineToPoint:point];
			}
		// endUserPath (upath, dps_ustroke);
		PSsetrgbcolor (0.8, 0.8, 1.0);	// thin grid color
		[path stroke];

	}
//
// tiles
//
	PSsetgray (0);						// for text

	if (scale > 4.0 / 64) {
		y = floor (bottom / 64);
		stopy = floor (top / 64);
		x = floor (left / 64);
		stopx = floor (right / 64);

		y *= 64;
		stopy *= 64;
		x *= 64;
		stopx *= 64;
		if (y < bottom)
			y += 64;
		if (x < left)
			x += 64;
		if (stopx >= right)
			stopx -= 64;
		if (stopy >= top)
			stopy -= 64;

		[path removeAllPoints];

		for (; y <= stopy; y += 64) {
			if (showcoords) {
				sprintf (text, "%i", y);
				PSmoveto (left, y);
				PSshow (text);
			}
			[path moveToPoint:point];
			point.x = right;
			[path lineToPoint:point];
		}

		for (; x <= stopx; x += 64) {
			if (showcoords) {
				sprintf (text, "%i", x);
				PSmoveto (x, bottom + 2);
				PSshow (text);
			}
			point.x = x;
			point.y = top;
			[path moveToPoint:point];
			point.y = bottom;
			[path lineToPoint:point];
		}

		// endUserPath (upath, dps_ustroke);
		PSsetgray (12.0 / 16);
		[path stroke];
	}

	return self;
}

/*
==================
drawWire
==================
*/
-drawWire:(NSRect) rects
{
	NSRect      visRect;
	int         i, j, c, c2;
	id          ent, brush;
	vec3_t      mins, maxs;
	BOOL        drawnames;

	drawnames =[quakeed_i showNames];

	if ([quakeed_i showCoordinates])	// if coords are showing, update
										// everything
	{
		visRect =[self visibleRect];
		rects = visRect;
		xy_draw_rect = rects;
	}


	NSRectClip (rects);

// erase window
	NSEraseRect (rects);

// draw grid
	[self drawGrid:rects];

// draw all entities, world first so entities take priority
	linestart (0, 0, 0);

	c =[map_i count];
	for (i = 0; i < c; i++) {
		ent =[map_i objectAtIndex:i];
		c2 =[ent count];
		for (j = c2 - 1; j >= 0; j--) {
			brush =[ent objectAtIndex:j];
			if ([brush selected])
				continue;
			if ([brush regioned])
				continue;
			[brush XYDrawSelf];
		}
		if (i > 0 && drawnames) {		// draw entity names
			brush =[ent objectAtIndex:0];
			if (![brush regioned]) {
				[brush getMins: mins maxs:maxs];
				PSmoveto (mins[0], mins[1]);
				PSsetrgbcolor (0, 0, 0);
				PSshow ([ent valueForQKey:"classname"]);
			}
		}
	}

	lineflush ();

// resize if needed
	newrect.origin.x -= gridsize;
	newrect.origin.y -= gridsize;
	newrect.size.width += 2 * gridsize;
	newrect.size.height += 2 * gridsize;
	if (!NSEqualRects (newrect, realbounds))
		[self newRealBounds:&newrect];

	return self;
}


/*
=============
drawSolid
=============
*/
-drawSolid
{
	const unsigned char *planes[5];
	NSRect      visRect;

	visRect =[self visibleRect];

//
// draw the image into imagebuffer
//
	r_origin[0] = visRect.origin.x;
	r_origin[1] = visRect.origin.y;

	r_origin[2] = scale / 2;			// using Z as a scale for the 2D
										// projection

	r_width = visRect.size.width * r_origin[2];
	r_height = visRect.size.height * r_origin[2];

	if (r_width != xywidth || r_height != xyheight) {
		xywidth = r_width;
		xyheight = r_height;

		if (xypicbuffer) {
			free (xypicbuffer);
			free (xyzbuffer);
		}
		xypicbuffer = malloc (r_width * (r_height + 1) * 4);
		xyzbuffer = malloc (r_width * (r_height + 1) * 4);
	}

	r_picbuffer = xypicbuffer;
	r_zbuffer = xyzbuffer;

	REN_BeginXY ();
	REN_ClearBuffers ();

//
// render the entities
//
	[map_i makeAllPerform:@selector (XYRenderSelf)];

//
// display the output
//
	[self lockFocus];
	[[self window] setBackingType:NSBackingStoreRetained];

	planes[0] = (unsigned char *) r_picbuffer;
	NSDrawBitmap (visRect,
					r_width,
					r_height,
					8,
					3,
					32, r_width * 4, NO, NO, NSCalibratedRGBColorSpace, planes);

	// NSPing ();
	[[self window] setBackingType:NSBackingStoreBuffered];
	[self unlockFocus];

	return self;
}

/*
===================
drawSelf
===================
*/
NSRect      xy_draw_rect;

-drawRect: (NSRect) rects
{
	static float drawtime;				// static to shut up compiler warning

	if (timedrawing)
		drawtime = Sys_DoubleTime ();

	xy_draw_rect = rects;
	newrect.origin.x = newrect.origin.y = 99999;
	newrect.size.width = newrect.size.height = -2 * 99999;

// setup for text
	// PSselectfont("Helvetica-Medium",10/scale);
	[[NSFont systemFontOfSize: 10 / scale] set];
	PSrotate (0);

	if (drawmode == dr_texture || drawmode == dr_flat)
		[self drawSolid];
	else
		[self drawWire:rects];

	if (timedrawing) {
		// NSPing ();
		drawtime = Sys_DoubleTime () - drawtime;
		printf ("CameraView drawtime: %5.3f\n", drawtime);
	}

	return self;
}



/*
===============================================================================

						USER INTERACTION

===============================================================================
*/

/*
================
dragLoop:
================
*/
static NSPoint oldreletive;

-dragFrom: (NSEvent *) startevent useGrid: (BOOL) ug callback:(void (*)(float dx, float dy)) callback
{
	NSEvent    *event;
	NSPoint     startpt, newpt;
	NSPoint     reletive, delta;

	startpt =[startevent locationInWindow];
	startpt =[self convertPoint: startpt fromView:NULL];

	oldreletive.x = oldreletive.y = 0;

	if (ug) {
		startpt.x =[self snapToGrid:startpt.x];
		startpt.y =[self snapToGrid:startpt.y];
	}

	while (1) {
		unsigned    eventMask = NSLeftMouseUpMask | NSLeftMouseDraggedMask
			| NSRightMouseUpMask | NSRightMouseDraggedMask
			| NSApplicationDefinedMask;
		event =[NSApp nextEventMatchingMask: eventMask untilDate:[NSDate
		 distantFuture]
		inMode: NSEventTrackingRunLoopMode dequeue:YES];


		if ([event type] == NSLeftMouseUp ||[event type] == NSRightMouseUp)
			break;
		if ([event type] == NSApplicationDefined) {	// doesn't work.  grrr.
			// [quakeed_i applicationDefined:event];
			continue;
		}

		newpt =[event locationInWindow];
		newpt =[self convertPoint: newpt fromView:NULL];

		if (ug) {
			newpt.x =[self snapToGrid:newpt.x];
			newpt.y =[self snapToGrid:newpt.y];
		}

		reletive.x = newpt.x - startpt.x;
		reletive.y = newpt.y - startpt.y;
		if (reletive.x == oldreletive.x && reletive.y == oldreletive.y)
			continue;

		delta.x = reletive.x - oldreletive.x;
		delta.y = reletive.y - oldreletive.y;
		oldreletive = reletive;

		callback (delta.x, delta.y);

	}

	return self;
}

//============================================================================


void
DragCallback (float dx, float dy)
{
	sb_translate[0] = dx;
	sb_translate[1] = dy;
	sb_translate[2] = 0;

	[map_i makeSelectedPerform:@selector (translate)];

	[quakeed_i redrawInstance];
}

-selectionDragFrom:(NSEvent *) theEvent
{
	Sys_Printf ("dragging selection\n");
	[self dragFrom: theEvent useGrid: YES callback:DragCallback];
	[quakeed_i updateAll];
	return self;

}

//============================================================================

void
ScrollCallback (float dx, float dy)
{
	NSRect      basebounds;
	NSPoint     neworg;
	float       scale;

	basebounds =[[xyview_i superview] bounds];
	[xyview_i convertRect: basebounds fromView:[xyview_i superview]];

	neworg.x = basebounds.origin.x - dx;
	neworg.y = basebounds.origin.y - dy;

	scale =[xyview_i currentScale];

	oldreletive.x -= dx;
	oldreletive.y -= dy;
	[xyview_i setOrigin: &neworg scale:scale];
}

-scrollDragFrom:(NSEvent *) theEvent
{
	Sys_Printf ("scrolling view\n");
	[self dragFrom: theEvent useGrid: YES callback:ScrollCallback];
	return self;

}

//============================================================================

vec3_t      direction;

void
DirectionCallback (float dx, float dy)
{
	vec3_t      org;
	float       ya;

	direction[0] += dx;
	direction[1] += dy;

	[cameraview_i getOrigin:org];

	if (direction[0] == org[0] && direction[1] == org[1])
		return;

	ya = atan2 (direction[1] - org[1], direction[0] - org[0]);

	[cameraview_i setOrigin: org angle:ya];
	[quakeed_i newinstance];
	[cameraview_i display];
}

-directionDragFrom:(NSEvent *) theEvent
{
	NSPoint     pt;

	Sys_Printf ("changing camera direction\n");

	pt =[theEvent locationInWindow];
	pt =[self convertPoint: pt fromView:NULL];

	direction[0] = pt.x;
	direction[1] = pt.y;

	DirectionCallback (0, 0);

	[self dragFrom: theEvent useGrid: NO callback:DirectionCallback];
	return self;
}

//============================================================================

id          newbrush;
vec3_t      neworg, newdrag;

void
NewCallback (float dx, float dy)
{
	vec3_t      min, max;
	int         i;

	newdrag[0] += dx;
	newdrag[1] += dy;

	for (i = 0; i < 3; i++) {
		if (neworg[i] < newdrag[i]) {
			min[i] = neworg[i];
			max[i] = newdrag[i];
		} else {
			min[i] = newdrag[i];
			max[i] = neworg[i];
		}
	}

	[newbrush setMins: min maxs:max];

	[quakeed_i redrawInstance];
}

-newBrushDragFrom:(NSEvent *) theEvent
{
	id          owner;
	texturedef_t td;
	NSPoint     pt;

	Sys_Printf ("sizing new brush\n");

	pt =[theEvent locationInWindow];
	pt =[self convertPoint: pt fromView:NULL];

	neworg[0] =[self snapToGrid:pt.x];
	neworg[1] =[self snapToGrid:pt.y];
	neworg[2] =[map_i currentMinZ];

	newdrag[0] = neworg[0];
	newdrag[1] = neworg[1];
	newdrag[2] =[map_i currentMaxZ];

	owner =[map_i currentEntity];

	[texturepalette_i getTextureDef:&td];

	newbrush =[[SetBrush alloc] initOwner: owner mins: neworg maxs: newdrag texture:&td];
	[owner addObject:newbrush];

	[newbrush setSelected:YES];

	[self dragFrom: theEvent useGrid: YES callback:NewCallback];

	[newbrush removeIfInvalid];

	[quakeed_i updateCamera];
	return self;

}

//============================================================================

void
ControlCallback (float dx, float dy)
{
	int         i;

	for (i = 0; i < numcontrolpoints; i++) {
		controlpoints[i][0] += dx;
		controlpoints[i][1] += dy;
	}

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

	dragpoint[0] = pt.x;
	dragpoint[1] = pt.y;
	dragpoint[2] = 2048;

	[[map_i selectedBrush] getXYdragface:dragpoint];
	if (!numcontrolpoints)
		return NO;

	Sys_Printf ("dragging brush plane\n");

	pt =[theEvent locationInWindow];
	pt =[self convertPoint: pt fromView:NULL];

	[self dragFrom: theEvent useGrid: YES callback:ControlCallback];

	[[map_i selectedBrush] removeIfInvalid];

	[quakeed_i updateAll];

	return YES;
}

-(BOOL) shearDragFrom:(NSEvent *) theEvent
{
	NSPoint     pt;
	vec3_t      dragpoint;
	vec3_t      p1, p2;
	float       time;
	id          br;
	int         face;

	if ([map_i numSelected] != 1)
		return NO;
	br =[map_i selectedBrush];

	pt =[theEvent locationInWindow];
	pt =[self convertPoint: pt fromView:NULL];

// if the XY point is inside the brush, make the point on top
	p1[0] = pt.x;
	p1[1] = pt.y;
	VectorCopy (p1, p2);

	p1[2] = -2048 * xy_viewnormal[2];
	p2[2] = 2048 * xy_viewnormal[2];

	VectorCopy (p1, dragpoint);
	[br hitByRay: p1: p2: &time:&face];

	if (time > 0) {
		dragpoint[2] = p1[2] + (time - 0.01) * xy_viewnormal[2];
	} else {
		[br getMins: p1 maxs:p2];
		dragpoint[2] = (p1[2] + p2[2]) / 2;
	}


	[br getXYShearPoints:dragpoint];
	if (!numcontrolpoints)
		return NO;

	Sys_Printf ("dragging brush plane\n");

	pt =[theEvent locationInWindow];
	pt =[self convertPoint: pt fromView:NULL];

	[self dragFrom: theEvent useGrid: YES callback:ControlCallback];

	[br removeIfInvalid];

	[quakeed_i updateAll];
	return YES;
}


/*
===============================================================================

						INPUT METHODS

===============================================================================
*/


/*
===================
mouseDown
===================
*/
-mouseDown:(NSEvent *) theEvent
{
	NSPoint     pt;
	id          ent;
	vec3_t      p1, p2;
	int         flags;

	pt =[theEvent locationInWindow];
	pt =[self convertPoint: pt fromView:NULL];

	p1[0] = p2[0] = pt.x;
	p1[1] = p2[1] = pt.y;
	p1[2] = xy_viewnormal[2] * -4096;
	p2[2] = xy_viewnormal[2] * 4096;

	flags =
		[theEvent modifierFlags] & (NSShiftKeyMask | NSControlKeyMask |
									NSAlternateKeyMask | NSCommandKeyMask);

//
// shift click to select / deselect a brush from the world
//
	if (flags == NSShiftKeyMask) {
		[map_i selectRay: p1: p2:YES];
		return self;
	}
//
// cmd-shift click to set a target/targetname entity connection
//
	if (flags == (NSShiftKeyMask | NSCommandKeyMask)) {
		[map_i entityConnect: p1:p2];
		return self;
	}
//
// bare click to either drag selection, or rubber band a new brush
//
	if (flags == 0) {
		// if double click, position Z checker
		if ([theEvent clickCount] > 1) {
			Sys_Printf ("positioned Z checker\n");
			[zview_i setPoint:&pt];
			[quakeed_i newinstance];
			[quakeed_i updateZ];
			return self;
		}
		// check eye
		if ([cameraview_i XYmouseDown: &pt flags:[theEvent
		 modifierFlags]])
			return self;				// camera move

		// check z post
		if ([zview_i XYmouseDown:&pt])
			return self;				// z view move

		// check clippers
		if ([clipper_i XYDrag:&pt])
			return self;

		// check single plane dragging
		if ([self planeDragFrom:theEvent])
			return self;

		// check selection
		ent =[map_i grabRay: p1:p2];
		if (ent)
			return[self selectionDragFrom:theEvent];

		if ([map_i numSelected]) {
			Sys_Printf ("missed\n");
			return self;
		}

		return[self newBrushDragFrom:theEvent];
	}
//
// control click = position and drag camera 
//
	if (flags == NSControlKeyMask) {
		[cameraview_i setXYOrigin:&pt];
		[quakeed_i newinstance];
		[cameraview_i display];
		[cameraview_i XYmouseDown: &pt flags:[theEvent modifierFlags]];
		return self;
	}
//
// command click = drag Z checker
//
	if (flags == NSCommandKeyMask) {
// check single plane dragging
		[self shearDragFrom:theEvent];
		return self;

		Sys_Printf ("moving Z checker\n");
		[zview_i setXYOrigin:&pt];
		[quakeed_i updateAll];
		[zview_i XYmouseDown:&pt];
		return self;
	}
//
// alt click = set entire brush texture
//
	if (flags == NSAlternateKeyMask) {
		if (drawmode != dr_texture) {
			Sys_Printf ("No texture setting except in texture mode!\n");
			NopSound ();
			return self;
		}
		[map_i setTextureRay: p1: p2:YES];
		[quakeed_i updateAll];
		return self;
	}
//
// ctrl-alt click = set single face texture
//
	if (flags == (NSControlKeyMask | NSAlternateKeyMask)) {
		if (drawmode != dr_texture) {
			Sys_Printf ("No texture setting except in texture mode!\n");
			NopSound ();
			return self;
		}
		[map_i setTextureRay: p1: p2:NO];
		[quakeed_i updateAll];
		return self;
	}

	Sys_Printf ("bad flags for click %x\n", flags);
	NopSound ();
	return self;
}

/*
===================
rightMouseDown
===================
*/
-rightMouseDown:(NSEvent *) theEvent
{
	NSPoint     pt;
	int         flags;

	pt =[theEvent locationInWindow];
	pt =[self convertPoint: pt fromView:NULL];

	flags =
		[theEvent modifierFlags] & (NSShiftKeyMask | NSControlKeyMask |
									NSAlternateKeyMask | NSCommandKeyMask);

	if (flags == NSCommandKeyMask) {
		return[self scrollDragFrom:theEvent];
	}

	if (flags == NSAlternateKeyMask) {
		return[clipper_i XYClick:pt];
	}

	if (flags == 0 || flags == NSControlKeyMask) {
		return[self directionDragFrom:theEvent];
	}

	Sys_Printf ("bad flags for click\n");
	NopSound ();

	return self;
}


@end
