#include "qwaq-button.h"
#include "qwaq-listener.h"

@implementation Button
-initWithPos: (Point) pos releasedIcon: (DrawBuffer *) released
						   pressedIcon: (DrawBuffer *) pressed
{
	Extent      size = mergeExtents ([released size], [pressed size]);

	if (!(self = [super initWithRect: {pos, size}])) {
		return nil;
	}
	icon[0] = released;
	icon[1] = pressed;
	onPress = [[ListenerGroup alloc] init];
	onRelease = [[ListenerGroup alloc] init];
	onClick = [[ListenerGroup alloc] init];
	onDrag = [[ListenerGroup alloc] init];
	onAuto = [[ListenerGroup alloc] init];
	onHover = [[ListenerGroup alloc] init];
	return self;
}

-draw
{
	[super draw];
	[textContext blitFromBuffer: icon[pressed]
							 to: pos
						   from: [icon[pressed] rect]];
	return self;
}

-handleEvent: (qwaq_event_t *) event
{
	ListenerGroup *action = nil;

	if (event.what & qe_mouse) {
		switch ((qwaq_mouse_event) (event.what & qe_mouse)) {
			case qe_mousedown:
				pressed = 1;
				[self redraw];
				action = onPress;
				break;
			case qe_mouseup:
				pressed = 0;
				[self redraw];
				action = onRelease;
				break;
			case qe_mouseclick:
				action = onClick;
				break;
			case qe_mousemove:
				if (pressed) {
					action = onDrag;
				}
				break;
			case qe_mouseauto:
				action = onAuto;
				break;
		}
		if (action) {
			[action respond: self];
		}
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
@end
