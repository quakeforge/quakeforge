/*
	CameraView.m

	Camera view object

	Copyright (C) 2001 Jeff Teunissen <deek@d2dc.net>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id$
*/

#include "Config.h"

#import <Foundation/NSGeometry.h>
#import <Foundation/NSObject.h>

#import "CameraView.h"
#import "Forge.h"
#import "Map.h"
#import "UserPath.h"
#import "ZView.h"

#import "render.h"

CameraView	*cameraView;
BOOL		timeDrawing = 0;

@implementation CameraView

/*
	(id) initWithFrame: (NSRect)

	Given a frame, set up our view.
*/
- (id) initWithFrame: (NSRect) frameRect
{
	int		size;
	
	[super initWithFrame: frameRect];
	
	cameraView = self;
	
	angles[0] = angles[1] = angles[2] = 0.0;
	
	[self matrixFromAngles];
	
	origin[0] = 64;
	origin[1] = 64;
	origin[2] = 48;
	
	move = 16;
	
	bounds = frameRect;
	size = bounds.size.width * bounds.size.height;
	zbuffer = malloc (size*4);
	imagebuffer = malloc (size*4);
	
	return self;
}

- setXYOrigin: (NSPoint) point
{
	origin[0] = point.x;
	origin[1] = point.y;
	return self;
}

- setZOrigin: (float) point
{
	origin[2] = point;
	return self;
}

- setOrigin: (vec3_t) org angle: (float) angle
{
	VectorCopy (org, origin);
	angles[1] = angle;
	[self matrixFromAngles];
	return self;
}

- getOrigin: (vec3_t) org
{
	VectorCopy (origin, org);
	return self;
}

- (float)yawAngle
{
	return angles[1];
}

- upFloor: sender
{
	sb_floor_dir = 1;
	sb_floor_dist = 99999;
	[map makeObjectsPerformSelector: @selector(feetToFloor)];
	if (sb_floor_dist == 99999)
	{
		qprintf ("already on top floor");
		return self;
	}
	qprintf ("up floor");
	origin[2] += sb_floor_dist;
	[forge updateCamera];
	return self;
}

- downFloor: sender
{
	sb_floor_dir = -1;
	sb_floor_dist = -99999;
	[map makeAllPerform: @selector(feetToFloor)];
	if (sb_floor_dist == -99999)
	{
		qprintf ("already on bottom floor");
		return self;
	}
	qprintf ("down floor");
	origin[2] += sb_floor_dist;
	[forge updateCamera];
	return self;
}

/*
===============================================================================

UI TARGETS

===============================================================================
*/

/*
============
homeView
============
*/
- homeView: sender
{
	angles[0] = angles[2] = 0;
	
	[self matrixFromAngles];

	[forge updateAll];

	qprintf ("homed view angle");
	
	return self;
}

- drawMode: (NSMatrix) sender
{
	drawmode = [sender selectedColumn];
	[forge updateCamera];
	return self;
}

- setDrawMode: (drawmode_t)mode
{
	drawmode = mode;
	[mode_radio_i selectCellAtRow: 0 column: mode];
	[forge updateCamera];
	return self;
}

/*
===============================================================================

TRANSFORMATION METHODS

===============================================================================
*/

- matrixFromAngles
{
	if (angles[0] > M_PI*0.4)
		angles[0] = M_PI*0.4;
	if (angles[0] < -M_PI*0.4)
		angles[0] = -M_PI*0.4;

// vpn
	matrix[2][0] = cos(angles[0])*cos(angles[1]);
	matrix[2][1] = cos(angles[0])*sin(angles[1]);
	matrix[2][2] = sin(angles[0]);

// vup	
	matrix[1][0] = cos(angles[0]+M_PI/2)*cos(angles[1]);
	matrix[1][1] = cos(angles[0]+M_PI/2)*sin(angles[1]);
	matrix[1][2] = sin(angles[0]+M_PI/2);

// vright
	CrossProduct (matrix[2], matrix[1], matrix[0]);

	return self;
}


- inverseTransform: (vec_t *)invec to:(vec_t *)outvec
{
	vec3_t		inverse[3];
	vec3_t		temp;
	int			i,j;
	
	for (i=0 ; i<3 ; i++)
		for (j=0 ; j<3 ; j++)
			inverse[i][j] = matrix[j][i];
	
	temp[0] = DotProduct(invec, inverse[0]);
	temp[1] = DotProduct(invec, inverse[1]);
	temp[2] = DotProduct(invec, inverse[2]);		

	VectorAdd (temp, origin, outvec);

	return self;
}



