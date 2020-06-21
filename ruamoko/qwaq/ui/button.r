#include "ruamoko/qwaq/ui/button.h"
#include "ruamoko/qwaq/ui/listener.h"

@implementation Button

+(Button *)withPos:(Point)pos releasedIcon:(DrawBuffer *)released
							   pressedIcon:(DrawBuffer *)pressed
{
	return [[[self alloc] initWithPos:pos
						 releasedIcon:released
						  pressedIcon:pressed] autorelease];
}

+(Button *)withRect:(Rect)rect
{
	return [[[self alloc] initWithRect:rect] autorelease];
}

-initWithPos: (Point) pos releasedIcon: (DrawBuffer *) released
						   pressedIcon: (DrawBuffer *) pressed
{
	Extent      size = mergeExtents ([released size], [pressed size]);

	if (!(self = [super initWithRect: {pos, size}])) {
		return nil;
	}
	icon[0] = [released retain];
	icon[1] = [pressed retain];
	onPress = [[ListenerGroup listener] retain];
	onRelease = [[ListenerGroup listener] retain];
	onClick = [[ListenerGroup listener] retain];
	onDrag = [[ListenerGroup listener] retain];
	onAuto = [[ListenerGroup listener] retain];
	onHover = [[ListenerGroup listener] retain];
	return self;
}

-initWithRect: (Rect) rect
{
	if (!(self = [super initWithRect: rect])) {
		return nil;
	}
	icon[0] = nil;
	icon[1] = nil;
	onPress = [[ListenerGroup listener] retain];
	onRelease = [[ListenerGroup listener] retain];
	onClick = [[ListenerGroup listener] retain];
	onDrag = [[ListenerGroup listener] retain];
	onAuto = [[ListenerGroup listener] retain];
	onHover = [[ListenerGroup listener] retain];
	return self;
}

-(void)dealloc
{
	[icon[0] release];
	[icon[1] release];
	[onPress release];
	[onRelease release];
	[onClick release];
	[onDrag release];
	[onAuto release];
	[onHover release];
	[super dealloc];
}

-draw
{
	[super draw];
	if (icon[pressed]) {
		[textContext blitFromBuffer: icon[pressed]
								 to: pos
							   from: [icon[pressed] rect]];
	}
	return self;
}

-handleEvent: (qwaq_event_t *) event
{
	ListenerGroup *action = nil;

	[super handleEvent: event];

	if (event.what & qe_mouse) {
		switch ((qwaq_mouse_event) (event.what & qe_mouse)) {
			case qe_mousedown:
				pressed = 1;
				click = 0;
				dragBase = {event.mouse.x, event.mouse.y};
				action = onPress;
				[self grabMouse];
				[self redraw];
				break;
			case qe_mouseup:
				pressed = 0;
				click = 0;
				action = onRelease;
				[self releaseMouse];
				if ([self containsPoint: {event.mouse.x, event.mouse.y}]) {
					[onClick respond: self];
				}
				[self redraw];
				break;
			case qe_mouseclick:
				action = onClick;
				click = event.mouse.click;
				break;
			case qe_mousemove:
				click = 0;
				if (pressed) {
					dragPos = {event.mouse.x, event.mouse.y};
					action = onDrag;
				}
				break;
			case qe_mouseauto:
				if (pressed
					&& [self containsPoint: {event.mouse.x, event.mouse.y}]) {
					click = 0;
					action = onAuto;
				}
				break;
		}
		if (action) {
			[action respond: self];
		}
		event.what = qe_none;
	}
	return self;
}

-(ListenerGroup *) onPress
{
	return onPress;
}

-(ListenerGroup *) onRelease
{
	return onRelease;
}

-(ListenerGroup *) onClick
{
	return onClick;
}

-(ListenerGroup *) onDrag
{
	return onDrag;
}

-(ListenerGroup *) onAuto
{
	return onAuto;
}

-(ListenerGroup *) onHover
{
	return onHover;
}

- (int) click
{
	return click;
}

- (Point) delta
{
	return {dragPos.x - dragBase.x, dragPos.y - dragBase.y};
}

-move:(Point) delta
{
	Point       pos = self.pos;
	[super move:delta];
	dragBase.x += self.pos.x - pos.x;
	dragBase.y += self.pos.y - pos.y;
	return self;
}

@end
