#ifndef __renderer_vulkan_vkgen_vkenum_h
#define __renderer_vulkan_vkgen_vkenum_h

#include "vktype.h"

@interface Enum: Type
{
	int          prefix_length;
}
-(void) writeTable;
@end

#endif//__renderer_vulkan_vkgen_vkenum_h
