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
	return self;
}

-(void)setBase:(unsigned)b
{
	if (b >= [views count])
		b = [views count] - 1;
	if (b < 0)
		b = 0;
	current = base = b;
}

- (int) keyEvent:(int)key unicode:(int)unicode down:(int)down
{
	View *cur = [views objectAtIndex: current];
	int   ret = [cur keyEvent: key unicode: unicode down: down];
	if (!ret) {
		switch (key) {
			case QFK_DOWN:
			//case QFM_WHEEL_DOWN:
				[self next];
				return 1;
			case QFK_UP:
			//case QFM_WHEEL_UP:
				[self prev];
				return 1;
		}
	}
	return ret;
}

-(void) next
{
	if (++current >= [views count])
		current = base;
	S_LocalSound ("misc/menu1.wav");
}

-(void) prev
{
	if (current-- == base)
		current = [views count] - 1;
	S_LocalSound ("misc/menu1.wav");
}

- (void) draw
{
	local View *cur;

	[super draw];
	cur = [views objectAtIndex:current];
	opt_cursor  (cur.xabs - 8, cur.yabs);
}
@end
