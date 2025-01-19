#include "Frame.h"
#include "Array.h"

#include "gui/Point.h"
#include "gui/Size.h"

@interface HUDObject : Object
{
	Point origin;
	BOOL visible;
	int handle;
}

- (id) initWithComponents: (int) x : (int) y;
- (void) dealloc;
- (int) handle;
- (Point) origin;
- (Size) size;
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

- (id) initWithComponents: (int) x :(int) y :(string) _text;
- (Size) size;
- (string) text;
- (void) setText: (string) _text;
- (void) display;
@end

@interface HUDGraphic : HUDObject
{
	QPic *picture;
}

- (id) initWithComponents: (int)x :(int)y :(string) _file;
- (void) dealloc;
- (Size) size;
- (void) setFile: (string) _file;
- (void) display;
@end

@extern void () HUD_Init;
@extern int HUDHandleClass;

@interface HUDAnimation : HUDObject
{
	Array *frames;
	unsigned currentFrame;
	float nextFrameTime;
	BOOL looping;
}
- (id) initWithComponents: (int) x :(int) y;
- (void) dealloc;
- (Size) size;
- (void) addFrame: (Frame *) frame;
- (void) changeFrame;
- (void) display;
- (void) start;
- (void) stop;
- (void) setLooping: (BOOL) _looping;
@end
