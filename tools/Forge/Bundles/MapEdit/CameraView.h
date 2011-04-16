#ifndef CameraView_h
#define CameraView_h

#include <AppKit/AppKit.h>

#include "QF/mathlib.h"

#include "SetBrush.h"

#include "render.h"

extern id  cameraview_i;

extern byte  renderlist[1024 * 1024 * 4];

void        CameraMoveto (vec3_t p);
void        CameraLineto (vec3_t p);

extern BOOL  timedrawing;

@interface CameraView: NSView
{
	float   xa, ya, za;
	float   move;

	int         camwidth, camheight;
	float       *zbuffer;
	unsigned    *imagebuffer;

	BOOL  angleChange;              // JR 6.8.95

	vec3_t  origin;
	vec3_t  matrix[3];

	NSPoint  dragspot;

	drawmode_t  drawmode;

	NSBezierPath    *xycamera;
	NSBezierPath    *xycamera_aim;
	NSBezierPath    *zcamera;

// UI links
	id  mode_radio_i;
}

- (id) setXYOrigin: (NSPoint *)pt;
- (id) setZOrigin: (float)pt;

- (id) setOrigin: (vec3_t)org
   angle: (float)angle;

- (id) getOrigin: (vec3_t)org;

- (float) yawAngle;

- (id) matrixFromAngles;
- (id) _keyDown: (NSEvent *)theEvent;

- (id) drawMode: sender;
- (id) setDrawMode: (drawmode_t)mode;

- (id) homeView: sender;

- (void) XYDrawSelf;                // for drawing viewpoint in XY view
- (void) ZDrawSelf;                 // for drawing viewpoint in XY view
- (BOOL) XYmouseDown: (NSPoint *)pt // return YES if brush handled
   flags: (int)flags;

- (BOOL) ZmouseDown: (NSPoint *)pt // return YES if brush handled
   flags: (int)flags;

- (id) upFloor: sender;
- (id) downFloor: sender;

@end
#endif // CameraView_h
