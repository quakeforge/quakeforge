#ifndef __renderer_vulkan_vkgen_vkfieldreadonly_h
#define __renderer_vulkan_vkgen_vkfieldreadonly_h

#include "vkfielddef.h"

@class FieldType;

@interface ReadOnlyField: FieldDef
{
	FieldType *type;
}
@end

#endif//__renderer_vulkan_vkgen_vkfieldreadonly_h
