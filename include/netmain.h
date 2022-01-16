/*
	netmain.h

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

#ifndef __net_h
#define __net_h

#include "QF/quakeio.h"
#include "QF/sizebuf.h"

/** \defgroup nq-net NetQuake network support.
	\ingroup network
*/
///@{

typedef struct
{
//FIXME not yet ready for ipv6
//#ifdef HAVE_IPV6
//	byte        ip[16];
//#else
	byte        ip[4];
//#endif
	unsigned short  port;
	unsigned short  family;
} netadr_t;

#define	NET_NAMELEN			64

#define NET_MAXMESSAGE		32000
#define NET_HEADERSIZE		(2 * sizeof(unsigned int))
#define NET_DATAGRAMSIZE	(MAX_DATAGRAM + NET_HEADERSIZE)

/** \name NetHeader flags
*/
///@{
#define NETFLAG_LENGTH_MASK	0x0000ffff
#define NETFLAG_DATA		0x00010000
#define NETFLAG_ACK			0x00020000
#define NETFLAG_NAK			0x00040000
#define NETFLAG_EOM			0x00080000
#define NETFLAG_UNRELIABLE	0x00100000
#define NETFLAG_CTL			0x80000000
///@}


#define NET_PROTOCOL_VERSION	3

/** \name Connection Protocol

	This is the network info/connection protocol.  It is used to find Quake
	servers, get info about them, and connect to them.  Once connected, the
	Quake game protocol (documented elsewhere) is used.

	General notes:

	\note	There are two address forms used.  The short form is just a
		port number.  The address that goes along with the port is defined as
		"whatever address you receive this reponse from".  This lets us use
		the host OS to solve the problem of multiple host addresses (possibly
		with no routing between them); the host will use the right address
		when we reply to the inbound connection request.  The long from is
		a full address and port in a string.  It is used for returning the
		address of a server that is not running locally.
*/
///@{

/** Connect Request:
	\arg \b string	\c game_name			\em "QUAKE"
	\arg \b byte	\c net_protocol_version	\em NET_PROTOCOL_VERSION

	\note	\c game_name is currently always "QUAKE", but is there so this
			same protocol can be used for future games as well
*/
#define CCREQ_CONNECT		0x01

/** Connect Request:
	\arg \b string	\c game_name			\em "QUAKE"
	\arg \b byte	\c net_protocol_version	\em NET_PROTOCOL_VERSION

	\note	\c game_name is currently always "QUAKE", but is there so this
			same protocol can be used for future games as well
*/
#define CCREQ_SERVER_INFO	0x02

/** Connect Request:
	\arg \b byte	\c player_number
*/
#define CCREQ_PLAYER_INFO	0x03

/** Connect Request:
	\arg \b string	\c rule
*/
#define CCREQ_RULE_INFO		0x04


/** Connect Reply:
	\arg \b long	\c port	The port which the client is to use for further
							communication.
*/
#define CCREP_ACCEPT		0x81

/** Connect Reply:
	\arg \b string	\c reason
*/
#define CCREP_REJECT		0x82

/** Connect Reply:
	\arg \b string	\c server_address
	\arg \b string	\c host_name
	\arg \b string	\c level_name
	\arg \b byte	\c current_players
	\arg \b byte	\c max_players
	\arg \b byte	\c protocol_version	\em NET_PROTOCOL_VERSION
*/
#define CCREP_SERVER_INFO	0x83

/** Connect Reply:
	\arg \b byte	\c player_number
	\arg \b string	\c name
	\arg \b long	\c colors
	\arg \b long	\c frags
	\arg \b long	\c connect_time
	\arg \b string	\c address
*/
#define CCREP_PLAYER_INFO	0x84

/** Connect Reply:
	\arg \b string	\c rule
	\arg \b string	\c value
*/
#define CCREP_RULE_INFO		0x85
///@}

