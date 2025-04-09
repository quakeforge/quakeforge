/*
	sv_pr_qwe.c

	qwe extentions needed by the KTPro mod

	Copyright (C) 2003 Bill Currie

	Author: Bill Currie <bill@taniwha.org>
	Date: 2003/11/17

	taken from qwe (actual author currently unkown, probably
	`highlander') and ported to qf

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cbuf.h"
#include "QF/cmd.h"
#include "QF/dstring.h"
#include "QF/idparse.h"
#include "QF/progs.h"
#include "QF/quakefs.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "qw/include/server.h"
#include "qw/include/sv_pr_qwe.h"
#include "qw/include/sv_progs.h"
#include "qw/include/sv_recorder.h"

typedef struct {
	pr_func_t   timeofday;
	pr_func_t   ConsoleCmd;
	pr_func_t   UserCmd;
} qwe_funcs_t;

static qwe_funcs_t qwe_funcs;

static void
PF_executecmd (progs_t *pr, void *data)
{
	int         old_other, old_self;	// mod_consolecmd will be executed, so
										// we need to store these

	old_self = *sv_globals.self;
	old_other = *sv_globals.other;

	Cbuf_Execute (sv_cbuf);

	*sv_globals.self = old_self;
	*sv_globals.other = old_other;
}

/*
	PF_tokanize

	tokenize string

	void tokanize(string)
*/

static void
PF_tokanize (progs_t *pr, void *data)
{
	const char *str;

	str = P_GSTRING (pr, 0);
	COM_TokenizeString (str, sv_args);
	cmd_args = sv_args;
}

/*
	PF_argc

	returns number of tokens (must be executed after PF_Tokanize!)

	float argc(void)
*/

static void
PF_argc (progs_t *pr, void *data)
{
	R_FLOAT (pr) = (float) Cmd_Argc ();
}

/*
	PF_argv

	returns token requested by user (must be executed after PF_Tokanize!)

	string argc(float)
*/

static void
PF_argv (progs_t *pr, void *data)
{
	int         num;

	num = (int) P_FLOAT (pr, 0);

	if (num < 0)
		num = 0;
	if (num > Cmd_Argc () - 1)
		num = Cmd_Argc () - 1;

	RETURN_STRING (pr, Cmd_Argv (num));
}

/*
	PF_teamfield

	string teamfield(.string field)
*/

static void
PF_teamfield (progs_t *pr, void *data)
{
	sv_fields.team_str = P_INT (pr, 0);
}

/*
	PF_substr

	string substr(string str, float start, float len)
*/

static void
PF_substr (progs_t *pr, void *data)
{
	const char *s;
	char       *tmp;
	int         start, len, l;

	s = P_GSTRING (pr, 0);
	start = (int) P_FLOAT (pr, 1);
	len = (int) P_FLOAT (pr, 2);
	l = strlen (s);

	if (start >= l || !len || !*s) {
		R_STRING (pr) = 0;
		return;
	}

	s += start;
	l -= start;

	if (len > l)
		len = l;

	tmp = Hunk_TempAlloc (0, len + 1);
	strncpy (tmp, s, len);
	tmp[len] = 0;

	RETURN_STRING (pr, tmp);
}

/*
	PF_strcat

	string strcat(string str1, string str2)
*/

static void
PF_strcat (progs_t *pr, void *data)
{
	RETURN_STRING (pr, PF_VarString (pr, 0, pr->pr_argc));
}

/*
	PF_strlen

	float strlen(string str)
*/

static void
PF_strlen (progs_t *pr, void *data)
{
	R_FLOAT (pr) = (float) strlen (P_GSTRING (pr, 0));
}

/*
	PF_str2byte

	float str2byte (string str)
*/

static void
PF_str2byte (progs_t *pr, void *data)
{
	R_FLOAT (pr) = (float) *P_GSTRING (pr, 0);
}

/*
	PF_str2short

	float str2short (string str)
*/

static void
PF_str2short (progs_t *pr, void *data)
{
	const byte *str = (byte *) P_GSTRING (pr, 0);

	R_FLOAT (pr) = (short) ((str[1] << 8) | str[0]);
}


/*
	PF_newstr

	string newstr (string str [, float size])
	The new string will be at least as big as size, if given.
*/
static void
PF_newstr (progs_t *pr, void *data)
{
	const char *s;
	dstring_t  *dstr;
	int         i;

	s = P_GSTRING (pr, 0);

	i = PR_NewMutableString (pr);
	dstr = PR_GetMutableString (pr, i);

	dstring_copystr (dstr, s);

	if (pr->pr_argc > 1 && P_FLOAT (pr, 1) > dstr->size) {
		int         s = dstr->size;
		dstr->size = P_FLOAT (pr, 1);
		dstring_adjust (dstr);
		memset (dstr->str + s, 0, dstr->size - s);
	}

	R_STRING (pr) = i;
}