/*
===============================================================================

						DRAWING METHODS

===============================================================================
*/

typedef struct
{
	vec3_t	trans;
	int		clipflags;
	vec3_t	screen;			// only valid if clipflags == 0
} campt_t;
#define	CLIP_RIGHT	1
#define	CLIP_LEFT	2
#define	CLIP_TOP	4
#define	CLIP_BOTTOM	8
#define	CLIP_FRONT	16

int		cam_cur;
campt_t	campts[2];

vec3_t	r_matrix[3];
vec3_t	r_origin;
float	mid_x, mid_y;
float	topscale = (240.0/3)/160;
float	bottomscale = (240.0*2/3)/160;

//extern	plane_t	frustum[5];

void MakeCampt (vec3_t in, campt_t *pt)
{
	vec3_t		temp;
	float		scale;
	
// transform the points
	VectorSubtract (in, r_origin, temp);
	
	pt->trans[0] = DotProduct(temp, r_matrix[0]);
	pt->trans[1] = DotProduct(temp, r_matrix[1]);
	pt->trans[2] = DotProduct(temp, r_matrix[2]);		

// check clip flags	
	if (pt->trans[2] < 1)
		pt->clipflags = CLIP_FRONT;
	else
		pt->clipflags = 0;

	if (pt->trans[0] > pt->trans[2])
		pt->clipflags |= CLIP_RIGHT;
	else if (-pt->trans[0] > pt->trans[2])
		pt->clipflags |= CLIP_LEFT;
		
	if (pt->trans[1] > pt->trans[2]*topscale )
		pt->clipflags |= CLIP_TOP;
	else if (-pt->trans[1] > pt->trans[2]*bottomscale )
		pt->clipflags |= CLIP_BOTTOM;
		
	if (pt->clipflags)
		return;
		
// project
	scale = mid_x/pt->trans[2];
	pt->screen[0] = mid_x + pt->trans[0]*scale;
	pt->screen[1] = mid_y + pt->trans[1]*scale;
}


void CameraMoveto(vec3_t p)
{
	campt_t	*pt;
	
	if (userPath->numberOfPoints > 2048)
		lineflush ();
		
	pt = &campts[cam_cur];
	cam_cur ^= 1;
	MakeCampt (p,pt);
	if (!pt->clipflags)
	{	// onscreen, so move there immediately
		UPmoveto (userPath, pt->screen[0], pt->screen[1]);
	}
}

void ClipLine (vec3_t p1, vec3_t p2, int planenum)
{
	float	d, d2, frac;
	vec3_t	new;
	mplane_t	*pl;
	float	scale;
	
	if (planenum == 5)
	{	// draw it!
		scale = mid_x/p1[2];
		new[0] = mid_x + p1[0]*scale;
		new[1] = mid_y + p1[1]*scale;
		UPmoveto (userPath, new[0], new[1]);
		
		scale = mid_x/p2[2];
		new[0] = mid_x + p2[0]*scale;
		new[1] = mid_y + p2[1]*scale;
		UPlineto (userPath, new[0], new[1]);
		return;
	}

	pl = &frustum[planenum];
	
	d = DotProduct (p1, pl->normal) - pl->dist;	
	d2 = DotProduct (p2, pl->normal) - pl->dist;
	if (d <= ON_EPSILON && d2 <= ON_EPSILON)
	{	// off screen
		return;
	}
	
	if (d >= 0 && d2 >= 0)
	{	// on front
		ClipLine (p1, p2, planenum+1);
		return;
	}
	
	frac = d/(d-d2);
	new[0] = p1[0] + frac*(p2[0]-p1[0]);
	new[1] = p1[1] + frac*(p2[1]-p1[1]);
	new[2] = p1[2] + frac*(p2[2]-p1[2]);
	
	if (d > 0)
		ClipLine (p1, new, planenum+1);
	else
		ClipLine (new, p2, planenum+1);
}

int	c_off, c_on, c_clip;

void CameraLineto(vec3_t p)
{
	campt_t		*p1, *p2;
	int			bits;
	
	p2 = &campts[cam_cur];
	cam_cur ^= 1;
	p1 = &campts[cam_cur];
	MakeCampt (p, p2);

	if (p1->clipflags & p2->clipflags)
	{
		c_off++;
		return;		// entirely off screen
	}
	
	bits = p1->clipflags | p2->clipflags;
	
	if (! bits )
	{
		c_on++;
	UPmoveto (userPath, p1->screen[0], p1->screen[1]);
		UPlineto (userPath, p2->screen[0], p2->screen[1]);
		return;		// entirely on screen
	}
	
// needs to be clipped
	c_clip++;

	ClipLine (p1->trans, p2->trans, 0);
}


