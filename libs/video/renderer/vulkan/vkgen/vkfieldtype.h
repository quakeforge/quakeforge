#ifndef __renderer_vulkan_vkgen_vkfieldtype_h
#define __renderer_vulkan_vkgen_vkfieldtype_h

#include <Object.h>

@class PLItem;

@interface FieldType: Object
{
	string parse_type;
	string type;
	string parser;
}
+fieldType:(PLItem *)item;
-initWithItem:(PLItem *)item;
-writeParseData;
-(string)parseType;
@end

#endif//__renderer_vulkan_vkgen_vkfieldtype_h
