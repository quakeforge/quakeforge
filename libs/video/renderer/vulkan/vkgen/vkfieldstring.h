#ifndef __renderer_vulkan_vkgen_vkfieldstring_h
#define __renderer_vulkan_vkgen_vkfieldstring_h

#include "vkfielddef.h"

@interface StringField: FieldDef
+fielddef:(PLItem *)item struct:(Struct *)strct field:(string)fname;
@end

#endif//__renderer_vulkan_vkgen_vkfieldstring_h
