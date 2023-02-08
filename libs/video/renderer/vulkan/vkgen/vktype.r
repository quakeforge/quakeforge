#include <hash.h>

#include "vkalias.h"
#include "vkenum.h"
#include "vkfixedarray.h"
#include "vkgen.h"
#include "vkstruct.h"
#include "vktype.h"

@implementation Type

static hashtab_t *registered_types;

static string get_type_key (void *type, void *unused)
{
	return ((Type *) type).type.encoding;
}

+(void)initialize
{
	registered_types = Hash_NewTable (127, get_type_key, nil, nil);
}

-initWithType: (qfot_type_t *) type
{
	if (!(self = [super init])) {
		return nil;
	}
	self.type = type;
	Hash_Add (registered_types, self);
	return self;
}

+(Type *) findType: (qfot_type_t *) type
{
	if (type.meta == ty_alias && !type.alias.name) {
		type = type.alias.full_type;
	}
	return (Type *) Hash_Find (registered_types, type.encoding);
}

+(Type *) lookup: (string) name
{
	return (Type *) Hash_Find (available_types, name);
}

+fromType: (qfot_type_t *) type
{
	if (type.size == 0) {
		return nil;
	}
	switch (type.meta) {
		case ty_basic:
		case ty_class:
			return [[Type alloc] initWithType: type];
		case ty_array:
			return [[FixedArray alloc] initWithType: type];
		case ty_enum:
			return [[Enum alloc] initWithType: type];
		case ty_struct:
		case ty_union:
			return [[Struct alloc] initWithType: type];
		case ty_alias:
			if (type.alias.name) {
				return [[Alias alloc] initWithType: type];
			}
			return [Type fromType: type.alias.full_type];
	}
	return nil;
}

-(string) key
{
	return type.encoding;
}

-(string) name
{
	if (type.meta == ty_basic) {
		if (type.type == ev_int) {
			return "int";
		}
		return pr_type_name[type.type];
	}
	//FIXME extract alias name and return proper type name
	return type.encoding;
}

-(void) setAlias: (Type *) alias
{
	if (!self.alias) {
		self.alias = alias;
	}
}

-(void) addToQueue
{
	string name = [self name];
	if (type.meta == ty_basic && type.type == ev_ptr) {
		[[Type findType: type.fldptr.aux_type] addToQueue];
	}
}

-(Type *) resolveType
{
	return self;
}

-(string) cexprType
{
	return "cexpr_" + [self name];
}

-(string) parseType
{
	if (type.meta == ty_basic) {
		return "QFString";
	}
	return "no parse " + [self name];
}

-(string) parseFunc
{
	if (type.meta == ty_basic) {
		return "parse_basic";
	}
	return "0";
}

-(string) parseData
{
	if (type.meta == ty_basic) {
		if (type.type == ev_int) {
			return "&cexpr_int";
		}
		return "&cexpr_" + pr_type_name[type.type];
	}
	return "0";
}

-(int) isPointer
{
	if ((type.meta == ty_basic || type.meta == ty_alias)
		&& type.type == ev_ptr) {
		return 1;
	}
	return 0;
}

-(Type *) dereference
{
	qfot_type_t *t = type;
	if (t.meta == ty_alias && t.type == ev_ptr) {
		t = type.alias.full_type;
	}
	if (t.meta == ty_basic && t.type == ev_ptr) {
		t = type.fldptr.aux_type;
	}
	return [Type findType:t];
}

@end
