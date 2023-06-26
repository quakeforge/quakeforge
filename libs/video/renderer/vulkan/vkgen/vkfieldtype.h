#ifndef __renderer_vulkan_vkgen_vkfieldtype_h
#define __renderer_vulkan_vkgen_vkfieldtype_h

#include <Object.h>

@class PLItem;

@interface FieldType: Object
{
	string parse_type;
	string type;
	string parser;
	string data;
}
+fieldType:(PLItem *)item;
-initWithItem:(PLItem *)item;
-writeParseData;
-(string)type;
-(string)exprType;
-(string)parseType;
+(string)anyType;
@end

string parseItemType (PLItem *item);

#endif//__renderer_vulkan_vkgen_vkfieldtype_h
