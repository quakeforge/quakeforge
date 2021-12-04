#ifndef __renderer_vulkan_vkgen_vkfixedarray_h
#define __renderer_vulkan_vkgen_vkfixedarray_h

#include <Object.h>

#include "vkgen.h"
#include "vktype.h"

@interface FixedArray: Type
{
	Type       *ele_type;
	int         ele_count;
}
-(void) writeTable;
-(void) writeSymtabInit;
-(void) writeSymtabEntry;
@end

#endif//__renderer_vulkan_vkgen_vkfixedarray_h
