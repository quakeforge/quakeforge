#ifndef __ruamoko_PropertyList_h
#define __ruamoko_PropertyList_h

#include <plist.h>
#include <Object.h>

@interface PLItem: Object
{
	plitem_t    item;
	int     own;
}
+ (PLItem *) newDictionary;
+ (PLItem *) newArray;
+ (PLItem *) newData:(void*) data size:(int) len;
+ (PLItem *) newString:(string) str;
+ (PLItem *) fromString:(string) str;
+ (PLItem *) fromFile:(QFile) file;

- initWithItem:(plitem_t) item;
- initWithOwnItem:(plitem_t) item;
- (string) write;
- (pltype_t) type;
@end

@interface PLDictionary: PLItem
+ (PLDictionary *) new;

- (int) count;
- (int) numKeys;
- (PLItem *) getObjectForKey:(string) key;
- (PLItem *) allKeys;
- addKey:(string) key value:(PLItem *) value;
@end

@interface PLArray: PLItem
+ (PLArray *) new;

- (int) count;
- (int) numObjects;
- (PLItem *) getObjectAtIndex:(int) index;
- addObject:(PLItem *) object;
- insertObject:(PLItem *) object atIndex:(int) index;
@end

@interface PLData: PLItem
+ (PLData *) new:(void*) data size:(int) len;
@end

@interface PLString: PLItem
+ (PLString *) new:(string) str;

- (string) string;
@end

#endif//__ruamoko_PropertyList_h
