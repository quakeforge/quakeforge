#ifndef __renderer_vulkan_vkgen_vkfieldarray_h
#define __renderer_vulkan_vkgen_vkfieldarray_h

#include "vkfielddef.h"

@class FieldType;

@interface ArrayField: FieldDef
{
	FieldType *type;
}
@end

#endif//__renderer_vulkan_vkgen_vkfieldarray_h
