#include "Frame.h"
#include "Array.h"

@class Point;

@interface HUDObject : Object
{
	Point origin;
	BOOL visible;
	integer handle;
}

- (id) initWithComponents: (integer) x : (integer) y;
- (void) dealloc;
- (integer) handle;
- (Point) origin;
- (Point) size;
- (void) setOrigin: (Point) newPoint;
- (void) translate: (Point) addPoint;
- (BOOL) isVisible;
- (void) setVisible: (BOOL) _visible;
- (void) display;
@end

@interface HUDText : HUDObject
{
	string text;
}

- (id) initWithComponents: (integer) x :(integer) y :(string) _text;
- (Point) size;
- (string) text;
- (void) setText: (string) _text;
- (void) display;
@end

@interface HUDGraphic : HUDObject
{
	QPic picture;
}

- (id) initWithComponents: (integer)x :(integer)y :(string) _file;
- (void) dealloc;
- (Point) size;
- (void) setFile: (string) _file;
- (void) display;
@end

@extern void () HUD_Init;
@extern integer HUDHandleClass;

@interface HUDAnimation : HUDObject
{
	Array frames;
	integer currentFrame;
	float nextFrameTime;
	BOOL looping;
}
- (id) initWithComponents: (integer) x :(integer) y;
- (void) dealloc;
- (Point) size;
- (void) addFrame: (Frame) frame;
- (void) changeFrame;
- (void) display;
- (void) start;
- (void) stop;
- (void) setLooping: (BOOL) _looping;
@end
