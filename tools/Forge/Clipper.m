/*
	Clipper.h

	Clipping plane class

	Copyright (C) 2001 Jeff Teunissen <deek@d2dc.net>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License as
	published by the Free Software Foundation; either version 2 of
	the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public
	License along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

	$Id$
*/
#ifdef HAVE_CONFIG_H
# include "Config.h"
#endif

#include <QF/mathlib.h>
#include <QF/bspfile.h>

#import <AppKit/NSGraphics.h>
#import <AppKit/DPSOperators.h>

#import "CameraView.h"
#import "Clipper.h"
#import "Forge.h"
#import "Map.h"
#import "SetBrush.h"
#import "XYView.h"
#import "ZView.h"

Clipper 		*clipper;

@implementation Clipper

- init
{
	[super init];
	clipper = self;
	return self;	
}

- (BOOL) hide
{
	int		oldnum;
	
	oldnum = num;
	num = 0;
	return (oldnum > 0);
}

- flipNormal
{
	vec3_t	temp;

	switch (num) {
		case 2:
			VectorCopy (pos[0], temp);
			VectorCopy (pos[1], pos[0]);
			VectorCopy (temp, pos[1]);
			break;
		case 3:
			VectorCopy (pos[0], temp);
			VectorCopy (pos[2], pos[0]);
			VectorCopy (temp, pos[2]);
			break;
		default:
			NSLog (@"No clipping plane");
			NSBeep ();
	}
	return self;
}

- (BOOL) getFace: (face_t *) f
{
	vec3_t	v1, v2, norm;
	int		i;
	
	VectorCopy (vec3_origin, plane.normal);
	plane.dist = 0;
	if (num < 2)
		return NO;
	if (num == 2)
	{
		VectorCopy (pos[0], pos[2]);
		pos[2][2] += 16;
	}
	
	for (i=0 ; i<3 ; i++)
		VectorCopy (pos[i], f->planepts[i]);
		
	VectorSubtract (pos[2], pos[0], v1);
	VectorSubtract (pos[1], pos[0], v2);
	
	CrossProduct (v1, v2, norm);
	VectorNormalize (norm);
	
	if ( !norm[0] && !norm[1] && !norm[2] )
		return NO;
	
	[texturepalette_i setTextureDef: &f->texture];

	return YES;
}

/*
================
XYClick
================
*/
- XYClick: (NSPoint) pt
{
	int		i;
	vec3_t	new;
		
	new[0] = [xyView snapToGrid: pt.x];
	new[1] = [xyView snapToGrid: pt.y];
	new[2] = [map currentMinZ];

	// see if a point is already there
	for (i=0 ; i<num ; i++) {
		if (new[0] == pos[i][0] && new[1] == pos[i][1]) {
			if (pos[i][2] == [map currentMinZ])
				pos[i][2] = [map currentMaxZ];
			else
				pos[i][2] = [map currentMinZ];
			[forge updateAll];
			return self;
		}
	}
	
	
	if (num == 3)
		num = 0;
	
	VectorCopy (new, pos[num]);
	num++;

	[forge updateAll];
	
	return self;
}

/*
================
XYDrag
================
*/
- (BOOL) XYDrag: (NSPoint *) pt
{
	int		i;

	for (i=0 ; i<3 ; i++) {
		if (fabs(pt->x - pos[i][0] > 10) || fabs(pt->y - pos[i][1] > 10))
			continue;	// drag this point
	}
	return NO;
}

- ZClick: (NSPoint) pt
{
	return self;
}

//=============================================================================

- carve
{
	[map makeSelectedPerform: @selector(carveByClipper)];
	num = 0;
	return self;
}


- (void) cameraDrawSelf
{
	vec3_t		mid;
	int			i;
	
	linecolor (1, 0.5, 0);

	for (i = 0; i < num; i++) {
		VectorCopy (pos[i], mid);
		mid[0] -= 8;
		mid[1] -= 8;
		CameraMoveto (mid);
		mid[0] += 16;
		mid[1] += 16;
		CameraLineto (mid);
		
		VectorCopy (pos[i], mid);
		mid[0] -= 8;
		mid[1] += 8;
		CameraMoveto (mid);
		mid[0] += 16;
		mid[1] -= 16;
		CameraLineto (mid);
	}
	
	return;
}

- (void) xyDrawSelf
{
	int		i;
	char	text[8];
	
	PSsetrgbcolor (1, 0.5, 0);
	PSselectfont ("Helvetica-Medium", 10 / [xyView currentScale]);
	PSrotate (0);

	for (i = 0; i < num; i++) {
		PSmoveto (pos[i][0] - 4, pos[i][1] - 4);
		sprintf (text, "%i", i);
		PSshow (text);
		PSstroke ();
		PSarc (pos[i][0], pos[i][1], 10, 0, 360);
		PSstroke ();
	}
	return;
}

- (void) zDrawSelf
{
	int		i;
	char	text[8];
	
	PSsetrgbcolor (1, 0.5, 0);
	PSselectfont ("Helvetica-Medium", 10 / [zView currentScale]);
	PSrotate (0);

	for (i = 0; i < num; i++) {
		PSmoveto ((-28 + (i * 8)) - 4, pos[i][2] - 4);
		sprintf (text, "%i", i);
		PSshow (text);
		PSstroke ();
		PSarc ((-28 + (i * 8)), pos[i][2], 10, 0, 360);
		PSstroke ();
	}
	return;
}

@end
