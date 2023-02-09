#ifndef __renderer_vulkan_vkgen_vktype_h
#define __renderer_vulkan_vkgen_vktype_h

#include <types.h>
#include <Object.h>

@class FieldDef;
@class Struct;

@interface Type: Object
{
	qfot_type_t *type;
	Type       *alias;
}
+fromType: (qfot_type_t *) type;
/**	\warning	returned string is ephemeral
*/
-(string) key;
/**	\warning	returned string is ephemeral
*/
-(string) name;
-(void) setAlias: (Type *) alias;
-(void) addToQueue;
-(Type *) resolveType;
+(Type *) findType: (qfot_type_t *) type;
+(Type *) lookup: (string) name;
-(string) cexprType;
-(string) parseType;
-(string) parseFunc;
-(string) parseData;

-(FieldDef *)fielddef:(Struct *)strct field:(string)fname;

-(int) isPointer;
-(Type *) dereference;
@end

#endif//__renderer_vulkan_vkgen_vktype_h
