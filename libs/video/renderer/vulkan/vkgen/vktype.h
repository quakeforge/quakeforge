#ifndef __renderer_vulkan_vkgen_vktype_h
#define __renderer_vulkan_vkgen_vktype_h

#include <types.h>
#include <Object.h>

@interface Type: Object
{
	qfot_type_t *type;
}
+fromType: (qfot_type_t *) type;
/**	\warning	returned string is ephemeral
*/
-(string) key;
/**	\warning	returned string is ephemeral
*/
-(string) name;
-(void) addToQueue;
-(Type *) resolveType;
+(Type *) findType: (qfot_type_t *) type;
+(Type *) lookup: (string) name;
-(string) parseType;
-(string) parseFunc;
-(string) parseData;

-(int) isPointer;
-(Type *) dereference;
@end

#endif//__renderer_vulkan_vkgen_vktype_h
