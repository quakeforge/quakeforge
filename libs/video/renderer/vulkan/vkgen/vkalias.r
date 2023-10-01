#include <string.h>

#include "vkalias.h"
#include "vkenum.h"
#include "vkgen.h"
#include "vkstruct.h"

@implementation Alias
-initWithType: (qfot_type_t *) type
{
	if (!(self = [super initWithType: type])) {
		return nil;
	}
	[[self resolveType] setAlias: self];
	return self;
}

-(string) name
{
	return type.alias.name;
}

-(Type *) resolveType
{
	return [Type findType: type.alias.aux_type];
}

-(void)addToQueue
{
	Type       *alias = [Type findType:type.alias.full_type];
	string      name = [self name];

	if ([alias name] == "VkFlags") {
		if (str_mid (name, -5) == "Flags") {
			string tag = str_mid (name, 0, -1) + "Bits";
			id enumObj = [(id) Hash_Find (available_types, tag) resolveType];
			[enumObj addToQueue];
		}
	} else if (name == "VkBool32") {
		id enumObj = [(id) Hash_Find (available_types, name) resolveType];
		[enumObj addToQueue];
	} else if ([alias class] == [Enum class]
			   || [alias class] == [Struct class]) {
		[alias addToQueue];
	} else if (alias.type.meta == ty_basic && alias.type.type == ev_ptr) {
		Type       *type = [Type findType:alias.type.fldptr.aux_type];
		if (!type) {
			// pointer to opaque struct. Probably
			// VK_DEFINE_NON_DISPATCHABLE_HANDLE or VK_DEFINE_HANDLE
			//string createInfo = name + "CreateInfo";
			//id structObj = (id) Hash_Find (available_types, createInfo);
			//if (structObj) {
			//	[structObj addToQueue];
			//}
		} else if ([type class] == [Alias class]) {
			type = [type resolveType];
			if ([type class] == [Struct class]) {
				[type addToQueue];
			}
		}
	}
}

-(string) cexprType
{
	Type       *alias = [Type findType:type.alias.full_type];
	string      name = [self name];

	if ([alias name] == "VkFlags") {
		if (str_mid (name, -5) == "Flags") {
			string tag = str_mid (name, 0, -1) + "Bits";
			id enumObj = [(id) Hash_Find (available_types, tag) resolveType];
			return [enumObj cexprType];
		}
	}
	if (name == "VkBool32") {
		id enumObj = [(id) Hash_Find (available_types, name) resolveType];
		return [enumObj cexprType];
	}
	if (name == "bool") {
		return "cexpr_bool";
	}
	if (name == "int32_t") {
		return "cexpr_int";
	}
	if (name == "uint32_t") {
		return "cexpr_uint";
	}
	if (name == "vec4f_t") {
		return "cexpr_vector";
	}
	if (name == "size_t") {
		return "cexpr_size_t";
	}
	return [alias cexprType];
}

-(string) parseType
{
	Type       *alias = [Type findType:type.alias.full_type];
	string      name = [self name];

	if ([alias name] == "VkFlags") {
		if (str_mid (name, -5) == "Flags") {
			string tag = str_mid (name, 0, -1) + "Bits";
			id enumObj = [(id) Hash_Find (available_types, tag) resolveType];
			return [enumObj parseType];
		}
	}
	switch (name) {
		case "VkBool32":
			id enumObj = [(id) Hash_Find (available_types, name) resolveType];
			return [enumObj parseType];
		case "int32_t":
		case "uint32_t":
		case "size_t":
		case "vec4f_t":
		case "bool":
			return "QFString";
	}
	return [alias parseType];
}

-(string) parseFunc
{
	Type       *alias = [Type findType:type.alias.full_type];
	string      name = [self name];

	if ([alias name] == "VkFlags") {
		if (str_mid (name, -5) == "Flags") {
			string tag = str_mid (name, 0, -1) + "Bits";
			id enumObj = [(id) Hash_Find (available_types, tag) resolveType];
			return [enumObj parseFunc];
		}
	}
	switch (name) {
		case "VkBool32":
			id enumObj = [(id) Hash_Find (available_types, name) resolveType];
			return [enumObj parseFunc];
		case "bool":
			return "parse_enum";
		case "int32_t":
			return "parse_int32_t";
		case "uint32_t":
			return "parse_uint32_t";
	}
	return [alias parseFunc];
}

-(string) parseData
{
	Type       *alias = [Type findType:type.alias.full_type];
	string      name = [self name];

	if ([alias name] == "VkFlags") {
		if (str_mid (name, -5) == "Flags") {
			string tag = str_mid (name, 0, -1) + "Bits";
			id enumObj = [(id) Hash_Find (available_types, tag) resolveType];
			return [enumObj parseData];
		}
	}
	switch (name) {
		case "VkBool32":
			id enumObj = [(id) Hash_Find (available_types, name) resolveType];
			return [enumObj parseData];
		case "int32_t":
		case "uint32_t":
			return "0";
		case "bool":
			return "&cexpr_bool_enum";
		case "vec4f_t":
			return "&cexpr_vector";
		case "size_t":
			return "&cexpr_size_t";
	}
	return [alias parseData];
}
@end