/*
=============
drawSolid
=============
*/
- drawSolid
{
	const unsigned char	*planes[5];
		
//
// draw it
//
	VectorCopy (origin, r_origin);
	VectorCopy (matrix[0], r_matrix[0]);
	VectorCopy (matrix[1], r_matrix[1]);
	VectorCopy (matrix[2], r_matrix[2]);
	
	r_width = [self bounds].size.width;
	r_height = [self bounds].size.height;
	r_picbuffer = imagebuffer;
	r_zbuffer = zbuffer;

	r_drawflat = (drawmode == dr_flat);
	
	REN_BeginCamera ();
	REN_ClearBuffers ();

//
// render the setbrushes
//	
	[map makeAllPerform: @selector(CameraRenderSelf)];

//
// display the output
//
	[[self window] setBackingType:NSBackingStoreRetained];
	
	planes[0] = (unsigned char *)imagebuffer;
	NSDrawBitmap(
		bounds, r_width, r_height,
		8, 3, 32, r_width*4,
		NO, // not planar
		NO, // no alpha
		NSCalibratedRGBColorSpace,
		planes
	);

//	NSPing ();
	[[self window] setBackingType:NSBackingStoreBuffered];
	
	
	
	return self;
}


/*
===================
drawWire
===================
*/
- drawWire: (NSRect) rect
{
// copy current info to globals for the C callbacks	
	mid_x = bounds.size.width / 2;
	mid_y = 2 * bounds.size.height / 3;

	VectorCopy (origin, r_origin);
	VectorCopy (matrix[0], r_matrix[0]);
	VectorCopy (matrix[1], r_matrix[1]);
	VectorCopy (matrix[2], r_matrix[2]);
	
	r_width = bounds.size.width;
	r_height = bounds.size.height;
	r_picbuffer = imagebuffer;
	r_zbuffer = zbuffer;

	REN_BeginCamera ();
	
// erase window
	NSEraseRect (rect);
	
// draw all entities
	linestart (0,0,0);
	[map makeUnselectedPerform: @selector(CameraDrawSelf)];
	lineflush ();

	return self;
}

/*
===================
drawSelf
===================
*/
- drawSelf: (NSRect) rects :(int)rectCount
{
	static float	drawtime;	// static to shut up compiler warning

	if (timeDrawing)
		drawtime = I_FloatTime ();

	if (drawmode == dr_texture || drawmode == dr_flat)
		[self drawSolid];
	else
		[self drawWire: rects];

	if (timeDrawing) {
//		NSPing ();
		drawtime = I_FloatTime() - drawtime;
		printf ("CameraView drawtime: %5.3f\n", drawtime);
	}

	return self;
}


/*
=============
XYDrawSelf
=============
*/
- XYDrawSelf
{
	
	PSsetrgbcolor (0,0,1.0);
	PSsetlinewidth (0.15);
	PSmoveto (origin[0]-16,origin[1]);
	PSrlineto (16,8);
	PSrlineto (16,-8);
	PSrlineto (-16,-8);
	PSrlineto (-16,8);
	PSrlineto (32,0);
	
	PSmoveto (origin[0],origin[1]);
	PSrlineto (64*cos(angles[1]+M_PI/4), 64*sin(angles[1]+M_PI/4));
	PSmoveto (origin[0],origin[1]);
	PSrlineto (64*cos(angles[1]-M_PI/4), 64*sin(angles[1]-M_PI/4));
	
	PSstroke ();
	
	return self;
}

/*
=============
ZDrawSelf
=============
*/
- ZDrawSelf
{
	PSsetrgbcolor (0,0,1.0);
	PSsetlinewidth (0.15);
	
	PSmoveto (-16,origin[2]);
	PSrlineto (16,8);
	PSrlineto (16,-8);
	PSrlineto (-16,-8);
	PSrlineto (-16,8);
	PSrlineto (32,0);
	
	PSmoveto (-15,origin[2]-47);
	PSrlineto (29,0);
	PSrlineto (0,54);
	PSrlineto (-29,0);
	PSrlineto (0,-54);

	PSstroke ();

	return self;
}


/*
===============================================================================

						XYZ mouse view methods

===============================================================================
*/

