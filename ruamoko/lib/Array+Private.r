#include "Array+Private.h"

@implementation Array (Private)

/**
	This is a somewhat dangerous thing to do, and it's done only so that we can 
	use an Array to implement AutoreleasePool.
*/
- (void) addObjectNoRetain: (id)anObject
{
	if (count == capacity) {
		capacity += granularity;
		_objs = (id *)obj_realloc (_objs, capacity * @sizeof (id));
	}
	_objs[count++] = anObject;
}

- (void) removeObjectNoRelease: (id)anObject
{
	local unsigned	i = count;
	local unsigned	tmp;

	while (i-- > 0) {
		if (_objs[i] == anObject) {
			for (tmp = i; tmp < count - 1; tmp++) {
				_objs[tmp] = _objs[tmp + 1];
			}
			count--;
		}
	}
}

@end
