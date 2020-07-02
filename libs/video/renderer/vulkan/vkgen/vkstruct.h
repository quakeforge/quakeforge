#ifndef __renderer_vulkan_vkgen_vkstruct_h
#define __renderer_vulkan_vkgen_vkstruct_h

#include <Object.h>

#include "vkgen.h"
#include "vktype.h"

@interface Struct: Type
{
}
-(void) forEachFieldCall: (varfunc) func;
-(void) writeTable;
@end

#endif//__renderer_vulkan_vkgen_vkstruct_h