/*
	PF_frestr

	void freestr (string str)
*/

static void
PF_freestr (progs_t *pr, void *data)
{
	PR_FreeString (pr, P_STRING (pr, 0));
}

/*
	PF_readcmd

	string readmcmd (string str)
*/

static void
PF_readcmd (progs_t *pr, void *data)
{
	const char *s;
	redirect_t  old;

	s = P_GSTRING (pr, 0);

	Cbuf_Execute (sv_cbuf);
	Cbuf_AddText (sv_cbuf, s);

	old = sv_redirected;
	if (old != RD_NONE)
		SV_EndRedirect ();

	SV_BeginRedirect (RD_MOD);
	Cbuf_Execute (sv_cbuf);
	RETURN_STRING (pr, outputbuf.str);
	SV_EndRedirect ();

	if (old != RD_NONE)
		SV_BeginRedirect (old);
}

/*
	PF_redirectcmd

	void redirectcmd (entity to, string str)
*/

static void
PF_redirectcmd (progs_t *pr, void *data)
{
	const char *s;
	int         entnum;
	extern redirect_t sv_redirected;

	if (sv_redirected)
		return;

	entnum = P_EDICTNUM (pr, 0);
	if (entnum < 1 || entnum > MAX_CLIENTS)
		PR_RunError (pr, "Parm 0 not a client");

	s = P_GSTRING (pr, 1);

	Cbuf_AddText (sv_cbuf, s);

	SV_BeginRedirect (RD_MOD + entnum);
	Cbuf_Execute (sv_cbuf);
	SV_EndRedirect ();
}

static void
PF_calltimeofday (progs_t *pr, void *data)
{
	date_t      date;
	dfunction_t *f;

	if ((f = PR_FindFunction (pr, "timeofday")) != NULL) {

		Sys_TimeOfDay (&date);

		PR_PushFrame (&sv_pr_state);
		auto params = PR_SaveParams (pr);
		PR_SetupParams (pr, 7, 1);
		P_FLOAT (pr, 0) = (float) date.sec;
		P_FLOAT (pr, 1) = (float) date.min;
		P_FLOAT (pr, 2) = (float) date.hour;
		P_FLOAT (pr, 3) = (float) date.day;
		P_FLOAT (pr, 4) = (float) date.mon;
		P_FLOAT (pr, 5) = (float) date.year;
		P_STRING (pr, 6) = PR_SetReturnString (pr, date.str);
		PR_ExecuteProgram (pr, (pr_func_t) (f - sv_pr_state.pr_functions));
		PR_RestoreParams (pr, params);
		PR_PopFrame (&sv_pr_state);
	}
}

/*
	PF_forcedemoframe

	void PF_forcedemoframe(float now)
	Forces demo frame
	if argument 'now' is set, frame is written instantly
*/

static void
PF_forcedemoframe (progs_t *pr, void *data)
{
	SVR_ForceFrame ();
	if (P_FLOAT (pr, 0) == 1)
		SVR_SendMessages ();
}


/*
	PF_strcpy

	void strcpy(string dst, string src)
*/

static void
PF_strcpy (progs_t *pr, void *data)
{
	dstring_copystr (P_DSTRING (pr, 0), P_GSTRING (pr, 1));
}

/*
	PF_strncpy

	void strcpy(string dst, string src, float count)
*/

static void
PF_strncpy (progs_t *pr, void *data)
{
	dstring_t  *dst = P_DSTRING (pr, 0);
	const char *src = P_GSTRING (pr, 1);
	size_t      count = P_FLOAT (pr, 2);

	dst->size = count;
	dstring_adjust (dst);
	strncpy (dst->str, src, count);
}


/*
	PF_strstr

	string strstr(string str, string sub)
*/

static void
PF_strstr (progs_t *pr, void *data)
{
	const char *str, *sub, *p;

	str = P_GSTRING (pr, 0);
	sub = P_GSTRING (pr, 1);

	if ((p = strstr (str, sub)) == NULL) {
		R_STRING (pr) = 0;
		return;
	}

	R_STRING (pr) = p - pr->pr_strings;
}

static inline void
clean_text (char *text)
{
	while (*text) {
		*text = sys_char_map[(byte) *text];
		text++;
	}
}

/*
	PF_log

	void log(string name, float console, string text)
*/

static void
PF_log (progs_t *pr, void *data)
{
	const char *name;
	char       *text;
	QFile      *file;

	name = va ("%s/%s.log", qfs_gamedir->dir.def, P_GSTRING (pr, 0));
	file = QFS_Open (name, "a");

	text = PF_VarString (pr, 2, pr->pr_argc);
	clean_text (text);

	if (P_FLOAT (pr, 1))
		Sys_Printf ("%c%s", 3, text);

	if (!file) {
		Sys_Printf ("coldn't open log file %s\n", name);
	} else {
		Qputs (file, text);
		Qflush (file);
		Qclose (file);
	}
}

