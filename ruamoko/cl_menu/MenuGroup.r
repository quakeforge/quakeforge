#include "key.h"
#include "sound.h"

#include "Array.h"

#include "options_util.h"
#include "MenuGroup.h"

@implementation MenuGroup
-(id) init
{
	if ((self = [super init]))
		current = base = 0;
}

-(void)setBase:(integer)b
{
	if (b >= [views count])
		b = [views count] - 1;
	if (b < 0)
		b = 0;
	current = base = b;
}

- (integer) keyEvent:(integer)key unicode:(integer)unicode down:(integer)down
{
	switch (key) {
		case QFK_DOWN:
		case QFM_WHEEL_DOWN:
			[self next];
			return 1;
		case QFK_UP:
		case QFM_WHEEL_UP:
			[self prev];
			return 1;
		default:
			return [[views objectAtIndex: current]
						keyEvent: key
						unicode: unicode
						down: down];
	}
}

-(void) next
{
	if (++current >= [views count])
		current = base;
	S_LocalSound ("misc/menu1.wav");
}

-(void) prev
{
	if (--current < base)
		current = [views count] - 1;
	S_LocalSound ("misc/menu1.wav");
}

- (void) draw
{
	local View []cur;

	[super draw];
	cur = [views objectAtIndex:current];
	opt_cursor  (cur.xabs - 8, cur.yabs);
}
@end
