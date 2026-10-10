#ifndef __renderer_vulkan_vkgen_vkflags64_h
#define __renderer_vulkan_vkgen_vkflags64_h

#include "vkenum.h"

@class Array;

@interface FlagBits64 : Object
{
	string name;
	ulong  value;
}
+(FlagBits64 *)flagbits:(string)name value:(ulong)value;
-(string)name;
-(ulong)value;
@end

@interface Flags64 : Enum
{
	Array       *members;
}
+(Type *)fromName:(string)name;
@end

#endif//__renderer_vulkan_vkgen_vkflags64_h