/*
================
modalMoveLoop
================
*/
- modalMoveLoop: (NSPoint) basept : (vec3_t) movemod : converter
{
	vec3_t			originbase;
	NSEvent			*event;
	NSPoint			newpt;
//	NSPoint			brushpt;
	vec3_t			delta;
//	id				ent;
	int				i;
//	vec3_t			temp;
	unsigned int	eventMask = NSLeftMouseUpMask | NSRightMouseUpMask |
								NSLeftMouseDraggedMask | NSRightMouseDraggedMask |
								NSApplicationDefinedMask | NSKeyDownMask;
	
	qprintf ("moving camera position");

	VectorCopy (origin, originbase);	
		
//
// modal event loop using instance drawing
//
	goto drawentry;

	while ([event type] != NSLeftMouseUp && [event type] != NSRightMouseUp)
	{
		//
		// calculate new point
		//
		newpt = [event locationInWindow];
		[converter convertPoint: newpt fromView: nil];
				
		delta[0] = newpt.x - basept.x;
		delta[1] = newpt.y - basept.y;
		delta[2] = delta[1];		// height change
		
		for (i=0 ; i<3 ; i++)
			origin[i] = originbase[i]+movemod[i]*delta[i];
		
#if 0	// FIXME	
		//
		// if command is down, look towards brush or entity
		//
		if ([event modifierFlags] & NSShiftKeyMask)
		{
			ent = [quakemap selectedEntity];
			if (ent)
			{
				[ent origin: temp];
				brushpt.x = temp[0];
				brushpt.y = temp[1];
			}
			else
				brushpt = [brush_i centerPoint];
			angles[1] = atan2 (brushpt.y - newpt.y, brushpt.x - newpt.x);
			[self matrixFromAngles];
		}
#endif
					
drawentry:
		// instance draw new frame
		[forge newinstance];
		[self display];
		
		event = [NSApp nextEventMatchingMask: eventMask
						untilDate: [NSDate distantFuture]
						inMode: NSEventTrackingRunLoopMode
						dequeue: YES];

		if ([event type] == NSKeyDown) {
			[self _keyDown: event];
			[self display];
			goto drawentry;
		}
		
	}

	return self;
}

//============================================================================

/*
===============
XYmouseDown
===============
*/
- (BOOL) XYmouseDown: (NSPoint) pt flags: (unsigned int) flags	// return YES if brush handled
{	
	vec3_t		movemod;
	
	if (fabs(pt.x - origin[0]) > 16
	|| fabs(pt.y - origin[1]) > 16 )
		return NO;
	
#if 0	
	if (flags & NS_ALTERNATEMASK)
	{	// up / down drag
		movemod[0] = 0;
		movemod[1] = 0;
		movemod[2] = 1;
	}
	else
#endif
	{	
		movemod[0] = 1;
		movemod[1] = 1;
		movemod[2] = 0;
	}
	
	[self modalMoveLoop: pt : movemod : xyView];
	
	return YES;
}


/*
===============
ZmouseDown
===============
*/
- (BOOL) ZmouseDown: (NSPoint) pt flags: (unsigned int) flags	// return YES if brush handled
{	
	vec3_t		movemod;
	
	if (fabs(pt.y - origin[2]) > 16
	|| pt.x < -8 || pt.x > 8 )
		return NO;
		
	movemod[0] = 0;
	movemod[1] = 0;
	movemod[2] = 1;
	
	[self modalMoveLoop: pt : movemod : zView];

	return YES;
}


//=============================================================================

/*
===================
viewDrag:
===================
*/
- viewDrag: (NSPoint) pt
{
	float			dx,dy;
	NSEvent			*event;
	NSPoint			newpt;
	unsigned int	eventMask = NSKeyDownMask | NSRightMouseUpMask |
								NSRightMouseDraggedMask;
	
	// modal event loop using instance drawing
	goto drawentry;

	while ([event type] != NSRightMouseUp) {

		// calculate new point
		newpt = [event locationInWindow];
		[self convertPoint: newpt  fromView: nil];

		dx = newpt.x - pt.x;
		dy = newpt.y - pt.y;
		pt = newpt;
	
		angles[1] -= dx/bounds.size.width*M_PI/2 * 4;
		angles[0] += dy/bounds.size.width*M_PI/2 * 4;
	
		[self matrixFromAngles];
		
drawentry:
		[forge newinstance];
		[self display];

		event = [NSApp nextEventMatchingMask: eventMask
						untilDate: [NSDate distantFuture]
						inMode: NSEventTrackingRunLoopMode
						dequeue: YES];
	
		if ([event type] == NSKeyDown) {
			[self _keyDown: event];
			[self display];
			goto drawentry;
		}
		
	}

	return self;
}


//=============================================================================

