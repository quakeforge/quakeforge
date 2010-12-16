#ifndef __MenuGroup_h
#define __MenuGroup_h

#include "gui/Group.h"

@interface MenuGroup : Group
{
	integer base;
	integer current;
}
-(void)setBase:(integer)b;
-(void) next;
-(void) prev;
@end

#endif//__MenuGroup_h
