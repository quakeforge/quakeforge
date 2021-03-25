/*
	connection.h

	connection management

	Copyright (C) 2004 Bill Currie <bill@taniwha.org>

	Author: Bill Currie
	Date: 2004/02/20

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

#ifndef __connection_h
#define __connection_h

#include "netchan.h"

/**	\defgroup qtv_connection Connection Management
	\ingroup qtv
*/
///@{

typedef struct connection_s {
	netadr_t    address;		///< Address of the remote end.
	void       *object;			///< Connection specific data.
	/**	Handler for incoming packets.
		\param con		This connection. ("this", "self"...)
		\param obj		The connection specific data (object)
	*/
	void      (*handler) (struct connection_s *con, void *obj);
} connection_t;

/**	Initialize the connection management system.
*/
void Connection_Init (void);

/**	Add a new connection.

	\param address	The address of the remote end.
	\param object	Connection specific data. Will be passed to \a handler.
	\param handler	Callback for handling incoming packets.
					connection_t::handler
	\return			The new connection object.
*/
connection_t *Connection_Add (netadr_t *address, void *object,
							  void (*handler)(connection_t *, void *));

/**	Delete a connection.

	\param con		The connection to be deleted.
*/
void Connection_Del (connection_t *con);

/**	Search for an established connection based on the remote address.

	\param address	The remote address.
	\return			The connection associated with the remote address, or
					NULL if not found.
*/
connection_t *Connection_Find (netadr_t *address);

///@}

#endif//__connection_h
