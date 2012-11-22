#include <Object.h>

@implementation Object
- (void) doesNotRecognizeSelector: (SEL)aSelector
{
    [self error: "%s does not recognize %s",
		object_get_class_name (self),
		sel_get_name (aSelector)];
}
@end
