#include "draw.h"
#include "debug.h"
#include "gib.h"
#include "HUD.h"

integer HUDHandleClass;

@implementation HUDObject
+ (void) initClass
{
	HUDHandleClass = GIB_Handle_Class_New ();
}

- (id) initWithComponents: (integer) x : (integer) y
{
	origin = [[Point alloc] initWithComponents: x :y];
	visible = YES;
	handle = GIB_Handle_New (self, HUDHandleClass);

	return self;
}

- (void) free
{
	[origin free];
	GIB_Handle_Free (handle, HUDHandleClass);
	[super free];
}

- (integer) handle
{
	return handle;
}

- (Point) origin
{
	return origin;
}

- (void) setOrigin: (Point) newPoint
{
	[origin setPoint :newPoint];
}

- (void) translate: (Point) addPoint
{
	[origin addPoint :addPoint];
}

- (BOOL) isVisible
{
	return visible;
}

- (void) setVisible: (BOOL) _visible
{
	visible = _visible;
}

- (void) display
{
}
@end

@implementation HUDText : HUDObject
- (id) initWithComponents: (integer) x :(integer) y :(string) _text
{
	self = [super initWithComponents :x :y];
	text = _text;

	return self;
}

- (string) text
{
	return text;
}

- (void) setText: (string) _text
{
	text = _text;
}

- (void) display
{
	if (visible)
		Draw_String ([origin x], [origin y], text);
}
@end

@implementation HUDGraphic : HUDObject
- (id) initWithComponents: (integer)x :(integer)y :(string) _file
{
	self = [super initWithComponents :x :y];
	picture = [[QPic alloc] initName :_file];

	return self;
}

- (void) free
{
	[super free];
	[picture free];
}

- (void) setFile: (string) _file
{
	[picture free];
	picture = [[QPic alloc] initName :_file];
}

- (void) display
{
	if (visible)
		[picture draw :[origin x] :[origin y]];
}
@end
