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
#include "QF/dstring.h"
#include "QF/va.h"

extern gib_thread_t *gib_threads;

int main (int argc, char **argv)
{
	QFile *file;
	char *f;
	int len, i;
	cbuf_args_t *args = Cbuf_ArgsNew ();
	cbuf_t *mbuf = Cbuf_New (&gib_interp);
	
	// Initialize required QF subsystems
	Cvar_Init_Hash ();
	Cmd_Init_Hash ();
	Cmd_Init ();	
	GIB_Init ();
	
	// Load the script
	file = Qopen (argv[1], "r");
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
		printf ("Could not open %s for reading: %s\n", argv[1], strerror(errno));
		return 1;
	}
	
	// If there is a hash-bang, strip it out
	if (mbuf->buf->str[0] == '#') {
		for (i = 0; mbuf->buf->str[i] != '\n' && mbuf->buf->str[i+1]; i++);
		dstring_snip (mbuf->buf, 0, i+1);
	}
	GIB_Parse_Strip_Comments (mbuf);
	
	// Prepare arguments
	for (i = 1; i < argc; i++)
		Cbuf_ArgsAdd (args, argv[i]);
	GIB_Function_Prepare_Args (mbuf, args);
	Cbuf_ArgsDelete (args);
	
	// Main loop
	while (1) {
		GIB_Thread_Execute ();
		Cbuf_Execute_Stack (mbuf);
		// Check if there is anything left to do
		if (!gib_threads && !mbuf->down && !mbuf->buf->str[0])
			return 0;
	}
}
