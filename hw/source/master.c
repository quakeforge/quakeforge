#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/idparse.h"
#include "QF/msg.h"
#include "QF/plugin.h"
#include "QF/qargs.h"
#include "QF/sizebuf.h"
#include "QF/sys.h"
#include "../qw/include/net.h"
#include "defs.h"

qboolean is_server = true;
static cbuf_t *mst_cbuf;
cvar_t     *sv_console_plugin;
SERVER_PLUGIN_PROTOS
static plugin_list_t server_plugin_list[] = {
	SERVER_PLUGIN_LIST
};


typedef struct filter_s {
	netadr_t    from;
	netadr_t    to;
	struct filter_s *next;
	struct filter_s *previous;
} filter_t;

filter_t   *filter_list = NULL;

static void
FL_Remove (filter_t * filter)
{
	if (filter->previous)
		filter->previous->next = filter->next;
	if (filter->next)
		filter->next->previous = filter->previous;
	filter->next = NULL;
	filter->previous = NULL;
	if (filter_list == filter)
		filter_list = NULL;
}

static void
FL_Clear (void)
{
	filter_t   *filter;

	for (filter = filter_list; filter;) {
		if (filter) {
			filter_t   *next = filter->next;

			FL_Remove (filter);
			free (filter);
			filter = NULL;
			filter = next;
		}
	}
	filter_list = NULL;
}

static filter_t *
FL_New (netadr_t *adr1, netadr_t *adr2)
{
	filter_t   *filter;

	filter = (filter_t *) calloc (1, sizeof (filter_t));
	if (adr1)
		NET_CopyAdr (&filter->from, adr1);
	if (adr2)
		NET_CopyAdr (&filter->to, adr2);
	return filter;
}

static void
FL_Add (filter_t * filter)
{
	filter->next = filter_list;
	filter->previous = NULL;
	if (filter_list)
		filter_list->previous = filter;
	filter_list = filter;
}

static filter_t *
FL_Find (netadr_t adr)
{
	filter_t   *filter;

	for (filter = filter_list; filter; filter = filter->next) {
		if (NET_CompareBaseAdr (filter->from, adr))
			return filter;
	}
	return NULL;
}

server_t   *sv_list = NULL;

void
SVL_Clear (void)
{
	server_t   *sv;

	for (sv = sv_list; sv;) {
		if (sv) {
			server_t   *next = sv->next;

			SVL_Remove (sv);
			free (sv);
			sv = NULL;
			sv = next;
		}
	} sv_list = NULL;
}

server_t *
SVL_New (netadr_t *adr)
{
	server_t   *sv;
	sv = (server_t *) calloc (1, sizeof (server_t));
	sv->heartbeat = 0;
	sv->info[0] = 0;
	sv->ip.ip[0] = sv->ip.ip[1] = sv->ip.ip[2] = sv->ip.ip[3] = 0;
	sv->ip.port = 0;
	sv->next = NULL;
	sv->previous = NULL;
	sv->players = 0;
	if (adr)
		NET_CopyAdr (&sv->ip, adr);
	return sv;
}

void
SVL_Add (server_t *sv)
{
	sv->next = sv_list;
	sv->previous = NULL;
	if (sv_list)
		sv_list->previous = sv;
	sv_list = sv;
}

void
SVL_Remove (server_t *sv)
{
	if (sv_list == sv)
		sv_list = sv->next;
	if (sv->previous)
		sv->previous->next = sv->next;
	if (sv->next)
		sv->next->previous = sv->previous;
	sv->next = NULL;
	sv->previous = NULL;
}

server_t   *
SVL_Find (netadr_t adr)
{
	server_t   *sv;

	for (sv = sv_list; sv; sv = sv->next) {
		if (NET_CompareAdr (sv->ip, adr))
			return sv;
	}
	return NULL;
}

static void
NET_Filter (void)
{
	netadr_t    filter_adr;
	int         hold_port;

	hold_port = net_from.port;
	NET_StringToAdr ("127.0.0.1:26950", &filter_adr);
	if (NET_CompareBaseAdr (net_from, filter_adr)) {
		NET_StringToAdr ("0.0.0.0:26950", &filter_adr);
		if (!NET_CompareBaseAdr (net_local_adr, filter_adr)) {
			NET_CopyAdr (&net_from, &net_local_adr);
			net_from.port = hold_port;
		}
		return;
	}
	// if no compare with filter list
	filter_t   *filter;

	if ((filter = FL_Find (net_from))) {
		NET_CopyAdr (&net_from, &filter->to);
		net_from.port = hold_port;
	}
}

void
SV_InitNet (void)
{
	int         port;
	int         p;

	mst_cbuf = Cbuf_New (&id_interp);

	port = PORT_MASTER;
	p = COM_CheckParm ("-port");
	if (p && p < com_argc) {
		port = atoi (com_argv[p + 1]);
		printf ("Port: %i\n", port);
	}
	NET_Init (port);

	// Add filters
	FILE       *filters;
	char        str[64];

	if ((filters = fopen ("filters.ini", "rt"))) {
		while (fgets (str, 64, filters)) {
			Cbuf_AddText (mst_cbuf, "filter add ");
			Cbuf_AddText (mst_cbuf, str);
			Cbuf_AddText (mst_cbuf, "\n");
		}
		fclose (filters);
	}
}

