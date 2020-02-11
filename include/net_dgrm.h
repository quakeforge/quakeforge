/*
	net_dgrm.h

	@description@

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


/** \defgroup nq-dgrm NetQuake Datagram network driver.
	\ingroup nq-nd
*/
///@{

/** Initialize the Datagram net driver.

	Also initializes the underlying lan drivers.

	\return			0 if successful, otherwise -1.
*/
int			Datagram_Init (void);

/** Control the listen status of the underlying lan drivers.

	\param state	True to enable, false to disable.
*/
void		Datagram_Listen (qboolean state);

/** Search for hosts (servers) on the local network.

	If \p xmit is true, broadcast a CCREQ_SERVER_INFO packet via all
	underlying lan drivers using their control ports.

	Check for any CCREP_SERVER_INFO responses. Ignore all other packets.

	\param xmit		True to send the broadcast, falst to only listen.
*/
void		Datagram_SearchForHosts (qboolean xmit);

/** Connect to the specified host.

	\param host		The host to which the connection will be made.
	\return			A pointer to the socket representing the connection, or
					null if unsuccessful.
*/
qsocket_t	*Datagram_Connect (const char *host);

/** Check for a new incoming connection.

	\return			A pointer to the socekt representing the new connection,
					or null if none found.
*/
qsocket_t 	*Datagram_CheckNewConnections (void);

/** Check for and process an incoming packet on the socket.

	\param sock		The socket to check.
	\return			-1 if there is an error
	\return			0 if the packet is stale
	\return			1 if the packet is reliable
	\return			2 if the packet is unreliable
*/
int			Datagram_GetMessage (qsocket_t *sock);

/** Send a reliable packet to the socket.

	\param sock		The socket to which the packet will be sent.
	\param data		The packet to be sent.
	\return			-1 If there is an error
	\return			1 if everything is ok.
*/
int			Datagram_SendMessage (qsocket_t *sock, sizebuf_t *data);

/** Send an unreliable packet to the socket.

	\param sock		The socket to which the packet will be sent.
	\param data		The packet to be sent.
	\return			-1 If there is an error
	\return			1 if everything is ok.
*/
int			Datagram_SendUnreliableMessage (qsocket_t *sock, sizebuf_t *data);

/** Check if a reliable message can be sent to the socket.

	\param sock		The socket to check.
	\return			True if the packet can be sent.
*/
qboolean	Datagram_CanSendMessage (qsocket_t *sock);

/** Check if an unreliable message can be sent to the socket.

	Always true.

	\param sock		The socket to check.
	\return			True if the packet can be sent.
*/
qboolean	Datagram_CanSendUnreliableMessage (qsocket_t *sock);

/** Close the socket.

	\param sock		The socket to close.
*/
void		Datagram_Close (qsocket_t *sock);

/** Shutdown the Datagram net driver.
*/
void		Datagram_Shutdown (void);

///@}
