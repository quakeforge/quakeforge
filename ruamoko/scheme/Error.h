#ifndef __Error_h
#define __Error_h
#include "SchemeObject.h"

@interface Error: SchemeObject
{
    string type, message;
}
+ (id) type: (string) t message: (string) m by: (SchemeObject *) o;
+ (id) type: (string) t message: (string) m;
- (id) initWithType: (string) t message: (string) m by: (SchemeObject *) o;
- (string) type;
- (string) message;

@end

#endif //__Error_h
