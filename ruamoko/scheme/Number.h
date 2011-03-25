#ifndef __Number_h
#define __Number_h
#include "SchemeObject.h"

@interface Number: SchemeObject
{
    int value;
}
+ (id) newFromInt: (int) i;
- (id) initWithInt: (int) i;
- (int) intValue;
- (string) printForm;
@end

#endif //__Number_h
