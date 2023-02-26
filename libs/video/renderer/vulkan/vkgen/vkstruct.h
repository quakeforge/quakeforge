#ifndef __renderer_vulkan_vkgen_vkstruct_h
#define __renderer_vulkan_vkgen_vkstruct_h

#include <Object.h>

#include "vkgen.h"
#include "vktype.h"

@class PLItem;

@interface Struct: Type
{
	string outname;
	string label_field;
	int write_symtab;
	int skip;

	Array *field_defs;
	PLItem *field_dict;
	PLItem *only;
}
-(void) queueFieldTypes;
-(qfot_var_t *)findField:(string) fieldName;
-(void) setLabelField:(string) label_field;
-(void) writeForward;
-(void) writeTable;
-(void) writeSymtabInit;
-(void) writeSymtabEntry;
-(string) outname;
@end

#endif//__renderer_vulkan_vkgen_vkstruct_h
