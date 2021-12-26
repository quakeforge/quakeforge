/*
	txtbuffer.h

	Text buffer for edit or scrollback

	Copyright (C) 2020  Bill Currie <bill@taniwha.org>

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

#ifndef __QF_ui_txtbuffer_h
#define __QF_ui_txtbuffer_h

#include <stdint.h>
#include <stdlib.h>

/** \defgroup txtbuffer Text buffers
	\ingroup utils
*/
///@{

/// must be a power of 2
#define TXTBUFFER_GROW 0x4000

/**	Text buffer implementing efficient editing.
*/
typedef struct txtbuffer_s {
	struct txtbuffer_s *next;
	char       *text;
	size_t      textSize;	///< amount of text in the buffer
	size_t      bufferSize;	///< current capacity of the buffer
	size_t      gapOffset;	///< beginning of the gap
	size_t      gapSize;	///< size of gap. gapSize + textSize == bufferSize
} txtbuffer_t;

/**	Create a new, empty buffer.
*/
txtbuffer_t *TextBuffer_Create (void);

/**	Destroy a buffer, freeing all resources connected to it.
	\param buffer	The buffer to destroy.
*/
void TextBuffer_Destroy (txtbuffer_t *buffer);

/**	Open a gap for writing at the specified offset.

	Text after the offset is moved to be after the opened gap.
	The buffer is resized as necessary.

	\param buffer	The buffer to be updated
	\param offset	The offset in the buffer at which to insert the text block
	\param text_len	The size of the gap to be opened
	\return			Pointr to beginning of gap if successful, 0 if failure
					(offset not valid or out of memory)
*/
char *TextBuffer_OpenGap (txtbuffer_t *buffer, size_t offset,
						   size_t text_len);

/**	Insert a block of text at the specified offset.

	Text after the offset is moved to be after the inserted block of text.
	The buffer is resized as necessary.
	nul characters are not significant in that they do not mark the end of
	the text to be inserted.

	\param buffer	The buffer to be updated
	\param offset	The offset in the buffer at which to insert the text block
	\param text		The text block to be inserted. May contain nul ('\0')
					characters.
	\param text_len	The number of characters to insert.
	\return			1 for success, 0 for failure (offset not valid or out
					of memory)
*/
int TextBuffer_InsertAt (txtbuffer_t *buffer, size_t offset,
						 const char *text, size_t text_len);

/**	Delete a block of text from the buffer.

	The buffer is not resized. Rather, its capacity before resizing is require
	is increased.

	\param buffer	The buffer to be updated
	\param offset	The offset of the beginning of the text block to be deleted
	\param len		The amount of characters to be deleted. Values larger than
					the amount of text in the buffer beyond the beginning of
					block are truncated to the amount of remaining text. Thus
					using excessivly large values sets the offset to be the
					end of the buffer.
	\return			1 for success, 0 for failure (offset not valid)
*/
int TextBuffer_DeleteAt (txtbuffer_t *buffer, size_t offset, size_t len);

///@}

#endif//__QF_ui_txtbuffer_h
