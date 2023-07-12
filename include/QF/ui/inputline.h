/*
	inputline.h

	Input line support

	Copyright (C) 2001  Bill Currie <bill@taniwha.org>
	Copyright (C) 2021  Bill Currie <bill@taniwha.org>

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

#ifndef __QF_ui_inputline_h
#define __QF_ui_inputline_h

#include <stdlib.h>

typedef struct inputline_s
{
	char	**lines;		// array of lines for input history
	int		num_lines;		// number of lines in arry. 1 == no history
	size_t	line_size;		// space available in each line. includes \0
	char	prompt_char;	// char placed at the beginning of the line
	int		edit_line;		// current line being edited
	int		history_line;	// current history line
	size_t	linepos;		// cursor position within the current edit line
	size_t	scroll;			// beginning of displayed line
	size_t	width;			// viewable width for horizontal scrolling
	const char *line;
	void   *user_data;		// eg: window pointer
	void	(*complete)(struct inputline_s *); // tab key pressed
	void	(*enter)(struct inputline_s *); // enter key pressed
	void	(*draw)(struct inputline_s *); // draw input line to screen

	int		x, y;			// coordinates depend on display
	int		cursor;			// is the cursor active (drawn?)
} inputline_t;

inputline_t *Con_CreateInputLine (int lines, int lsize, char prompt);
void Con_DestroyInputLine (inputline_t *inputline);
void Con_ClearTyping (inputline_t *il, int save);
void Con_ProcessInputLine (inputline_t *il, int ch);

#endif//__QF_ui_inputline_h
