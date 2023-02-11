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
	self.item = item;
	own = 0;
	return self;
}

-initWithOwnItem:(plitem_t *) item
{
	if (!(self = [super init]))
		return self;
	self.item = item;
	own = 1;
	return self;
}

- (void) dealloc
{
	if (own)
		PL_Free (item);
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
	if (!value.own) {
		obj_error (self, 0, "add of unowned key/value to PLDictionary");
		return self;
	}
	PL_D_AddObject (item, key, value.item);
	value.own = 0;
	[value release];
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
	if (!object.own) {
		obj_error (self, 0, "add of unowned object to PLArray");
		return self;
	}
	PL_A_AddObject (item, object.item);
	object.own = 0;
	[object release];
	return self;
}

- insertObject:(PLItem *) object atIndex:(int) index
{
	if (!object.own) {
		obj_error (self, 0, "add of unowned object to PLArray");
		return self;
	}
	PL_A_InsertObjectAtIndex (item, object.item, index);
	object.own = 0;
	[object release];
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
	return [[PLDictionary alloc] initWithOwnItem: PL_NewDictionary ()];
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
	if (!value.own) {
		obj_error (self, 0, "add of unowned key/value to PLDictionary");
		return self;
	}
	PL_D_AddObject (item, key, value.item);
	value.own = 0;
	[value release];
	return self;
}

@end

@implementation PLArray

+ (PLArray *) new
{
	return [[PLArray alloc] initWithOwnItem: PL_NewArray ()];
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
	if (!object.own) {
		obj_error (self, 0, "add of unowned object to PLArray");
		return self;
	}
	PL_A_AddObject (item, object.item);
	object.own = 0;
	[object release];
	return self;
}

- insertObject:(PLItem *) object atIndex:(int) index
{
	if (!object.own) {
		obj_error (self, 0, "add of unowned object to PLArray");
		return self;
	}
	PL_A_InsertObjectAtIndex (item, object.item, index);
	object.own = 0;
	[object release];
	return self;
}

@end

@implementation PLData

+ (PLData *) new:(void*) data size:(int) len
{
	return [[PLData alloc] initWithOwnItem: PL_NewData (data, len)];
}

@end

@implementation PLString

+ (PLString *) new: (string) str
{
	return [[PLString alloc] initWithOwnItem: PL_NewString (str)];
}

- (string) string
{
	return PL_String (item);
}

@end
