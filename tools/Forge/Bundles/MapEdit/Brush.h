#ifndef Brush_h
#define Brush_h

#include <AppKit/AppKit.h>
#include "SetBrush.h"
#include "EditWindow.h"

extern id   brush_i;

extern BOOL brushdraw;					// YES when drawing cutbrushes and ents

@interface Brush:SetBrush {
	id          cutbrushes_i;
	id          cutentities_i;
	boolean     updatemask[MAXBRUSHVERTEX];
	BOOL        dontdraw;				// for modal instance loops 
	BOOL        deleted;				// when not visible at all 
}

-init;

-initFromSetBrush:br;

-deselect;
-(BOOL) isSelected;

-(BOOL) XYmouseDown:(NSPoint *) pt;	// return YES if brush handled
-(BOOL) ZmouseDown:(NSPoint *) pt;		// return YES if brush handled

-_keyDown:(NSEvent *) theEvent;

-(NSPoint) centerPoint;					// for camera flyby mode

-InstanceSize;
-XYDrawSelf;
-ZDrawSelf;
-CameraDrawSelf;

-flipHorizontal:sender;
-flipVertical:sender;
-rotate90:sender;

-makeTall:sender;
-makeShort:sender;
-makeWide:sender;
-makeNarrow:sender;

-placeEntity:sender;

-cut:sender;
-copy:sender;

-addBrush;

@end
#define Brush_h
