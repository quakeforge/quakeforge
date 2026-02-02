#include "ui_object.h"

@implementation UI_Object

-initWithContext:(imui_ctx_t)ctx
{
	if (!(self = [super init])) {
		return nil;
	}
	IMUI_context = ctx;
	return self;
}

@end
