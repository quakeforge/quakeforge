#include <sys/types.h>
#include <sys/timeb.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "QF/qtypes.h"

//Defines

#define	MAX_ARGS		80
#define MAX_NUM_ARGVS	50

#define	MAX_MASTERS	8				// max recipients for heartbeat packets

#define	PORT_ANY	-1

#define	PORT_MASTER	26900
#define	PORT_SERVER	26950

//=========================================

// out of band message id bytes

// M = master, S = server, C = client, A = any
// the second character will allways be \n if the message isn't a single
// byte long (?? not true anymore?)

#define	S2C_CHALLENGE		'c'
#define	S2C_CONNECTION		'j'
#define	A2A_PING			'k'	// respond with an A2A_ACK
#define	A2A_ACK				'l'	// general acknowledgement without info
#define	A2A_NACK			'm'	// [+ comment] general failure
#define A2A_ECHO			'e' // for echoing
#define	A2C_PRINT			'n'	// print a message on client

#define	S2M_HEARTBEAT		'a'	// + serverinfo + userlist + fraglist
#define	A2C_CLIENT_COMMAND	'B'	// + command line
#define	S2M_SHUTDOWN		'C'

//KS:
#define A2M_LIST			'o'
#define M2A_SENDLIST		'p'

#define	MAX_MSGLEN		1450		// max length of a reliable message
#define	MAX_DATAGRAM	1450		// max length of unreliable message
#define	MAX_UDP_PACKET	(MAX_MSGLEN*2)	// one more than msg + header

#define	MAX_SERVERINFO_STRING	512
#define	MAX_CLIENTS		32

//Typedefs

typedef struct server_s
{
	netadr_t ip;
	int		 heartbeat;
	int		 players;
	char	 info[MAX_SERVERINFO_STRING];
	struct server_s *next;
	struct server_s *previous;
	double timeout;
} server_t;

//Function prototypes

//net_test.cpp

void SV_GetConsoleCommands (void);
void SV_Frame(void);
void SV_Shutdown(void);
double Sys_DoubleTime (void);

//net.cpp

void SV_InitNet (void);
int UDP_OpenSocket (int port);
void SV_ReadPackets (void);
void SV_ConnectionlessPacket (void);
void	NET_CopyAdr (netadr_t *a, netadr_t *b);
void SVL_Add(server_t *sv);
void SVL_Remove(server_t *sv);
void SVL_Clear(void);
server_t* SVL_Find(netadr_t adr);
server_t* SVL_New(netadr_t *adr);
void Cmd_Filter_f(void);


//Globals


extern int			net_socket;
//extern WSADATA		winsockdata;

extern int num_masters;

extern short	(*BigShort) (short l);
extern short	(*LittleShort) (short l);
extern int	(*BigLong) (int l);
extern int	(*LittleLong) (int l);
extern float	(*BigFloat) (float l);
extern float	(*LittleFloat) (float l);

extern int		msg_readcount;
extern qboolean	msg_badread;

extern server_t *sv_list;

