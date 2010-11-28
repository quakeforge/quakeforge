#ifndef __CvarObject_h
#define __CvarObject_h

#include "Object.h"

@interface CvarObject : Object
{
	string name;
}
-(id)initWithCvar:(string)cvname;
@end

#endif//__CvarObject_h
