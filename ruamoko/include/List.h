#ifndef __ruamoko_List_h
#define __ruamoko_List_h

#include "Object.h"

struct list_bucket_t = {
	list_bucket_t [] next;
	list_bucket_t [][] prev;
	id obj;
};

@interface List: Object
{
	integer count;
	list_bucket_t [] head;
	list_bucket_t [][] tail;
}
- (id) init;
- (void) free;
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
