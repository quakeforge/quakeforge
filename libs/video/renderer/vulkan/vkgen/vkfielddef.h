#ifndef __renderer_vulkan_vkgen_vkfielddef_h
#define __renderer_vulkan_vkgen_vkfielddef_h

#include <types.h>
#include <Object.h>

@class PLItem;
@class Struct;
@class Type;

@interface FieldDef: Object
{
	int line;
	qfot_var_t *field;
	string struct_name;
	string field_name;
	string value_field;
	string size_field;
}
+fielddef:(PLItem *)item struct:(Struct *)strct field:(string)fname;
-init:(PLItem *)item struct:(Struct *)strct field:(string)fname;
-writeParseData;
-writeField;
-writeSymbol;
-(string) name;
-(int) searchType;
@end

#endif//__renderer_vulkan_vkgen_vkfielddef_h