void
NET_CopyAdr (netadr_t *a, netadr_t *b)
{
	a->ip[0] = b->ip[0];
	a->ip[1] = b->ip[1];
	a->ip[2] = b->ip[2];
	a->ip[3] = b->ip[3];
	a->port = b->port;
}

static void
AnalysePacket (void)
{
	byte        c;
	byte       *p;
	int         i;

	printf ("%s sending packet:\n", NET_AdrToString (net_from));
	p = net_message->message->data;
	for (i = 0; i < net_message->message->cursize; i++, p++) {
		c = p[0];
		printf (" %3i ", c);
		if (i % 8 == 7)
			printf ("\n");
	}
	printf ("\n");
	printf ("\n");
	p = net_message->message->data;
	for (i = 0; i < net_message->message->cursize; i++, p++) {
		c = p[0];
		if (c == '\n')
			printf ("  \\n ");

		else if (c >= 32 && c <= 127)
			printf ("   %c ", c);

		else if (c < 10)
			printf ("  \\%1i ", c);

		else if (c < 100)
			printf (" \\%2i ", c);

		else
			printf ("\\%3i ", c);
		if (i % 8 == 7)
			printf ("\n");
	}
	printf ("\n");
}

static void
Mst_SendList (void)
{
	byte        buf[MAX_DATAGRAM];
	sizebuf_t   msg;

	msg.data = buf;
	msg.maxsize = sizeof (buf);
	msg.cursize = 0;
	msg.allowoverflow = true;
	msg.overflowed = false;
	server_t   *sv;
	short int   sv_num = 0;


	// number of servers:
	for (sv = sv_list; sv; sv = sv->next)
		sv_num++;
	MSG_WriteByte (&msg, 255);
	MSG_WriteByte (&msg, 255);
	MSG_WriteByte (&msg, 255);
	MSG_WriteByte (&msg, 255);
	MSG_WriteByte (&msg, 255);
	MSG_WriteByte (&msg, 'd');
	MSG_WriteByte (&msg, '\n');
	if (sv_num > 0)
		for (sv = sv_list; sv; sv = sv->next) {
			MSG_WriteByte (&msg, sv->ip.ip[0]);
			MSG_WriteByte (&msg, sv->ip.ip[1]);
			MSG_WriteByte (&msg, sv->ip.ip[2]);
			MSG_WriteByte (&msg, sv->ip.ip[3]);
			MSG_WriteShort (&msg, sv->ip.port);
		}
	NET_SendPacket (msg.cursize, msg.data, net_from);
}

static void
Mst_Packet (void)
{
	char        msg;
	server_t   *sv;


	// NET_Filter();
	msg = net_message->message->data[1];
	if (msg == A2A_PING) {
		NET_Filter ();
		printf ("%s >> A2A_PING\n", NET_AdrToString (net_from));
		if (!(sv = SVL_Find (net_from))) {
			sv = SVL_New (&net_from);
			SVL_Add (sv);
		}
		sv->timeout = Sys_DoubleTime ();
	}

	else if (msg == S2M_HEARTBEAT) {
		NET_Filter ();
		printf ("%s >> S2M_HEARTBEAT\n", NET_AdrToString (net_from));
		if (!(sv = SVL_Find (net_from))) {
			sv = SVL_New (&net_from);
			SVL_Add (sv);
		}
		sv->timeout = Sys_DoubleTime ();
	}

	else if (msg == S2M_SHUTDOWN) {
		NET_Filter ();
		printf ("%s >> S2M_SHUTDOWN\n", NET_AdrToString (net_from));
		if ((sv = SVL_Find (net_from))) {
			SVL_Remove (sv);
			free (sv);
		}
	}

	else if (msg == 'c') {
		printf ("%s >> ", NET_AdrToString (net_from));
		printf ("Gamespy server list request\n");
		Mst_SendList ();
	}

	else {
		byte       *p;

		p = net_message->message->data;
		printf ("%s >> ", NET_AdrToString (net_from));
		printf ("Pingtool server list request\n");
		if (p[0] == 0 && p[1] == 'y') {
			Mst_SendList ();
		}

		else {
			printf ("%s >> ", NET_AdrToString (net_from));
			printf ("%c\n", net_message->message->data[1]);
			AnalysePacket ();
		}
	}
}

void
SV_ReadPackets (void)
{
	while (NET_GetPacket ()) {
		Mst_Packet ();
	}
}

void
SV_ConnectionlessPacket (void)
{
	printf ("%s>>%s\n", NET_AdrToString (net_from), net_message->message->data);
}

int         argv_index_add;

