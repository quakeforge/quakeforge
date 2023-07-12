#ifndef __renderer_vulkan_vkgen_vkfieldignore_h
#define __renderer_vulkan_vkgen_vkfieldignore_h

#include "vkfielddef.h"

@class FieldType;

@interface IgnoreField: FieldDef
{
	FieldType *type;
}
@end

#endif//__renderer_vulkan_vkgen_vkfieldignore_h
