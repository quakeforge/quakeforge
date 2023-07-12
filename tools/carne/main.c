#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

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
#include "QF/gib.h"
#include "QF/dstring.h"
#include "QF/va.h"

#include "gib_thread.h"
#include "gib_parse.h"

static bool carne_done = false;
static int carne_exitcode = 0;

static void
Carne_GIB_Exit_f (void)
{
	carne_done = true;
	// Put it in wait mode so that Cbuf_Execute_Stack clears out
	// we can then safely nuke the stack later
	cbuf_active->state = CBUF_STATE_WAIT;
	if (GIB_Argc() == 2)
		carne_exitcode = atoi (GIB_Argv(1));
}

static int
Carne_Execute_Script (const char *path, cbuf_args_t *args)
{
	QFile *file;
	cbuf_t *mbuf = Cbuf_New (GIB_Interpreter ());
	char *f;
	int len, i;

	file = Qopen (path, "r");
	if (file) {
		len = Qfilesize (file);
		f = (char *) malloc (len + 1);
		if (f) {
			f[len] = 0;
			Qread (file, f, len);
			// If there is a hash-bang, strip it out
			i = 0;
			if (f[0] == '#')
				for (; f[i] != '\n' && f[i+1]; i++);
			Cbuf_AddText (mbuf, f+i);
			GIB_DATA(mbuf)->script = malloc (sizeof (gib_script_t));
			GIB_DATA(mbuf)->script->file = strdup (path);
			GIB_DATA(mbuf)->script->text = strdup (f);
			GIB_DATA(mbuf)->script->refs = 1;
			free (f);
		}
		Qclose (file);
	} else {
		printf ("Could not open %s for reading: %s\n", path, strerror(errno));
		carne_exitcode = 1;
		goto ERROR;
	}

	if (gib_parse_error) {
		carne_exitcode = 2;
		goto ERROR;
	}

	//GIB_Function_Prepare_Args_D (mbuf, args->argv, args->argc);

	// Main loop
	while (1) {
		GIB_Thread_Execute ();
		Cbuf_Execute_Stack (mbuf);
		// Check if there is anything left to do
		if (carne_done || !GIB_DATA(mbuf)->program)
			break;
	}
ERROR:
	Cbuf_DeleteStack (mbuf);
	return carne_exitcode;
}

static int
Carne_Execute_Stdin (void)
{
	char linebuf[1024];
	cbuf_t *cbuf = Cbuf_New (GIB_Interpreter ());

	memset (linebuf, 0, sizeof(linebuf));

	while (fgets(linebuf, sizeof(linebuf)-1, stdin)) {
		GIB_Thread_Execute ();
		Cbuf_AddText (cbuf, linebuf);
		if (!gib_parse_error)
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
	cbuf_args_t *args;
	int result, i;

	// Initialize required QF subsystems
	Sys_Init ();
	GIB_Init (false); // No sandbox

	GIB_Builtin_Add ("exit", Carne_GIB_Exit_f);

	if (argc > 1) {
		// Prepare arguments
		args = Cbuf_ArgsNew ();
		for (i = 1; i < argc; i++)
			Cbuf_ArgsAdd (args, argv[i]);
		// Run the script
		result = Carne_Execute_Script (argv[1], args);
		Cbuf_ArgsDelete (args);
		return result;
	} else
		return Carne_Execute_Stdin ();
}
