/*
	net_udp.h

	NetQuake UDP lan driver.

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

#ifndef __net_udp_h
#define __net_udp_h

#include "QF/qtypes.h"

/** \defgroup nq-udp NetQuake UDP lan driver.
	\ingroup nq-ld
*/
///@{

/** Initialize the UDP network interface.

	Opens a single control socket.

	\return			The control socket or -1 on failure.
*/

int UDP_Init (void);

/** Shutdown the UDP network interface.
*/
void UDP_Shutdown (void);

/** Open or close the accept socket.

	Sets net_acceptsocket to the socket number or -1.
	The accept socket is stored in net_hostport.

	\param state	True to open the socket, false to close it.
*/
void UDP_Listen (bool state);

/** Open a single socket on the specified port.

	If \p port is 0, then use the operating system default.

	\param port		The port on which to open the socket.
	\return			The socket number or -1 on failure.
*/
int UDP_OpenSocket (int port);

/** Close a socket.

	\param socket	The socket to close.
	\return			0 for success, -1 for failure.
*/
int UDP_CloseSocket (int socket);

/** Does very little.

	\param socket	The socket on which to do very little.
	\param addr		The address to which very little will be done.
	\return			0
*/
int UDP_Connect (int socket, netadr_t *addr);

/** Check for incoming packets on the accept socket.

	Checks if any packets are available on net_acceptsocket.
*/
int UDP_CheckNewConnections (void);

/** Read a packet from the specified socket.

	\param socket	The socket from which the packet will be read.
	\param buf		The buffer into which the packet will be read.
	\param len		The maximum number of bytes to read.
	\param[out] addr	The address from which the packet originated.
	\return			The number of bytes read or -1 on error.
*/
int UDP_Read (int socket, byte *buf, int len, netadr_t *addr);

/** Send a packet via the specified socket to the specified address.

	\param socket	The socket via which the packet will be sent.
	\param buf		The packet data to be sent.
	\param len		The number of bytes in the packet.
	\param addr		The addres to which the packet will be sent.
	\return			The number of bytes sent or -1 on error.
*/
int UDP_Write (int socket, byte *buf, int len, netadr_t *addr);

/** Broadcast a packet via the specified socket.

	\warning	It is a fatal error to use more than one socket for
				broadcasting.

	\param socket	The socket via which the packet will be broadcast.
	\param buf		The packet data to be sent.
	\param len		The number of bytes in the packet.
	\return			The number of bytes sent or -1 on error.
*/
int UDP_Broadcast (int socket, byte *buf, int len);

/** Convert an address to a string.

	Include the port number in the string.

	\warning	The return value is a pointer to a static buffer. The returned
				string must be saved if mixing calls with different addresses.

	\param addr		The address to convert.
	\return			The address in human readable form.
*/
const char *UDP_AddrToString (netadr_t *addr);

/** Retrieve the address to which the socket is bound.

	\param socket	The socket for which the address will be retrieved.
	\param[out] addr	The address to which the socket is bound.
	\return			0
*/
int UDP_GetSocketAddr (int socket, netadr_t *addr);

/** Convert an address to a hostname.

	\param[in] addr	The address to covert.
	\param[out] name	Output buffer for the hostname.

	\bug	No checking is done on the size of the buffer, and uses strcpy.
*/
int UDP_GetNameFromAddr (netadr_t *addr, char *name);

/** Convert a human readable address to a quake address.

	Accepts both host names (full or partial) and numeric form.

	The port address can be specified when using a numeric address.

	Numeric addresses can be partial: unspecified portions are filled out
	using the network address of the socket.

	\param name		The human readable address to be converted.
	\param addr		The resulting address of the conversion.
	\return			0 if the conversion is successful, otherwise -1.
*/
int UDP_GetAddrFromName (const char *name, netadr_t *addr);

/** Compare two network addresses.

	Compare the port number too.

	\param addr1	The first address to compare.
	\param addr2	The second address to compare.
	\return			-2 if the family is different.
	\return			-1 if the family is the same, but the address is
					different.
	\return			1 if the family and address are the same, but the port
					is different.
	\return			0 if everything is the same.
*/
int UDP_AddrCompare (netadr_t *addr1, netadr_t *addr2);

/** Get the port number from the socket address.

	\param addr		The socket address from which to retrieve the port number.
	\return			The port number.
*/
int UDP_GetSocketPort (netadr_t *addr);

/** Set the port number of the socket address.

	\param addr		The socket address which to will have its port number set.
	\param port		The port number to which the socket address will be set.
	\return			0
*/
int UDP_SetSocketPort (netadr_t *addr, int port);

///@}

#endif // __net_udp_h
