/*
	#FILENAME#

	#DESCRIPTION#

	Copyright (C) 2002 #AUTHOR#

	Author: #AUTHOR#
	Date: #DATE#

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

#define GIB_DATA(buffer) ((gib_buffer_data_t *)(buffer->data))

typedef struct gib_buffer_data_s {
	struct dstring_s *arg_composite;
	struct dstring_s *current_token;
	struct dstring_s *loop_program;
	struct dstring_s *loop_data;
	
	char *loop_var_p, *loop_list_p, *loop_ifs_p;
	
	// Data for handling return values
	struct {
		qboolean waiting, available; // Return value states
		struct dstring_s *retval; // Returned value
				
		// Data saved by tokenizer/processor
		unsigned int line_pos; // Position within line
		unsigned int token_pos; // Position within token
		qboolean cat; // Concatenate to previous token?
		int noprocess; // Process tokens?
		char delim; // delimiter of token
	} ret;
	
	struct hashtab_s *locals; // Local variables
	
	enum {
		GIB_BUFFER_NORMAL, // Normal buffer
		GIB_BUFFER_LOOP, // Looping buffer
		GIB_BUFFER_PROXY // Responsible for embedded command
	} type;
} gib_buffer_data_t;

void GIB_Buffer_Construct (struct cbuf_s *cbuf);
void GIB_Buffer_Destruct (struct cbuf_s *cbuf);
void GIB_Buffer_Reset (struct cbuf_s *cbuf);
