#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <string.h>
#include <ctype.h>
#include "QF/cvar.h"
#include "QF/console.h"
#include "QF/qargs.h"
#include "QF/cmd.h"
#include "QF/zone.h"
#include "QF/quakefs.h"
#include "gib.h"
#include "gib_instructions.h"
#include "gib_interpret.h"
#include "gib_modules.h"
#include "gib_parse.h"
#include "gib_vars.h"


// Standard cvars

void
GIB_Init (void)
{
	Cmd_AddCommand ("gibload", GIB_Load_f, "No Description");
	Cmd_AddCommand ("gibstats", GIB_Stats_f, "No Description");
	Cmd_AddCommand ("gib", GIB_Gib_f, "No Description");
	GIB_Init_Instructions ();
}

void
GIB_Gib_f (void)
{
	gib_sub_t  *sub;
	gib_module_t *mod;
	int         i, ret;

	if (!(mod = GIB_Get_ModSub_Mod (Cmd_Argv (1)))) {
		Con_Printf ("Module not found!\n");
		return;
	}
	if (!(sub = GIB_Get_ModSub_Sub (Cmd_Argv (1))))
		Con_Printf ("Subroutine not found!\n");
	else {
		gib_subargc = Cmd_Argc () - 1;
		gib_subargv[0] = sub->name;
		for (i = 1; i <= gib_subargc; i++)
			gib_subargv[i] = Cmd_Argv (i + 1);
		ret = GIB_Run_Sub (mod, sub);
		if (ret != 0)
			Con_Printf ("Error in execution of %s!\nError code: %i\n",
						Cmd_Argv (1), ret);
	}
}

void
GIB_Load_f (void)
{
	char        filename[256];
	QFile      *f;

	snprintf (filename, sizeof (filename), "%s/%s.gib", com_gamedir,
			  Cmd_Argv (1));
	f = Qopen (filename, "r");
	if (f) {
		GIB_Module_Load (Cmd_Argv (1), f);
		Qclose (f);
	} else
		Con_Printf ("gibload: File not found.\n");
}