static void
Cmd_FilterAdd (void)
{
	filter_t   *filter;
	netadr_t    to, from;

	if (Cmd_Argc () < 4 + argv_index_add) {
		printf
			("Invalid command parameters. Usage:\nfilter add x.x.x.x:port x.x.x.x:port\n\n");
		return;
	}
	NET_StringToAdr (Cmd_Argv (2 + argv_index_add), &from);
	NET_StringToAdr (Cmd_Argv (3 + argv_index_add), &to);
	if (to.port == 0)
		from.port = BigShort (PORT_SERVER);
	if (from.port == 0)
		from.port = BigShort (PORT_SERVER);
	if (!(filter = FL_Find (from))) {
		printf ("Added filter %s\t\t%s\n", Cmd_Argv (2 + argv_index_add),
				Cmd_Argv (3 + argv_index_add));
		filter = FL_New (&from, &to);
		FL_Add (filter);
	}

	else
		printf ("%s already defined\n\n", Cmd_Argv (2 + argv_index_add));
}

static void
Cmd_FilterRemove (void)
{
	filter_t   *filter;
	netadr_t    from;

	if (Cmd_Argc () < 3 + argv_index_add) {
		printf
			("Invalid command parameters. Usage:\nfilter remove x.x.x.x:port\n\n");
		return;
	}
	NET_StringToAdr (Cmd_Argv (2 + argv_index_add), &from);
	if ((filter = FL_Find (from))) {
		printf ("Removed %s\n\n", Cmd_Argv (2 + argv_index_add));
		FL_Remove (filter);
		free (filter);
	}

	else
		printf ("Cannot find %s\n\n", Cmd_Argv (2 + argv_index_add));
}

static void
Cmd_FilterList (void)
{
	filter_t   *filter;

	for (filter = filter_list; filter; filter = filter->next) {
		printf ("%s", NET_AdrToString (filter->from));
		printf ("\t\t%s\n", NET_AdrToString (filter->to));
	}
	if (filter_list == NULL)
		printf ("No filter\n");
	printf ("\n");
}

static void
Cmd_FilterClear (void)
{
	printf ("Removed all filters\n\n");
	FL_Clear ();
}

void
Cmd_Filter_f (void)
{
	argv_index_add = 0;
	if (!strcmp (Cmd_Argv (1), "add"))
		Cmd_FilterAdd ();

	else if (!strcmp (Cmd_Argv (1), "remove"))
		Cmd_FilterRemove ();

	else if (!strcmp (Cmd_Argv (1), "clear"))
		Cmd_FilterClear ();

	else if (Cmd_Argc () == 3) {
		argv_index_add = -1;
		Cmd_FilterAdd ();
	}

	else if (Cmd_Argc () == 2) {
		argv_index_add = -1;
		Cmd_FilterRemove ();
	}

	else
		Cmd_FilterList ();
}

static void
SV_WriteFilterList (void)
{
	FILE       *filters;

	if ((filters = fopen ("filters.ini", "wt"))) {
		if (filter_list == NULL) {
			fclose (filters);
			return;
		}
		filter_t   *filter;

		for (filter = filter_list; filter; filter = filter->next) {
			fprintf (filters, "%s", NET_AdrToString (filter->from));
			fprintf (filters, " %s\n", NET_AdrToString (filter->to));
		}
		fclose (filters);
	}
}

int         sv_mode;

void
SV_Shutdown (void)
{
	NET_Shutdown ();

	// write filter list
	SV_WriteFilterList ();
}

void
SV_GetConsoleCommands (void)
{
	const char *cmd;

	while (1) {
		cmd = Sys_ConsoleInput ();
		if (!cmd)
			break;
		Cbuf_AddText (mst_cbuf, cmd);
	}
}

#define SV_TIMEOUT 450
static void
SV_TimeOut (void)
{

	// Remove listed severs that havnt sent a heartbeat for some time
	double      time = Sys_DoubleTime ();
	server_t   *sv;
	server_t   *next;

	if (sv_list == NULL)
		return;
	for (sv = sv_list; sv;) {
		if (sv->timeout + SV_TIMEOUT < time) {
			next = sv->next;
			printf ("%s timed out\n", NET_AdrToString (sv->ip));
			SVL_Remove (sv);
			free (sv);
			sv = next;
		}

		else
			sv = sv->next;
	}
}
void
SV_Frame ()
{
	SV_GetConsoleCommands ();
	Cbuf_Execute_Stack (mst_cbuf);
	SV_TimeOut ();
	SV_ReadPackets ();
}

int
main (int argc, const char **argv)
{
	PI_Init ();

	sv_console_plugin = Cvar_Get ("sv_console_plugin", "server",
								  CVAR_ROM, 0, "Plugin used for the console");
	PI_RegisterPlugins (server_plugin_list);
	Con_Init (sv_console_plugin->string);
	if (con_module)
		con_module->data->console->cbuf = mst_cbuf;
	con_list_print = Sys_Printf;

	COM_InitArgv (argc, argv);
	Cmd_Init ();
	SV_InitNet ();
	printf ("Exe: " __TIME__ " " __DATE__ "\n");
	printf ("======== HW master initialized ========\n\n");
	while (1) {
		SV_Frame ();
	}
	return 0;
}
