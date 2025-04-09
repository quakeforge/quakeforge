#ifndef __ruamoko_PropertyList_h
#define __ruamoko_PropertyList_h

#include <plist.h>
#include <Object.h>

@class PLItem;

@protocol PLDictionary
- (int) count;
- (int) numKeys;
- (PLItem *) getObjectForKey:(string) key;
- (PLItem *) allKeys;
- (string) keyAtIndex:(int) index;
- addKey:(string) key value:(PLItem *) value;
@end

@protocol PLArray
- (int) count;
- (int) numObjects;
- (PLItem *) getObjectAtIndex:(int) index;
- addObject:(PLItem *) object;
- insertObject:(PLItem *) object atIndex:(int) index;
@end

@protocol PLString
- (string) string;
@end

@interface PLItem: Object <PLDictionary, PLArray, PLString>
{
	plitem_t   *item;
}
+ (PLItem *) newDictionary;
+ (PLItem *) newArray;
+ (PLItem *) newData:(void*) data size:(int) len;
+ (PLItem *) newString:(string) str;
+ (PLItem *) fromString:(string) str;
+ (PLItem *) fromFile:(QFile) file;

- initWithItem:(plitem_t *) item;
- (plitem_t *) item;
- (string) write;
- (pltype_t) type;
- (int) line;
@end

@interface PLDictionary: PLItem <PLDictionary>
+ (PLDictionary *) new;
+ (PLItem *) fromString:(string) str;
+ (PLItem *) fromFile:(QFile) file;
@end

@interface PLArray: PLItem <PLArray>
+ (PLArray *) new;
+ (PLItem *) fromString:(string) str;
+ (PLItem *) fromFile:(QFile) file;
@end

@interface PLData: PLItem
+ (PLData *) new:(void*) data size:(int) len;
@end

@interface PLString: PLItem <PLString>
+ (PLString *) new:(string) str;
@end

#endif//__ruamoko_PropertyList_h
