#ifndef CameraView_h
#define CameraView_h

#include <AppKit/AppKit.h>

#include "QF/mathlib.h"

#include "SetBrush.h"

#include "render.h"

extern id cameraview_i;

extern	byte	renderlist[1024*1024*4];

void CameraMoveto(vec3_t p);
void CameraLineto(vec3_t p);

extern	BOOL	timedrawing;

@interface CameraView :  NSView
{
	float		xa, ya, za;
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

- setXYOrigin: (NSPoint *)pt;
- setZOrigin: (float)pt;

- setOrigin: (vec3_t)org angle: (float)angle;
- getOrigin: (vec3_t)org;

- (float)yawAngle;

- matrixFromAngles;
- _keyDown: (NSEvent *)theEvent;

- drawMode: sender;
- setDrawMode: (drawmode_t)mode;

- homeView: sender;

- XYDrawSelf;						// for drawing viewpoint in XY view
- ZDrawSelf;						// for drawing viewpoint in XY view
- (BOOL)XYmouseDown: (NSPoint *)pt flags:(int)flags;	// return YES if brush handled
- (BOOL)ZmouseDown: (NSPoint *)pt flags:(int)flags;	// return YES if brush handled

- upFloor:sender;
- downFloor: sender;

@end

#endif//CameraView_h
