#ifndef __Number_h
#define __Number_h
#include "SchemeObject.h"

@interface Number: SchemeObject
{
    integer value;
}
+ (id) newFromInt: (integer) i;
- (id) initWithInt: (integer) i;
- (integer) intValue;
- (string) printForm;
@end

#endif //__Number_h
