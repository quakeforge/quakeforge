#ifndef __MenuGroup_h
#define __MenuGroup_h

#include "gui/Group.h"

@interface MenuGroup : Group
{
	int base;
	int current;
}
-(void)setBase:(int)b;
-(void) next;
-(void) prev;
@end

#endif//__MenuGroup_h
