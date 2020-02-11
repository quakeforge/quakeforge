/*
	draw.h

	Video buffer handling definitions and prototypes

	Copyright (C) 1996-1997  Id Software, Inc.

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

#ifndef _DRAW_H
#define _DRAW_H

/** \defgroup video Video Sub-sytem */

/** \defgroup video_renderer Renderer Sub-system
	\ingroup video
*/

/** \defgroup video_renderer_draw Generic draw functions
	\ingroup video_renderer
*/
///@{

#include "QF/wad.h"

extern byte *draw_chars;

/** Initialize the draw stuff.
*/
void Draw_Init (void);

/** Draws one 8*8 graphics character with 0 being transparent.
	It can be clipped to the top of the screen to allow the console to be
	smoothly scrolled off.
	\param x	horizontal location of the top left corner of the character.
	\param y	vertical location of the top left corner of the character.
	\param ch	8 bit character to draw.
	\note		The character drawn is from the quake character set, which is
				(by default) standard ascii for 0x20-0x7e (white). 0xa0-0xfe is
				also standard ascii (brown). 0x01-0x1f and 0x80-0x9f are
				various drawing characters, and 0x7f is a backwards arrow.
*/
void Draw_Character (int x, int y, unsigned ch);

/** Draws a character string to the screen.
	No line wrapping is performed.
	\param x	horizontal location of the top left corner of the character.
	\param y	vertical location of the top left corner of the character.
	\param str	8 bit character string to draw.
	\note		See Draw_Character() for character set description.
				String is normal nul terminated C string.
*/
void Draw_String (int x, int y, const char *str);

/** Draws a character sub-string to the screen.
	No line wrapping is performed.
	\param x	horizontal location of the top left corner of the character.
	\param y	vertical location of the top left corner of the character.
	\param str	8 bit character string to draw.
	\param count Maximum characters of the string to draw.
	\note		See Draw_Character() for character set description.
				Draws up to \p count characters, or stops at the first nul
				character.
*/
void Draw_nString (int x, int y, const char *str, int count);

/** Draws a character string to the screen.
	No line wrapping is performed.
	\param x	horizontal location of the top left corner of the character.
	\param y	vertical location of the top left corner of the character.
	\param str	8 bit character string to draw.
	\note		See Draw_Character() for character set description.
				String is normal nul terminated C string.
				Characters of the string are forced to have their high bit set
				(ie, they will be in the range 0x80-0xff).
*/
void Draw_AltString (int x, int y, const char *str);

/** Draw the console background with various optional effects.
	\param lines Vertical size in pixels of the console.
	\param alpha Console transparency level (255 = opaque).
	\note		\p alpha is effective only in the OpenGL renderer. Effectively
				255 (opaque) for the software renderer.

	\c gl_conspin causes the background to spin.

	\c gl_constretch causes the background to stretch rather than slide.
*/
void Draw_ConsoleBackground (int lines, byte alpha);

/** Draw a crosshair at the center of the screen.
	\c crosshair specifies which crosshair (1 = '+', 2 = large 'x' shape,
	3 = fancy '+' shape)
	\c cl_crossx and \c cl_crossy offset the crosshair from the center of the
	screen.
*/
void Draw_Crosshair (void);

/** Draw the specified crosshair on the screen.
	\param ch	crosshair to draw
	\param x	horizontal position of the center of the crosshair.
	\param y	vertical position of the center of the crosshair.

	See Draw_Crosshair() for description of crosshair values.
*/
void Draw_CrosshairAt (int ch, int x, int y);

/** Clear a rectangle with a tiled background.
	\param x	horizontal position of the upper left corner of the rectangle
	\param y	horizontal position of the upper left corner of the rectangle
	\param w	width of the rectangle
	\param h	height of the rectangle

	The background used is the "backtile" WAD lump.
*/
void Draw_TileClear (int x, int y, int w, int h);

/** Clear a rectangle with a solid color.
	\param x	horizontal position of the upper left corner of the rectangle
	\param y	horizontal position of the upper left corner of the rectangle
	\param w	width of the rectangle
	\param h	height of the rectangle
	\param c	8 bit color index.

	The color comes from the quake palette.
*/
void Draw_Fill (int x, int y, int w, int h, int c);

/** Draw a text box on the screen
	\param x	horizontal location of the upper left corner of the box
	\param y	vertical location of the upper left corner of the box
	\param width horizontal size in character cells of the region
	\param lines vertical size in character cells of the region
	\param alpha transparency of the box
*/
void Draw_TextBox (int x, int y, int width, int lines, byte alpha);

/** Darken the screen.
*/
void Draw_FadeScreen (void);

/** Shift the screen colors.
*/
void Draw_BlendScreen (quat_t color);
///@}

/** \defgroup video_renderer_draw_qpic QPic functions
	\ingroup video_renderer_draw
*/
///@{
/** Load a qpic from the filesystem.
	\param path	path of the file within the quake filesystem
	\param alpha transparency level of the pic.
	\return		pointer qpic data.
	\note		Up to MAX_CACHED_PICS qpics can be loaded at a time this way
*/
qpic_t *Draw_CachePic (const char *path, qboolean alpha);

/** Remove a qpic from the qpic cache.

	This affects only those qpics that were loaded via Draw_CachePic.

	\param path	path of the file within the quake filesystem
*/
void Draw_UncachePic (const char *path);

/** Create a qpic from raw data.

	\param width	The width of the pic.
	\param height	The height of the pic.
	\param data		The raw data bytes. The system palette will be used for
					colors.
	\return			pointer qpic data.
*/
qpic_t *Draw_MakePic (int width, int height, const byte *data);

/** Destroy a qpic created by Draw_MakePic.

	\param pic		The qpic to destory.
*/
void Draw_DestroyPic (qpic_t *pic);

/** Load a qpic from gfx.wad.
	\param name	name of the was lump to load
	\return		pointer qpic data.
*/
qpic_t *Draw_PicFromWad (const char *name);

/** Draw a qpic to the screen
	\param x	horizontal location of the upper left corner of the qpic
	\param y	vertical location of the upper left corner of the qpic
	\param pic	qpic to draw
*/
void Draw_Pic (int x, int y, qpic_t *pic);

/** Draw a qpic to the screen
	\param x	horizontal location of the upper left corner of the qpic
	\param y	vertical location of the upper left corner of the qpic
	\param pic	qpic to draw
*/
void Draw_Picf (float x, float y, qpic_t *pic);

/** Draw a sub-region of a qpic to the screan
	\param x	horizontal screen location of the upper left corner of the
				sub-region
	\param y	vertical screen location of the upper left corner of the
				sub-region
	\param pic	qpic to draw
	\param srcx	horizontal qpic location of the upper left corner of the
				sub-region
	\param srcy	vertical qpic location of the upper left corner of the
				sub-region
	\param width horizontal size of the sub-region to be drawn
	\param height vertical size of the sub-region to be drawn
*/
void Draw_SubPic(int x, int y, qpic_t *pic, int srcx, int srcy, int width, int height);
///@}

#endif // _DRAW_H
