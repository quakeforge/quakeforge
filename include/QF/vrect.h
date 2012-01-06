/*
	vrect.h

	Rectangle manipulation

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/1/6

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

	$Id$
*/

#ifndef __QF_vrect_h
#define __QF_vrect_h

typedef struct vrect_s {
	int         x;
	int         y;
	int         width;
	int         height;
	struct vrect_s *next;
} vrect_t;

#define VRect_MinX(vr) ((vr)->x)
#define VRect_MinY(vr) ((vr)->y)
#define VRect_MaxX(vr) ((vr)->x + (vr)->width)
#define VRect_MaxY(vr) ((vr)->y + (vr)->height)
#define VRect_IsEmpty(vr) ((vr)->width <= 0 || (vr)->height <= 0)

/** Create a new rectangle of the specified shape.

	Other VRect functions will return a rectangle (or chain of rectangles)
	created by this function.

	\param x		The X coordinate of the top-left corner.
	\param y		The Y coordinate of the top-left corner.
	\param width	The width of the rectangle.
	\param height	The height of the rectangle.
	\return			The newly created rectangle.
*/
vrect_t *VRect_New (int x, int y, int width, int height);

/** Free one rectangle.

	This function will not free any other rectangles linked to the specified
	rectangle.

	\param rect		The rectangle to be freed.
*/
void VRect_Delete (vrect_t *rect);

/** Return a rectangle representing the intersection of \a r1 and \a r2.

	The returned rectangle may be empty. Use VRect_IsEmpty to check.

	\param r1		The first rectangle.
	\param r2		The second rectangle.
	\return			The single (possibly empty) rectangle representing the
					intersection of \a r1 and \a r2.
	\note	It is the caller's responsibility to delete the returned rectangle.
*/
vrect_t *VRect_Intersect (const vrect_t *r1, const vrect_t *r2);

/** Return two rectangles representing the portions of \a r above and below
	\a y.

	One of the returned rectangles may be empty. Use VRect_IsEmpty to check.
	The first rectangle represents the portion of \a r that is above \a y, and
	the second rectangle repesents the portion of \a r that is below \a y. The
	second rectangle is linked to the first via the first's vrect_t::next
	pointer.

	\param r		The rectangle to split.
	\param y		The horizontal line by which \r will be split.
	\return			The two linked rectangles representing the portions above
					and below \a y. The returned pointer points to the first
					(above) rectangle, which links to the second (below)
					rectangle.
	\note	It is the caller's responsibility to delete the returned
			rectangles.
*/
vrect_t *VRect_HSplit (const vrect_t *r, int y);

/** Return two rectangles representing the portions of \a r to the left and
	right of \a y.

	One of the returned rectangles may be empty. Use VRect_IsEmpty to check.
	The first rectangle represents the portion of \a r that is to the left of
	\a y, and the second rectangle repesents the portion of \a r that is to
	the right of \a y. The second rectangle is linked to the first via the
	first's vrect_t::next pointer.

	\param r		The rectangle to split.
	\param x		The vertical line by which \r will be split.
	\return			The two linked rectangles representing the portions to the
					left and right of \a y. The returned pointer points to the
					first (left) rectangle, which links to the second (right)
					rectangle.
	\note	It is the caller's responsibility to delete the returned
			rectangles.
*/
vrect_t *VRect_VSplit (const vrect_t *r, int x);

/** Return up to four rectangles representing the result of carving rectangle
	\a s out of rectangle \a r.

	None of the returned rectangles will be empty. If the difference is empty,
	null will be returned.

	\param r		The rectangle to be carved.
	\param s		The rectangle used to carve \a r.
	\return			Up to four linked rectangles representing the portions of
					\a r after carving out the portion represented by \a s, or
					null if the result is empty. A new rectangle that is a copy
					of \a r will be returned if \a r and \a s do not intersect.
	\note	It is the caller's responsibility to delete the returned
			rectangles.
*/
vrect_t *VRect_Difference (const vrect_t *r, const vrect_t *s);

#endif//__QF_vrect_h