/*
	PF_conprint
*/
static void
PF_conprint (progs_t *pr, void *data)
{
	Sys_Printf ("%c%s", 3, PF_VarString (pr, 0, pr->pr_argc));
}

#define QWE (PR_RANGE_QWE << PR_RANGE_SHIFT) |

#define bi(x,n,np,params...) {"QWE:"#x, PF_##x, n, np, {params}}
#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
static builtin_t builtins[] = {
	bi(executecmd,      QWE 83, 0),
	bi(tokanize,        QWE 84, 1, p(string)), /* sic */
	bi(argc,            QWE 85, 0),
	bi(argv,            QWE 86, 1, p(float)),
	bi(teamfield,       QWE 87, 1, p(field)),
	bi(substr,          QWE 88, 3, p(string), p(float), p(float)),
	bi(strcat,          QWE 89, -1),
	bi(strlen,          QWE 90, 1, p(string)),
	bi(str2byte,        QWE 91, 1, p(string)),
	bi(str2short,       QWE 92, 1, p(string)),
	bi(newstr,          QWE 93, 2, p(string), p(float)),
	bi(freestr,         QWE 94, 1, p(string)),
	bi(conprint,        QWE 95, -1),
	bi(readcmd,         QWE 96, 1, p(string)),
	bi(strcpy,          QWE 97, 2, p(string), p(string)),
	bi(strstr,          QWE 98, 2, p(string), p(string)),
	bi(strncpy,         QWE 99, 3, p(string), p(string), p(float)),
	bi(log,             QWE 100, 3, p(string), p(float), p(string)),
	bi(redirectcmd,     QWE 101, 2, p(entity), p(string)),
	bi(calltimeofday,   QWE 102, 0),
	bi(forcedemoframe,  QWE 103, 1, p(float)),
	{0}
};
#define LAST_QWE_BUILTIN 103

static struct {
	const char *name;
	pr_func_t  *field;
} qwe_func_list[] = {
	{"timeofday",			&qwe_funcs.timeofday},
	{"ConsoleCmd",			&qwe_funcs.ConsoleCmd},
	{"UserCmd",				&qwe_funcs.UserCmd},
	{"localinfoChanged",	&sv_funcs.LocalinfoChanged},
	{"UserInfo_Changed",	&sv_funcs.UserInfoChanged},
	{"ChatMessage",			&sv_funcs.ChatMessage},
};

static int
qwe_console_cmd (void)
{
	if (qwe_funcs.ConsoleCmd) {
		if (sv_redirected != RD_MOD) {
			*sv_globals.time = sv.time;
			*sv_globals.self = 0;
		}
		PR_ExecuteProgram (&sv_pr_state, qwe_funcs.ConsoleCmd);
		return (int) R_FLOAT (&sv_pr_state);
	}

	return false;
}

static int
qwe_user_cmd (void)
{
	if (qwe_funcs.UserCmd) {
		if (sv_redirected != RD_MOD) {
			*sv_globals.time = sv.time;
			*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, sv_player);
		}
		PR_ExecuteProgram (&sv_pr_state, qwe_funcs.UserCmd);
		return (int) R_FLOAT (&sv_pr_state);
	}

	return false;
}

static int
qwe_load_finish (progs_t *pr)
{
	edict_t    *ent;
	pr_def_t   *targetname;

	targetname = PR_FindField (pr, "targetname");
	ent = EDICT_NUM (pr, 0);
	SVstring (ent, netname) = PR_SetString (pr, PACKAGE_VERSION);//FIXME
	if (targetname)
		E_STRING (ent, targetname->ofs) = PR_SetString (pr, "MVDSV");
	SVfloat (ent, impulse) = 0;//QWE_VERSION;//FIXME
	SVfloat (ent, items) = LAST_QWE_BUILTIN;
	return 1;
}

static int
qwe_load (progs_t *pr)
{
	size_t      i;

	for (i = 0; i < sizeof (qwe_func_list) / sizeof (qwe_func_list[0]); i++) {
		dfunction_t *f = PR_FindFunction (pr, qwe_func_list[i].name);

		*qwe_func_list[i].field = 0;
		if (f)
			*qwe_func_list[i].field = (pr_func_t) (f - pr->pr_functions);
	}

	sv_cbuf->unknown_command = qwe_console_cmd;
	ucmd_unknown = qwe_user_cmd;
	PR_AddLoadFinishFunc (pr, qwe_load_finish);
	return 1;
}

void
SV_PR_QWE_Init (progs_t *pr)
{
	PR_RegisterBuiltins (pr, builtins, 0);
	PR_AddLoadFunc (pr, qwe_load);
}
