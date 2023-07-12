#ifndef __renderer_vulkan_vkgen_vkfieldcustom_h
#define __renderer_vulkan_vkgen_vkfieldcustom_h

#include "vkfielddef.h"

@interface CustomField: FieldDef
{
	string      pltype;
	string      parser;
	PLItem     *fields;
}
@end

#endif//__renderer_vulkan_vkgen_vkfieldcustom_h
