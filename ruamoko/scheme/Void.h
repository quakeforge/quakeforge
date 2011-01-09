#ifndef __Void_h
#define __Void_h
#include "SchemeObject.h"

@interface Void: SchemeObject
{
}
+ (id) voidConstant;
@end

@extern Void []voidConstant;

#endif //__Void_h
