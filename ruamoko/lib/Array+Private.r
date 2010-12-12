#include "Array+Private.h"

#if 0
@implementation Array (Private)

/**
	This is a somewhat dangerous thing to do, and it's only done so that we can 
	use an Array to implement AutoreleasePool.
*/
- (void) addObjectNoRetain: (id)anObject
{
	if (count == capacity) {
		capacity += granularity;
		_objs = (id [])obj_realloc (_objs, capacity * @sizeof (id));
	}
	_objs[count++] = anObject;
}

- (void) removeObjectNoRelease: (id)anObject
{
	local unsigned	i = count;
	local unsigned	tmp;

	do {
		if (_objs[--i] == anObject) {
			for (tmp = i; tmp < count; tmp++) {
				_objs[tmp] = _objs[tmp + 1];
			}
			count--;
		}
	} while (i);
}

@end
#endif
