#ifndef __ruamoko_PropertyList_h
#define __ruamoko_PropertyList_h

#include "plist.h"
#include "Object.h"

@interface PLItem: Object
{
	plitem_t    item;
	integer     own;
}
+ (PLItem) newDictionary;
+ (PLItem) newArray;
+ (PLItem) newData:(void[]) data size:(integer) len;
+ (PLItem) newString:(string) str;
+ (PLItem) newFromString:(string) str;

-initWithItem:(plitem_t) item;
-initWithOwnItem:(plitem_t) item;
- (string) write;
- (pltype_t) type;
@end

@interface PLDictionary: PLItem
+ (PLDictionary) new;

- (integer) count;
- (integer) numKeys;
- (PLItem) getObjectForKey:(string) key;
- (PLItem) allKeys;
- addKey:(PLItem) key value:(PLItem) value;
@end

@interface PLArray: PLItem
+ (PLArray) new;

- (integer) count;
- (integer) numObjects;
- (PLItem) getObjectAtIndex:(integer) index;
- addObject:(PLItem) object;
- insertObject:(PLItem) object atIndex:(integer) index;
@end

@interface PLData: PLItem
+ (PLData) new:(void[]) data size:(integer) len;
@end

@interface PLString: PLItem
+ (PLString) new:(string) str;

- (string) string;
@end

#endif//__ruamoko_PropertyList_h