/*
===================
mouseDown
===================
*/
- (void) mouseDown: (NSEvent *) theEvent
{
	NSPoint			pt;
	int				i;
	vec3_t			p1, p2;
	float			forward, right, up;
	int				flags;
		
	pt = [theEvent locationInWindow];
	
	[self convertPoint: pt fromView: nil];

	VectorCopy (origin, p1);
	forward = 160;
	right = pt.x - 160;
	up = pt.y - 240*2/3;
	for (i=0 ; i<3 ; i++)
		p2[i] = forward*matrix[2][i] + up*matrix[1][i] + right*matrix[0][i];
	for (i=0 ; i<3 ; i++)
		p2[i] = p1[i] + 100*p2[i];

	flags = [theEvent modifierFlags] & (NSShiftKeyMask | NSControlKeyMask |
										NSAlternateKeyMask | NSCommandKeyMask);

	// bare click to select a texture
	if (!flags) {
		[map getTextureRay: p1 : p2];
		return;
	}
	
	// shift click to select / deselect a brush from the world
	if (flags == NSShiftKeyMask) {
		[map selectRay: p1 : p2 : NO];
		return;
	}


	// cmd-shift click to set a target/targetname entity connection
	if (flags == (NSShiftKeyMask | NSCommandKeyMask)) {
		[map entityConnect: p1 : p2];
		return;
	}

	// alt click = set entire brush texture
	if (flags == NSAlternateKeyMask) {
		if (drawmode != dr_texture) {
			qprintf ("No texture setting except in texture mode!\n");
			NopSound ();
			return;
		}		
		[map setTextureRay: p1 : p2 : YES];
		[forge updateAll];
		return;
	}
		
	// ctrl-alt click = set single face texture
	if (flags == (NSControlKeyMask | NSAlternateKeyMask) ) {
		if (drawmode != dr_texture) {
			qprintf ("No texture setting except in texture mode!\n");
			NopSound ();
			return;
		}
		[map setTextureRay: p1 : p2 : NO];
		[forge updateAll];
		return;
	}

	qprintf ("bad modifier flags for click");
	NopSound ();
	
	return;
}

/*
===================
rightMouseDown
===================
*/
- (void) rightMouseDown: (NSEvent *) theEvent
{
	NSPoint			pt;
	int				flags;
		
	pt = [theEvent locationInWindow];
	
	[self convertPoint: pt fromView: nil];

	flags = [theEvent modifierFlags] & (NSShiftKeyMask | NSControlKeyMask |
								NSAlternateKeyMask | NSCommandKeyMask);

	// click = drag camera
	if (!flags) {
		qprintf ("looking");
		[self viewDrag: pt];
		return;
	}		

	qprintf ("bad modifier flags for click");
	NopSound ();
	
	return;
}

/*
===============
keyDown
===============
*/
- (void) _keyDown: (NSEvent *) theEvent
{
	NSString	*chars = [theEvent characters];
	unichar 	ch;
	
	if ([chars length] != 1) {
		qprintf ("Incorrect number of characters in keyDown event.");
		return;
	}

	ch = [chars characterAtIndex: 1];

	switch (ch) {
		case 13:
			return;
		
		case 'a':
		case 'A':
			angles[0] += M_PI/8;
			[self matrixFromAngles];
			[forge updateCamera];
			return;

		case 'z':
		case 'Z':
			angles[0] -= M_PI/8;
			[self matrixFromAngles];
			[forge updateCamera];
			return;

		case NSRightArrowFunctionKey:
			angles[1] -= M_PI*move/(64*2);
			[self matrixFromAngles];
			[forge updateCamera];
			break;

		case NSLeftArrowFunctionKey:
			angles[1] += M_PI*move/(64*2);
			[self matrixFromAngles];
			[forge updateCamera];
			break;

		case NSUpArrowFunctionKey:
			origin[0] += move*cos(angles[1]);
			origin[1] += move*sin(angles[1]);
			[forge updateCamera];
			break;

		case NSDownArrowFunctionKey:
			origin[0] -= move*cos(angles[1]);
			origin[1] -= move*sin(angles[1]);
			[forge updateCamera];
			break;

		case '.':
			origin[0] += move*cos(angles[1]-M_PI_2);
			origin[1] += move*sin(angles[1]-M_PI_2);
			[forge updateCamera];
			break;

		case ',':
			origin[0] -= move*cos(angles[1]-M_PI_2);
			origin[1] -= move*sin(angles[1]-M_PI_2);
			[forge updateCamera];
			break;

		case 'd':
		case 'D':
			origin[2] += move;
			[forge updateCamera];
			break;

		case 'c':
		case 'C':
			origin[2] -= move;
			[forge updateCamera];
			break;
	}
    return;
}

@end