typedef struct qsocket_s {
	struct qsocket_s	*next;
	/// \name socket timing
	//@{
	double			connecttime;		///< Time client connected.
	double			lastMessageTime;	///< Time last message was received.
	double			lastSendTime;		///< Time last message was sent.
	//@}

	/// \name socket status
	//@{
	qboolean		disconnected;		///< Socket is not in use.
	qboolean		canSend;			///< Socket can send a message.
	qboolean		sendNext;
	//@}

	/// \name socket drivers
	//@{
	int				driver;				///< Net driver used by this socket.
	int				landriver;			///< Lan driver used by this socket.
	int				socket;				///< Lan driver socket handle.
	void			*driverdata;		///< Net driver private data.
	//@}

	/// \name message transmission
	//@{
	unsigned int	ackSequence;
	unsigned int	sendSequence;
	unsigned int	unreliableSendSequence;
	int				sendMessageLength;
	byte			sendMessage[NET_MAXMESSAGE];
	//@}

	/// \name message reception
	//@{
	unsigned int	receiveSequence;
	unsigned int	unreliableReceiveSequence;
	int				receiveMessageLength;
	byte			receiveMessage[NET_MAXMESSAGE];
	//@}

	/// \name socket address
	//@{
	netadr_t	addr;
	char			address[NET_NAMELEN];	///< Human readable form.
	//@}
} qsocket_t;

/** \name socket management
*/
///@{
extern qsocket_t	*net_activeSockets;
extern qsocket_t	*net_freeSockets;
extern int			net_numsockets;
///@}

#define	MAX_NET_DRIVERS		8

extern int			DEFAULTnet_hostport;
extern int			net_hostport;

extern int net_driverlevel;

/** \name message statistics
*/
///@{
extern int		messagesSent;
extern int		messagesReceived;
extern int		unreliableMessagesSent;
extern int		unreliableMessagesReceived;
///@}

/** Create and initialize a new qsocket.

	Called by drivers when a new communications endpoint is required.
	The sequence and buffer fields will be filled in properly.

	\return			The qsocket representing the connection.
*/
qsocket_t *NET_NewQSocket (void);

/** Destroy a qsocket.

	\param sock		The qsocket representing the connection.
*/
void NET_FreeQSocket(qsocket_t *sock);

/** Cache the system time for the network sub-system.

	\return			The current time.
*/
double SetNetTime(void);


#define HOSTCACHESIZE	8

typedef struct {
	char	name[16];
	char	map[16];
	char	cname[32];
	int		users;
	int		maxusers;
	int		driver;
	int		ldriver;
	netadr_t addr;
} hostcache_t;

void NET_AddCachedHost (const char *name, const char *map, const char *cname,
						int users, int maxusers, int driver, int ldriver,
						const netadr_t *addr);

extern	double		net_time;
extern	struct msg_s *net_message;
extern	unsigned	net_activeconnections;

/** Initialize the networking sub-system.
*/
void		NET_Init (void);

/** Check for new connections.

	\return			Pointer to the qsocket for the new connection if there
					is one, otherwise null.
*/
struct qsocket_s	*NET_CheckNewConnections (void);

/** Connect to a host.

	\param host		The name of the host to which will be connected.
	\return			Pointer to the qsocket representing the connection, or
					null if unable to connect.
*/
struct qsocket_s	*NET_Connect (const char *host);

/** Check if a message can be sent to the connection.

	\param sock		The qsocket representing the connection.
	\return			True if the message can be sent.
*/
qboolean NET_CanSendMessage (qsocket_t *sock);

/** Read a single message from the connection into net_message.

	If there is a complete message, return it in net_message.

	\param sock		The qsocket representing the connection.
	\return			0 if no data is waiting.
	\return			1 if a message was received.
	\return			-1 if the connection died.
*/
int			NET_GetMessage (struct qsocket_s *sock);

/** Send a single reliable message to the connection.

	Try to send a complete length+message unit over the reliable stream.

	\param sock		The qsocket representing the connection.
	\param data		The message to send.
	\return			0 if the message connot be delivered reliably, but the
					connection is still considered valid
	\return			1 if the message was sent properly
	\return			-1 if the connection died
*/
int			NET_SendMessage (struct qsocket_s *sock, sizebuf_t *data);

/** Send a single unreliable message to the connection.

	\param sock		The qsocket representing the connection.
	\param data		The message to send.
	\return			1 if the message was sent properly
	\return			-1 if the connection died
*/
int			NET_SendUnreliableMessage (struct qsocket_s *sock, sizebuf_t *data);

