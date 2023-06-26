#include <PropertyList.h>

@implementation PLItem

+ (PLItem *) newDictionary
{
	return [PLDictionary new];
}

+ (PLItem *) newArray
{
	return [PLArray new];
}

+ (PLItem *) newData:(void*) data size:(int) len
{
	return [PLData new:data size:len];
}

+ (PLItem *) newString:(string) str
{
	return [PLString new:str];
}

+ itemClass:(plitem_t *) item
{
	local string classname = nil;
	local id class;

	if (!item)
		return nil;
	switch (PL_Type (item)) {
	case QFDictionary:
		classname = "PLDictionary";
		break;
	case QFArray:
		classname = "PLArray";
		break;
	case QFBinary:
		classname = "PLData";
		break;
	case QFString:
		classname = "PLString";
		break;
	default:
		return nil;
	}
	class = obj_lookup_class (classname);
	return [[class alloc] initWithItem: item];
}

+ (PLItem *) fromString:(string) str
{
	return [[PLItem itemClass: PL_GetPropertyList (str)] autorelease];
}

+ (PLItem *) fromFile:(QFile) file
{
	return [[PLItem itemClass: PL_GetFromFile (file)] autorelease];
}

- initWithItem:(plitem_t *) item
{
	if (!(self = [super init]))
		return self;
	PL_Retain (item);
	self.item = item;
	return self;
}

- (void) dealloc
{
	PL_Release (item);
	[super dealloc];
}

- (plitem_t *) item
{
	return item;
}

- (string) write
{
	return PL_WritePropertyList (item);
}

- (pltype_t) type
{
	return PL_Type (item);
}

- (int) line
{
	return PL_Line (item);
}

- (int) count
{
	if ([self class] == [PLDictionary class]) {
		return PL_D_NumKeys (item);
	} else {
		return PL_A_NumObjects (item);
	}
}

- (int) numKeys
{
	return PL_D_NumKeys (item);
}

- (PLItem *) getObjectForKey:(string) key
{
	return [[PLItem itemClass: PL_ObjectForKey (item, key)] autorelease];
}

- (PLItem *) allKeys
{
	return [[PLItem itemClass: PL_D_AllKeys (item)] autorelease];
}

- (string) keyAtIndex:(int) index
{
	return PL_KeyAtIndex (item, index);
}

- addKey:(string) key value:(PLItem *) value
{
	PL_D_AddObject (item, key, value.item);
	return self;
}

- (int) numObjects
{
	return PL_A_NumObjects (item);
}

- (PLItem *) getObjectAtIndex:(int) index
{
	return [[PLItem itemClass: PL_ObjectAtIndex (item, index)] autorelease];
}

- addObject:(PLItem *) object
{
	PL_A_AddObject (item, object.item);
	return self;
}

- insertObject:(PLItem *) object atIndex:(int) index
{
	PL_A_InsertObjectAtIndex (item, object.item, index);
	return self;
}

- (string) string
{
	return PL_String (item);
}

@end


@implementation PLDictionary

+ (PLDictionary *) new
{
	return [[PLDictionary alloc] initWithItem: PL_NewDictionary ()];
}

+ (PLItem *) fromString:(string) str
{
	return [[PLItem itemClass: PL_GetDictionary (str)] autorelease];
}

+ (PLItem *) fromFile:(QFile) file
{
	return [[PLItem itemClass: PL_GetDictionaryFromFile (file)] autorelease];
}

- (int) count
{
	return PL_D_NumKeys (item);
}

- (int) numKeys
{
	return PL_D_NumKeys (item);
}

- (PLItem *) getObjectForKey:(string) key
{
	return [[PLItem itemClass: PL_ObjectForKey (item, key)] autorelease];
}

- (PLItem *) allKeys
{
	return [[PLItem itemClass: PL_D_AllKeys (item)] autorelease];
}

- addKey:(string) key value:(PLItem *) value
{
	PL_D_AddObject (item, key, value.item);
	return self;
}

@end

@implementation PLArray

+ (PLArray *) new
{
	return [[PLArray alloc] initWithItem: PL_NewArray ()];
}

+ (PLItem *) fromString:(string) str
{
	return [[PLItem itemClass: PL_GetArray (str)] autorelease];
}

+ (PLItem *) fromFile:(QFile) file
{
	return [[PLItem itemClass: PL_GetArrayFromFile (file)] autorelease];
}

- (int) count
{
	return PL_A_NumObjects (item);
}

- (int) numObjects
{
	return PL_A_NumObjects (item);
}

- (PLItem *) getObjectAtIndex:(int) index
{
	return [[PLItem itemClass: PL_ObjectAtIndex (item, index)] autorelease];
}

- addObject:(PLItem *) object
{
	PL_A_AddObject (item, object.item);
	return self;
}

- insertObject:(PLItem *) object atIndex:(int) index
{
	PL_A_InsertObjectAtIndex (item, object.item, index);
	return self;
}

@end

@implementation PLData

+ (PLData *) new:(void*) data size:(int) len
{
	return [[PLData alloc] initWithItem: PL_NewData (data, len)];
}

@end

@implementation PLString

+ (PLString *) new: (string) str
{
	return [[PLString alloc] initWithItem: PL_NewString (str)];
}

- (string) string
{
	return PL_String (item);
}

@end
