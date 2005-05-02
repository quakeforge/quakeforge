#include "Nil.h"
#include "defs.h"

Nil one_nil_to_rule_them_all;

@implementation Nil

+ (void) initialize
{
    one_nil_to_rule_them_all = [Nil new];
    [one_nil_to_rule_them_all makeRootCell];
}

+ (id) nil
{
    return one_nil_to_rule_them_all;
}

- (string) printForm
{
    return "()";
}

@end
