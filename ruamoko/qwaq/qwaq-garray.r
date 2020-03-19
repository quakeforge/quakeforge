#include <Array.h>
#include "event.h"
#include "qwaq-garray.h"

@implementation Array (Group)
- (void) makeObjectsPerformSelector: (SEL)selector
								 if: (condition_func)condition
							   with: (void *)data
{
	for (int i = 0; i < [self count]; i++) {
		if (condition (_objs[i], data)) {
			[_objs[i] performSelector: selector];
		}
	}
}

- (void) makeObjectsPerformSelector: (SEL)selector
						 withObject: (void *)anObject
								 if: (condition_func2)condition
							   with: (void *)data
{
	for (int i = 0; i < [self count]; i++) {
		if (condition (_objs[i], anObject, data)) {
			[_objs[i] performSelector: selector withObject: anObject];
		}
	}
}

- (void) makeReversedObjectsPerformSelector: (SEL)selector
{
	for (int i = [self count]; i-->0; ) {
		[_objs[i] performSelector: selector];
	}
}

- (void) makeReversedObjectsPerformSelector: (SEL)selector
								 withObject: (void *)anObject
{
	for (int i = [self count]; i-->0; ) {
		[_objs[i] performSelector: selector withObject: anObject];
	}
}

- (void) makeReversedObjectsPerformSelector: (SEL)selector
										 if: (condition_func)condition
									   with: (void *)data
{
	for (int i = [self count]; i-->0; ) {
		if (condition (_objs[i], data)) {
			[_objs[i] performSelector: selector];
		}
	}
}

- (void) makeReversedObjectsPerformSelector: (SEL)selector
								 withObject: (void *)anObject
										 if: (condition_func2)condition
									   with: (void *)data
{
	for (int i = [self count]; i-->0; ) {
		if (condition (_objs[i], anObject, data)) {
			[_objs[i] performSelector: selector withObject: anObject];
		}
	}
}
@end
