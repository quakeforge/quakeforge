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

*/

static const char rcsid[] =
        "$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "QF/dstring.h"
#include "QF/cbuf.h"
#include "QF/hash.h"
#include "QF/gib_buffer.h"

void GIB_Buffer_Construct (struct cbuf_s *cbuf)
{
	cbuf->data = calloc (1, sizeof (gib_buffer_data_t));
	GIB_DATA (cbuf)->arg_composite = dstring_newstr ();
	GIB_DATA (cbuf)->current_token = dstring_newstr ();
	GIB_DATA (cbuf)->ret.retval = dstring_newstr ();
}

void GIB_Buffer_Destruct (struct cbuf_s *cbuf)
{
	dstring_delete (GIB_DATA (cbuf)->arg_composite);
	dstring_delete (GIB_DATA (cbuf)->current_token);
	if (GIB_DATA (cbuf)->loop_program)
		dstring_delete (GIB_DATA(cbuf)->loop_program);
	if (GIB_DATA (cbuf)->loop_data)
		dstring_delete (GIB_DATA(cbuf)->loop_data);
	dstring_delete (GIB_DATA (cbuf)->ret.retval);
	if (GIB_DATA(cbuf)->locals && GIB_DATA(cbuf)->type == GIB_BUFFER_NORMAL)
		Hash_DelTable (GIB_DATA(cbuf)->locals);
	free (cbuf->data);
}
