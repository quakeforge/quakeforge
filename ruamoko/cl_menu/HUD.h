#include "Point.h"

@interface HUDObject : Object
{
	Point origin;
	BOOL visible;
	integer handle;
}

+ (void) initClass;
- (id) initWithComponents: (integer) x : (integer) y;
- (void) free;
- (integer) handle;
- (Point) origin;
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
- (string) text;
- (void) setText: (string) _text;
- (void) display;
@end

@interface HUDGraphic : HUDObject
{
	QPic picture;
}

- (id) initWithComponents: (integer)x :(integer)y :(string) _file;
- (void) free;
- (void) setFile: (string) _file;
- (void) display;
@end

@extern void () HUD_Init;
@extern integer HUDHandleClass;