/** Send a reliable message to all attached clients.

	\param data		The message to send.
	\param blocktime	The blocking timeout in seconds.
	\return			The number of clients to which the message could not be
					sent before the timeout.
*/
int			NET_SendToAll(sizebuf_t *data, double blocktime);

/** Close a connection.

	If a dead connection is returned by a get or send function, this function
	should be called when it is convenient.

	Server calls when a client is kicked off for a game related misbehavior
	like an illegal protocal conversation.  Client calls when disconnecting
	from a server.

	A netcon_t number will not be reused until this function is called for it

	\param sock		The qsocket representing the connection.
*/
void		NET_Close (struct qsocket_s *sock);

/** Run any current poll procedures.
*/
void NET_Poll(void);


typedef struct _PollProcedure {
	struct _PollProcedure	*next;
	double					nextTime;
	void					(*procedure)(void *);
	void					*arg;
} PollProcedure;

/** Schedule a poll procedure to run.

	The poll procedure will be called by NET_Poll() no earlier than
	"now"+timeOffset.

	\param pp		The poll procedure to shedule.
	\param timeOffset	The time offset from "now" at which the procedure
					will be run.
*/
void SchedulePollProcedure(PollProcedure *pp, double timeOffset);

extern	qboolean	tcpipAvailable;
extern	char		my_tcpip_address[NET_NAMELEN];

extern	qboolean	slistInProgress;
extern	qboolean	slistSilent;
extern	qboolean	slistLocal;

extern struct cvar_s	*hostname;

extern QFile *vcrFile;

///@}

/** \defgroup nq-ld NetQuake lan drivers.
	\ingroup nq-net
*/
///@{

typedef struct {
	const char		*name;
	qboolean	initialized;
	int			controlSock;
	int			(*Init) (void);
	void		(*Shutdown) (void);
	void		(*Listen) (qboolean state);
	int 		(*OpenSocket) (int port);
	int 		(*CloseSocket) (int socket);
	int 		(*Connect) (int socket, netadr_t *addr);
	int 		(*CheckNewConnections) (void);
	int 		(*Read) (int socket, byte *buf, int len, netadr_t *addr);
	int 		(*Write) (int socket, byte *buf, int len, netadr_t *addr);
	int 		(*Broadcast) (int socket, byte *buf, int len);
	const char *		(*AddrToString) (netadr_t *addr);
	int 		(*GetSocketAddr) (int socket, netadr_t *addr);
	int 		(*GetNameFromAddr) (netadr_t *addr, char *name);
	int 		(*GetAddrFromName) (const char *name, netadr_t *addr);
	int			(*AddrCompare) (netadr_t *addr1, netadr_t *addr2);
	int			(*GetSocketPort) (netadr_t *addr);
	int			(*SetSocketPort) (netadr_t *addr, int port);
} net_landriver_t;

extern int 				net_numlandrivers;
extern net_landriver_t	net_landrivers[MAX_NET_DRIVERS];

///@}

/** \defgroup nq-nd NetQuake network drivers.
	\ingroup nq-net
*/
///@{

typedef struct {
	const char		*name;
	qboolean	initialized;
	int			(*Init) (void);
	void		(*Listen) (qboolean state);
	void		(*SearchForHosts) (qboolean xmit);
	qsocket_t	*(*Connect) (const char *host);
	qsocket_t 	*(*CheckNewConnections) (void);
	int			(*QGetMessage) (qsocket_t *sock);
	int			(*QSendMessage) (qsocket_t *sock, sizebuf_t *data);
	int			(*SendUnreliableMessage) (qsocket_t *sock, sizebuf_t *data);
	qboolean	(*CanSendMessage) (qsocket_t *sock);
	qboolean	(*CanSendUnreliableMessage) (qsocket_t *sock);
	void		(*Close) (qsocket_t *sock);
	void		(*Shutdown) (void);
	int			controlSock;
} net_driver_t;

extern int			net_numdrivers;
extern net_driver_t	net_drivers[MAX_NET_DRIVERS];

///@}

#endif // __net_h
