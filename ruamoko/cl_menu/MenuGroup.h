#ifndef __MenuGroup_h
#define __MenuGroup_h

#include "gui/Group.h"

/**	A group of views for use as a menu.

	A menu may consist of decorations and actual menu items. For correct
	results, the decoration views must be added before the menu item views.
*/
@interface MenuGroup : Group
{
	unsigned base;				///< The index of the first menu item.
	unsigned current;			///< The currently selected menu item.
}

/**	Set the index of the first menu item.

	\param	b	The index of the first menu item.
*/
-(void) setBase: (unsigned) b;

/**	Select the next menu item.

	Wraps back to the base menu item if the current menu item is the last.
*/
-(void) next;

/**	Select the previous menu item.

	Wraps to the last menu item if the current menu item is the base item.
*/
-(void) prev;
@end

#endif//__MenuGroup_h
