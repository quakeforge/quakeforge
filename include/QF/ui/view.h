/*
	view.h

	console view object

	Copyright (C) 2003 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2003/5/5

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/

#ifndef __QF_ui_view_h
#define __QF_ui_view_h

/** \defgroup console_view Console View Objects
	\ingroup console
*/
///@{

/** Control the positioning of a view within its parent. The directions are
	the standard compass rose (north, east, south, west in clockwise order)
	with north at the top.

	The origin of the view is taken to be the corresponding point on the edge
	of the view (eg, southeast is bottom right), or the view's center for
	center gravity. When the relative coordinates of the view are (0,0), the
	view's origin is placed on the parent view's gravity point using the view's
	gravity (\em not the parent view's gravity). That is, the parent view's
	gravity has no effect on the view's position within the parent view.

	The gravity also affects the direction the view moves as the relative
	coordinates of the view change.

	No checking is done to ensure the view stays within the parent, or that the
	view is smaller than the parent. This is by design. It is up to the drawing
	callbacks to do any necessary clipping.
*/
typedef enum {
	grav_center,	///< +ve X right, +ve Y down, -X left,  -ve Y up
	grav_north,		///< +ve X right, +ve Y down, -X left,  -ve Y up
	grav_northeast,	///< +ve X left,  +ve Y down, -X right, -ve Y up
	grav_east,		///< +ve X left,  +ve Y down, -X right, -ve Y up
	grav_southeast,	///< +ve X left,  +ve Y up,   -X right, -ve Y down
	grav_south,		///< +ve X right, +ve Y up,   -X left,  -ve Y down
	grav_southwest,	///< +ve X right, +ve Y up,   -X left,  -ve Y down
	grav_west,		///< +ve X right, +ve Y down, -X left,  -ve Y up
	grav_northwest,	///< +ve X right, +ve Y down, -X left,  -ve Y up
} grav_t;

extern struct exprtype_s grav_t_type;

/** The view object.
*/
typedef struct view_s view_t;
struct view_s {
	/// Coordinates of view's origin relative to parent's gravity point.
	//@{
	int     xpos, ypos;
	//@}
	/// Size of the view.
	//@{
	int     xlen, ylen;
	//@}
	/** Absolute coordinates of the top left (northwest) corner of the view.
		Set interally.
	*/
	//@{
	int     xabs, yabs;
	//@}
	/** Coordinates of the top left (northwest) corner of the view relative to
		the parent view's top left corner. Set internally.
	*/
	//@{
	int     xrel, yrel;
	//@}
	grav_t  gravity;			///< The gravity of the view.
	view_t *parent;				///< The parent view.
	view_t **children;			///< The child views.
	int     num_children;		///< Number of child views in view.
	int     max_children;		///< Size of children array.
	/** Callback for drawing the view. defaults to view_draw(). if overridden,
		the supplied callback should call view_draw() to draw any child views
		unless the view is a leaf view.

		\note All coordinates are set appropriately before this is called.

		\param view		This view.
	*/
	void    (*draw)(view_t *view);
	/** Callback for when the position and/or size of the view changes. Set
		this if the underlying drawing system needs to take any action when
		the view's geometry changes (eg, moving/resizing the window in curses).

		\note All coordinates are set appropriately before this is called.

		\param view		This view.
	*/
	void    (*setgeometry)(view_t *view);
	/** User supplied data. Purely for external use. The view functions do not
		touch this at all except view_new(), which just sets it to 0.
	*/
	void   *data;
	unsigned visible:1;			///< If false, view_draw() skips this view.
	unsigned resize_x:1;		///< If true, view's width follows parent's.
	unsigned resize_y:1;		///< If true, view's height follows parent's.
};

/** Create a new view. view_t::draw is set to view_draw() and the view is made
	visible. All coordinates are set appropriately for the new view being a
	root view. All other fields not set by the parameters are 0.

	\param xp		The X coordinate of the view's origin.
	\param yp		The Y coordinate of the view's origin.
	\param xl		The width of the view.
	\param yl		The height of the view.
	\param grav		The gravity of the view. determines the view's origin and
					its positioning within the view's parent.
*/
view_t *view_new (int xp, int yp, int xl, int yl, grav_t grav);

/** Insert a view into a parent view at the specified location. If \c pos is
	negative, it is taken to be relative to the end of the parent's list of
	views (view_insert (par, view, -1) is equivalent to view_add (par, view)).
	\c pos is clipped to be within the correct range.

	The absolute X and Y coordianates of the inserted view and its child views
	are updated based on the view's gravity.

	The position of the view within the parent view's child view list
	determines the draw order (and thus Z order) of the view, with position 0
	being drawn first.

	\param par		The parent view into which the view is to be inserted.
	\param view		The view to insert.
	\param pos		The position at which to insert the view into the parent.
*/
void view_insert (view_t *par, view_t *view, int pos);

/** Add a view to a parent view at the end of the parents view list.

	The absolute X and Y coordianates of the inserted view and its child views
	are updated based on the view's gravity.

	The added view will be drawn last (and thus on top of earlier views).

	\param par		The parent view to which the view is to be added.
	\param view		The view to add.
*/
void view_add (view_t *par, view_t *view);

/** Remove a view from its parent.

	\param par		The parent view from which the view is to be removed.
	\param view		The view to remove.
*/
void view_remove (view_t *par, view_t *view);

/** Delete a view and all its child views. If the view has a parent, the view
	will be removed from its parent.

	\param view		The view to delete.
*/
void view_delete (view_t *view);

/** Draw the child views of a view. If a child view is not visible
	(view_t::visible is 0), the child will be skipped.

	\note	It is best to always use view_t::draw() to draw a view rather than
			calling this directly. This function is really for the view's draw
			callback to call to draw its sub-views.

	\param view		The view of which to draw the children.
*/
void view_draw (view_t *view);

/** Change the size of a view. The view's children are also resized based on
	their view_t::resize_x and view_t::resize_y flags.

	The absolute X and Y coorinates of the view are updated as necessary to
	keep the coordinates of the view's origin correct relative to the view's
	geometry.

	\param view		The view to resize.
	\param xl		The new width of the view.
	\param yl		The new height of the view.
*/
void view_resize (view_t *view, int xl, int yl);

/** Chage the location of a view.

	The absolute X and Y coorinates of the view are updated as necessary to
	keep the coordinates of the view's origin correct relative to the view's
	geometry.

	\param view		The view to move.
	\param xp		The new X coordinate of the view relative to its gravity.
	\param yp		The new Y coordinate of the view relative to its gravity.
*/
void view_move (view_t *view, int xp, int yp);

/** Chage the location and size of a view in a single operation.

	The absolute X and Y coorinates of the view are updated as necessary to
	keep the coordinates of the view's origin correct relative to the view's
	geometry.

	\param view		The view to move.
	\param xp		The new X coordinate of the view relative to its gravity.
	\param yp		The new Y coordinate of the view relative to its gravity.
	\param xl		The new width of the view.
	\param yl		The new height of the view.
*/
void view_setgeometry (view_t *view, int xp, int yp, int xl, int yl);

/** Change the gravity of a view, adjusting its position appropriately

	\param view		The view which will have its gravity set..
	\param grav		The gravity of the view. determines the view's origin and
					its positioning within the view's parent.
*/
void view_setgravity (view_t *view, grav_t grav);

///@}

#endif//__QF_ui_view_h
