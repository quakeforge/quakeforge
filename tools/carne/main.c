#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/sys.h"
#include "QF/cvar.h"
#include "QF/zone.h"
#include "QF/quakefs.h"
#include "QF/quakeio.h"
#include "QF/gib_parse.h"
#include "QF/gib_init.h"
#include "QF/gib_thread.h"
#include "QF/gib_function.h"
#include "QF/gib_builtin.h"
#include "QF/gib_buffer.h"
#include "QF/dstring.h"
#include "QF/va.h"

extern gib_thread_t *gib_threads;

static qboolean carne_done = false;
static int carne_exitcode = 0;

void
Carne_GIB_Exit_f (void)
{
	carne_done = true;
	// Put it in wait mode so that Cbuf_Execute_Stack clears out
	// we can then safely nuke the stack later
	cbuf_active->state = CBUF_STATE_WAIT;
	if (GIB_Argc() == 2)
		carne_exitcode = atoi (GIB_Argv(1));
}

int
Carne_Execute_Script (const char *path, cbuf_args_t *args)
{
	QFile *file;
	cbuf_t *mbuf = Cbuf_New (&gib_interp);
	char *f;
	int len, i;
	
	file = Qopen (path, "r");
	if (file) {
		len = Qfilesize (file);
		f = (char *) malloc (len + 1);
		if (f) {
			f[len] = 0;
			Qread (file, f, len);
			Cbuf_InsertText (mbuf, f);
			free (f);
		}
		Qclose (file);
	} else {
		printf ("Could not open %s for reading: %s\n", path, strerror(errno));
		return 1;
	}
	
	// If there is a hash-bang, strip it out
	if (mbuf->buf->str[0] == '#') {
		for (i = 0; mbuf->buf->str[i] != '\n' && mbuf->buf->str[i+1]; i++);
		dstring_snip (mbuf->buf, 0, i+1);
	}
	GIB_Parse_Strip_Comments (mbuf);
	
	GIB_Function_Prepare_Args (mbuf, args);
	
	// Main loop
	while (1) {
		GIB_Thread_Execute ();
		Cbuf_Execute_Stack (mbuf);
		// Check if there is anything left to do
		if (carne_done || (!gib_threads && !mbuf->down && !mbuf->buf->str[0]))
			break;
	}
	Cbuf_DeleteStack (mbuf);
	return carne_exitcode;
}

int
Carne_Execute_Stdin ()
{
	char linebuf[1024];
	cbuf_t *cbuf = Cbuf_New (&gib_interp);
	
	memset (linebuf, 0, sizeof(linebuf));
	
	while (fgets(linebuf, sizeof(linebuf)-1, stdin)) {
		GIB_Thread_Execute ();
		Cbuf_AddText (cbuf, linebuf);
		Cbuf_Execute_Stack (cbuf);
		if (carne_done)
			break;
	}
	Cbuf_DeleteStack (cbuf);
	return carne_exitcode;
}

int
main (int argc, char **argv)
{
	cbuf_args_t *args = Cbuf_ArgsNew ();
	int result, i;
	
	// Initialize required QF subsystems
	Cvar_Init_Hash ();
	Cmd_Init_Hash ();
	Cmd_Init ();	
	GIB_Init (false); // No sandbox
	
	GIB_Builtin_Add ("exit", Carne_GIB_Exit_f, GIB_BUILTIN_NORMAL);
	
	if (argc > 1) {
		// Prepare arguments
		for (i = 1; i < argc; i++)
			Cbuf_ArgsAdd (args, argv[i]);
		// Run the script
		result = Carne_Execute_Script (argv[1], args);
		Cbuf_ArgsDelete (args);
		return result;
	} else
		return Carne_Execute_Stdin ();
}
