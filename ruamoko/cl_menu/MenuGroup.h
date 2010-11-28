#ifndef __MenuGroup_h
#define __MenuGroup_h

#include "gui/Group.h"

@interface MenuGroup : Group
{
	integer base;
	integer current;
}
-(void)setBase:(integer)b;
@end

#endif//__MenuGroup_h
