/*
	CameraView.h

	Perspective viwer class definitions

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

#import "SetBrush.h"

@interface CameraView:  NSView
{
	float		angles[3];
	
	NSRect		bounds;

	float		move;
		
	float		*zbuffer;
	unsigned	*imagebuffer;
	
	BOOL		angleChange;		// JR 6.8.95
	
	vec3_t		origin;
	vec3_t		matrix[3];
	
	NSPoint		dragspot;
	
	drawmode_t	drawmode;
	
	// UI links
	id			mode_radio_i;
	
}

- setXYOrigin: (NSPoint) point;
- setZOrigin: (float) point;

- setOrigin: (vec3_t) org angle: (float) angle;
- getOrigin: (vec3_t) org;

- (float) yawAngle;

- matrixFromAngles;
- (void) _keyDown: (NSEvent *) theEvent;

- drawMode: (NSMatrix) sender;
- setDrawMode: (drawmode_t) mode;

- (id) homeView: (id) sender;

- XYDrawSelf;						// for drawing viewpoint in XY view
- ZDrawSelf;						// for drawing viewpoint in XY view
- (BOOL) XYmouseDown: (NSPoint) pt flags: (unsigned int) flags;	// return YES if brush handled
- (BOOL) ZmouseDown: (NSPoint) pt flags: (unsigned int) flags;	// return YES if brush handled

- upFloor: sender;
- downFloor: sender;

@end

extern CameraView	*cameraView;
extern byte 		renderList[1024*1024*4];
extern	BOOL		timeDrawing;

void CameraMoveto (vec3_t p);
void CameraLineto (vec3_t p);
