#ifndef __ruamoko_List_h
#define __ruamoko_List_h

#include "Object.h"

@interface List: Object
{
	integer count;
	struct list_bucket_s [] head;
	struct list_bucket_s [][] tail;
}
- (id) init;
- (id) getItemAt: (integer) index;
- (id) head;
- (id) tail;
- (void) addItemAtHead: (id) item;
- (void) addItemAtTail: (id) item;
- (id) removeItem: (id) item;
- (id) removeItemAtHead;
- (id) removeItemAtTail;
- (integer) count;
-(void)makeObjectsPerformSelector:(SEL)selector;
-(void)makeObjectsPerformSelector:(SEL)selector withObject:(id)arg;
@end

#endif//__ruamoko_List_h
