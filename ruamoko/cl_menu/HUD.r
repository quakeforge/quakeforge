#include "draw.h"
#include "debug.h"
#include "gib.h"
#include "string.h"
#include "HUD.h"

integer HUDHandleClass;

@implementation HUDObject
+ (void) initClass
{
	HUDHandleClass = GIB_Handle_Class_New ();
}

- (id) initWithComponents: (integer) x : (integer) y
{
	self = [super init];
	origin = [[Point alloc] initWithComponents: x :y];
	visible = YES;
	handle = GIB_Handle_New (self, HUDHandleClass);

	return self;
}

- (void) free
{
	[origin free];
	[size free];
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

- (Point) size
{
	return size;
}

- (void) setOrigin: (Point) newPoint
{
	[origin setPoint :newPoint];
}

- (void) translate: (Point) addPoint
{
	[origin addPoint :addPoint];
}

/*
- (void) center Horizontal: (BOOL) h Vertical: (BOOL) v
{
	Point newCoords;
	integer newX, newY;

	if (h) {
		newX = X
*/

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
	[self setText :_text];

	return self;
}

- (string) text
{
	return text;
}

- (void) setText: (string) _text
{
	text = _text;
	[size free];
	size = [[Point alloc] initWithComponents :8*(integer) strlen (text) :8];
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
	[self setFile :_file];

	return self;
}

- (void) free
{
	[picture free];
	[super free];
}

- (void) setFile: (string) _file
{
	[picture free];
	picture = [[QPic alloc] initName :_file];
	[size free];
	size = [[Point alloc] initWithComponents :[picture width] :[picture height]];
}

- (void) display
{
	if (visible)
		[picture draw :[origin x] :[origin y]];
}
@end
