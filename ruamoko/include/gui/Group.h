#ifndef __ruamoko_gui_Group_h
#define __ruamoko_gui_Group_h

#include <gui/View.h>

/**	\addtogroup gui */
///@{

@class Array;

/** A group of logically realted views.

	The sub-views are all positioned relative to the group's origin.
	Sub-views may be other groups.

	The order in which views are added determines the draw and event handling
	order.

	\todo Events are not handled.
*/
@interface Group : View
{
	Array *views;
}

/**	Add a view to the group.

	\param	aView	The view to be added.
	\return			The added view.
*/
- (View*) addView: (View*)aView;

/**	Add an array of views to the group.

	The views will be appened to any already existing sub-views, maintaining
	the order of the views in the array.

	\param	viewlist The array of views to be added.
	\return			self
*/
- (id) addViews: (Array*)viewlist;
@end

///@}

#endif//__ruamoko_gui_Group_h
