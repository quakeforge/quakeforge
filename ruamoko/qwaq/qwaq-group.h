#ifndef __qwaq_group_h
#define __qwaq_group_h

#include <Array.h>
#include "qwaq-view.h"

typedef BOOL condition_func (id object, void *data);
typedef BOOL condition_func2 (id object, void *anObject, void *data);

@interface Array (Group)
- (void) makeObjectsPerformSelector: (SEL)selector
								 if: (condition_func)condition
							   with: (void *)data;
- (void) makeObjectsPerformSelector: (SEL)selector
						 withObject: (void *)anObject
								 if: (condition_func2)condition
							   with: (void *)data;
- (void) makeReversedObjectsPerformSelector: (SEL)selector;
- (void) makeReversedObjectsPerformSelector: (SEL)selector
								 withObject: (void *)anObject;
- (void) makeReversedObjectsPerformSelector: (SEL)selector
										 if: (condition_func)condition
									   with: (void *)data;
- (void) makeReversedObjectsPerformSelector: (SEL)selector
								 withObject: (void *)anObject
										 if: (condition_func2)condition
									   with: (void *)data;
@end

@interface Group : View
{
	Array      *views;
	int         focused;
	id          buffer;	//FIXME id<TextContext> or sim
}
-initWithContext: (id) context;	//FIXME id<TextContext> or sim
-insert: (View *) view;
-remove: (View *) view;
@end

#endif//__qwaq_group_h
