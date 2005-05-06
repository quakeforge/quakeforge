#include "Boolean.h"

Boolean trueConstant;
Boolean falseConstant;

@implementation Boolean

+ (void) initialize
{
    trueConstant = [Boolean new];
    [trueConstant makeRootCell];
    falseConstant = [Boolean new];
    [falseConstant makeRootCell];
    
}

+ (id) trueConstant
{
    return trueConstant;
}

+ (id) falseConstant
{
    return falseConstant;
}

- (string) printForm
{
    return self == trueConstant ? "#t" : "#f";
}

@end
