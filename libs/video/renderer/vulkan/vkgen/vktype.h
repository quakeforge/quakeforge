#ifndef __renderer_vulkan_vkgen_vktype_h
#define __renderer_vulkan_vkgen_vktype_h

#include <types.h>
#include <Object.h>

@interface Type: Object
{
	qfot_type_t *type;
}
-initWithType: (qfot_type_t *) type;
/**	\warning	returned string is ephemeral
*/
-(string) name;
-(void) addToQueue;
@end

#endif//__renderer_vulkan_vkgen_vktype_h
