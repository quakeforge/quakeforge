#include "draw.h"
#include "debug.h"
#include "gib.h"
#include "string.h"
#include "system.h"
#include "HUD.h"

integer HUDHandleClass;

@implementation HUDObject
- (id) initWithComponents: (integer) x : (integer) y
{
	self = [super init];
	origin = [[Point alloc] initWithComponents: x :y];
	visible = YES;

	return self;
}

- (void) dealloc
{
	[origin release];
	[super dealloc];
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
	return NIL;
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
	[self setText :_text];

	return self;
}

- (Point) size
{
	return [[Point alloc] initWithComponents :8*(integer) strlen (text) :8];
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
	[self setFile :_file];

	return self;
}

- (void) dealloc
{
	[picture release];
	[super dealloc];
}

- (Point) size
{
	return [[Point alloc] initWithComponents :[picture width] :[picture height]];
}

- (void) setFile: (string) _file
{
	[picture release];
	picture = [[QPic alloc] initName :_file];
}

- (void) display
{
	if (visible)
		[picture draw :[origin x] :[origin y]];
}
@end

@implementation HUDAnimation : HUDObject

- (id) initWithComponents: (integer) x :(integer) y
{
	self = [super initWithComponents :x :y];
	frames = [[Array alloc] init];
	currentFrame = 0;
	nextFrameTime = 0;
	looping = NO;

	return self;
}

- (void) dealloc
{
	[frames release];
	[super dealloc];
}

- (Point) size
{
	local Frame frame;

	frame = [frames getItemAt :currentFrame];
	return [frame size];
}

- (void) addFrame: (Frame) frame
{
	[frames addItem :frame];
}

- (void) changeFrame
{
	while (time >= nextFrameTime) {
		local Frame f;
		if (++currentFrame == [frames count]) {
			if (looping)
				currentFrame = 0;
			else {
				nextFrameTime = 0.0;
				currentFrame = 0;
				return;
			}
		}
		f = [frames getItemAt :currentFrame];
		nextFrameTime += [f duration];
	}
}

- (void) display
{
	local Frame f;

	if (!visible)
		return;

	if (nextFrameTime)
		[self changeFrame];

	f = [frames getItemAt :currentFrame];
	[f draw :[origin x] :[origin y]];
}

- (void) start
{
	local Frame f;

	currentFrame = 0;
	f = [frames getItemAt :0];
	nextFrameTime = time + [f duration];
}

- (void) stop
{
	nextFrameTime = 0.0;
	currentFrame = 0;
}

- (void) setLooping: (BOOL) _looping
{
	looping = _looping;
}

@end
