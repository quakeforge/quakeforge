/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

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
#if defined(_WIN32) && defined(HAVE_MALLOC_H)
# include <malloc.h>
#endif
#include <ctype.h>

#include "qfalloca.h"

#include "QF/cmd.h"
#include "QF/dstring.h"
#include "QF/quakefs.h"
#include "QF/sys.h"

#include "qw/include/server.h"
#include "qw/include/sv_pr_cpqw.h"
#include "qw/include/sv_progs.h"
#include "world.h"

static struct {
	func_t      ClientCommand;
} cpqw_funcs;

/*
	PF_getuid

	float(entity client) getuid
*/
static void
PF_getuid (progs_t *pr)
{
	edict_t    *client_ent;
	int         e_num;

	float       retval = 0;

	client_ent = P_EDICT (pr, 0);
	e_num = NUM_FOR_EDICT (pr, client_ent);

	if (e_num <= MAX_CLIENTS && e_num >= 1)
		retval = (float) svs.clients[e_num - 1].userid;

	R_FLOAT (pr) = retval;
}

/*
	PF_strcat

	string(string st1, string st2) strcat
*/
static void
PF_strcat (progs_t *pr)
{
	const char *st1 = P_GSTRING (pr, 0);
	const char *st2 = P_GSTRING (pr, 1);

	R_STRING (pr) = PR_CatStrings (pr, st1, st2);
}

/*
	PF_padstr

	string(string st, float len) padstr
*/
static void
PF_padstr (progs_t *pr)
{
	const char *st;
	size_t      i, padlen, givenlen;
	char       *padding;

	st = P_GSTRING (pr, 0);
	padlen = (unsigned int) P_FLOAT (pr, 1);
	givenlen = strlen (st);

	// Check if nothing should be done due to error or no need to..
	if ( (padlen <= givenlen)) {			// nothing should be done
		R_INT (pr) = P_STRING (pr, 0);		// return given string
		return;
	}
	// ok, lets pad it!
	padlen -= givenlen;
	padding = alloca (padlen + 1);

	for (i = 0; i < padlen; i++)
		padding[i] = ' ';
	padding[i] = '\0';

	R_STRING (pr) = PR_CatStrings (pr, st, padding);
}

/*
	PF_colstr

	string(string srcstr, float action) colstr
*/
// Possible action values
#define COLSTR_WHITE   0				// converts any red chars to white
#define COLSTR_MIX1    1				// mix red and white chars
#define COLSTR_RED     2				// converts any white chars to red
#define COLSTR_MIX2    3				// mix red and white chars
#define COLSTR_NUMBER  4				// converts any numbers to special
										// number chars
#define COLSTR_NUMBERV 5				// second variant of special number
										// chars (different on only some
										// non-original charsets)

static void
PF_colstr (progs_t *pr)
{
	const char *srcstr;
	unsigned char *result;
	unsigned    action;
	size_t      len, i;

	srcstr = P_GSTRING (pr, 0);
	action = (unsigned) P_FLOAT (pr, 1);
	len = strlen (srcstr);

	// Check for errors, if any, return given string
	if (len == 0 || action > COLSTR_NUMBERV) {
		R_INT (pr) = P_STRING (pr, 0);	// return given string
		return;
	}
	// Process string..
	result = alloca (len + 1);
	strcpy ((char *) result, srcstr);

	switch (action) {
		case COLSTR_WHITE:

			for (i = 0; i < len; i++) {
				if (result[i] > 160)	// is red char
					result[i] -= 128;
			}

			break;

		case COLSTR_MIX1:

			for (i = 0; i < len; i++) {
				if (i & 0x01) {
					if (result[i] < 128 && result[i] > 32)	// is white char
						result[i] += 128;
				} else {
					if (result[i] > 160)	// is red char
						result[i] -= 128;
				}
			}

			break;

		case COLSTR_RED:

			for (i = 0; i < len; i++) {
				if (result[i] < 128 && result[i] > 32)	// is white char
					result[i] += 128;
			}

			break;

		case COLSTR_MIX2:

			for (i = 0; i < len; i++) {
				if (i & 0x01) {
					if (result[i] > 160)	// is red char
						result[i] -= 128;
				} else {
					if (result[i] < 128 && result[i] > 32)	// is white char
						result[i] += 128;
				}
			}

			break;

		case COLSTR_NUMBER:

			for (i = 0; i < len; i++) {
				if (result[i] > 47 && result[i] < 58)	// is a white number
					result[i] -= 30;
			}

			break;

		case COLSTR_NUMBERV:

			for (i = 0; i < len; i++) {
				if (result[i] > 47 && result[i] < 58)	// is a white number
					result[i] += 98;
			}
	}

	RETURN_STRING (pr, (char *) result);
}

/*
	PF_strcasecmp

	float(string st1, string st2) strcasecmp
*/
static void
PF_strcasecmp (progs_t *pr)
{
	const char *st1;
	const char *st2;

	st1 = P_GSTRING (pr, 0);
	st2 = P_GSTRING (pr, 1);

	R_FLOAT (pr) = (float) strncasecmp (st1, st2, 99999);
}

/*
	PF_strlen

	float(string st) strlen
*/

static void
PF_strlen (progs_t *pr)
{
	const char *st;

	st = P_GSTRING (pr, 0);

	R_FLOAT (pr) = (float) strlen (st);
}

static char
KK_qwchar (char c)
{
	c &= 0x7f;
	if (c >= ' ')
		return tolower ((byte) c);
	if (c >= 0x12 && c <= 0x1b)
		return c - 0x12 + '0';
	switch (c) {
		case 0x05:
		case 0x0e:
		case 0x0f:
		case 0x1c:
			return '.';
		case 0x10:
			return '[';
		case 0x11:
			return ']';
		default:
			return 'X';
	}
}

static edict_t *
KK_Match_Str2 (const char *substr)
{
	int         i, j, count;
	dstring_t  *lstr = dstring_new ();
	dstring_t  *lname = dstring_new ();
	client_t   *cl;
	edict_t    *retedict = 0;

	dstring_copystr (lstr, substr);
	for (j = 0; lstr->str[j]; j++)
		lstr->str[j] = KK_qwchar (lstr->str[j]);

	for (i = count = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++) {
		if (!cl->state)
			continue;
		dstring_copystr (lname, cl->name);
		for (j = 0; lname->str[j]; j++)
			lname->str[j] = KK_qwchar (lname->str[j]);
		if (strstr (lname->str, lstr->str)) {
			retedict = cl->edict;
			count++;
		}
	}
	if (count > 1)
		retedict = 0;
	dstring_delete (lstr);
	dstring_delete (lname);
	return retedict;
}

/*
	PF_getclient

	entity(string st) getclient
*/
static void
PF_getclient (progs_t *pr)
{
	edict_t    *ent;
	const char *st;
	int         i, uid;
	client_t   *cl;

	st = P_GSTRING (pr, 0);

	ent = (edict_t *) KK_Match_Str2 (st);

	// no substring match?
	if (!ent) {
		uid = atoi (st);				// lets assume its an user id

		if (uid != 0)					// was it even a number?
		{
			// then, lets see if a client with that userid is here
			for (i = 0, cl = svs.clients; i < MAX_CLIENTS && !ent; i++, cl++) {
				if (cl->userid == uid)	// yeah, found it
					ent = cl->edict;
			}
		}
	}
	// Failed retrieving an user by name and by userid, so return world
	if (!ent)
		ent = sv.edicts;

	RETURN_EDICT (pr, ent);
}

/*
==============
PF_mutedtime

float(entity client) mutedtime
==============
*/

static void
PF_mutedtime (progs_t *pr)
{
	edict_t    *client_ent;
	int         e_num;

	float       retval = 0;

	client_ent = P_EDICT (pr, 0);
	e_num = NUM_FOR_EDICT (pr, client_ent);

	if (e_num <= MAX_CLIENTS && e_num > 0) {
		if (realtime >= svs.clients[e_num - 1].lockedtill)
			retval = (float) 0;
		else
			retval = (float) svs.clients[e_num - 1].lockedtill - realtime;
	}

	R_FLOAT (pr) = retval;
}

/*
==============
PF_validatefile

float(string st) validatefile
==============
*/

static void
PF_validatefile (progs_t *pr)
{
	float       retval;
	QFile      *f;
	const char *st;

	st = P_GSTRING (pr, 0);

	f = QFS_FOpenFile (st);
	if (!f) {
		retval = 0.0;
	} else {
		retval = 1.0;
		Qclose (f);
	}

	R_FLOAT (pr) = retval;
}

/*
	PF_putsaytime

	void(entity client) putsaytime
*/

extern int  fp_messages, fp_persecond, fp_secondsdead;

static void
PF_putsaytime (progs_t *pr)
{
	edict_t    *client_ent;
	int         e_num, tmp;

	client_ent = P_EDICT (pr, 0);
	e_num = NUM_FOR_EDICT (pr, client_ent);

	if (e_num <= MAX_CLIENTS && e_num >= 1) {
		tmp = svs.clients[e_num - 1].whensaidhead - fp_messages + 1;
		if (tmp < 0)
			tmp = 10 + tmp;
		if (svs.clients[e_num - 1].whensaid[tmp]
			&& (realtime - svs.clients[e_num - 1].whensaid[tmp] <
				fp_persecond)) {
			svs.clients[e_num - 1].lockedtill = realtime + fp_secondsdead;
			return;
		}

		svs.clients[e_num - 1].whensaidhead++;
		if (svs.clients[e_num - 1].whensaidhead > 9)
			svs.clients[e_num - 1].whensaidhead = 0;
		svs.clients[e_num - 1].whensaid[svs.clients[e_num - 1].whensaidhead] =
			realtime;
	}
}

/*
	PF_makestr

	string(string st) makestr
*/

static void
PF_makestr (progs_t *pr)
{
	string_t    res = PR_NewMutableString (pr);
	dstring_t  *dst = PR_GetMutableString (pr, res);
    const char *src = P_GSTRING (pr, 0);

	dstring_copystr (dst, src);
	R_STRING (pr) = res;
}

/*
	PF_delstr

	void(string st) delstr
*/

static void
PF_delstr (progs_t *pr)
{
	PR_FreeString (pr, P_STRING (pr, 0));
}

/*
	PF_getwave

	float(float inputnum, float modes, float minnum, float maxnum,
		float balance, float offset, float shape) getwave
*/

static float
GetCircleWave (float inputnum)
{
	inputnum = inputnum - (int) inputnum;

	if (inputnum >= 0.75)				// -1 to 0
	{
		if (inputnum == 0.75)
			return -1;

		inputnum = inputnum - 0.75;

		inputnum = inputnum * 4;

		return (1 - sqrt (1 - (inputnum * inputnum))) - 1;
	} else if (inputnum >= 0.5)			// 0 to -1
	{
		if (inputnum == 0.5)
			return 0;

		inputnum = inputnum - 0.5;

		inputnum = 0.25 - inputnum;
		inputnum = inputnum * 4;

		return (1 - sqrt (1 - (inputnum * inputnum))) - 1;
	} else if (inputnum >= 0.25)		// 1 to 0
	{
		if (inputnum == 0.25)
			return 1;

		inputnum = inputnum - 0.25;

		inputnum = inputnum * 4;

		return sqrt (1 - (inputnum * inputnum));
	} else								// 0 to 1
	{
		if (inputnum == 0)
			return 0;

		inputnum = 0.25 - inputnum;
		inputnum = inputnum * 4;

		return sqrt (1 - (inputnum * inputnum));
	}

	return 0;
}

static float
GetLinearWave (float inputnum)
{
	inputnum = inputnum - (int) inputnum;

	if (inputnum <= 0.25)
		return inputnum / 0.25;
	else if (inputnum <= 0.5)
		return 1 - ((inputnum - 0.25) / 0.25);
	else if (inputnum <= 0.75)
		return -(inputnum - 0.5) / 0.25;
	else
		return -1 + (inputnum - 0.75) / 0.25;

	return 0;
}

// GetWave possible mode flags
#define GWAVE_STANDARD    0
#define GWAVE_USEMINMAX   1
#define GWAVE_USEBALANCE  2				// TODO: Unimplemented yet
#define GWAVE_USEOFFSET   4
#define GWAVE_USESHAPE    8

static void
PF_getwave (progs_t *pr)
{
	float       retval, inputnum, minnum, maxnum, balance, offset, shape;
	float       temp;

	unsigned int modes;

	inputnum = P_FLOAT (pr, 0);

	modes = (unsigned int) P_FLOAT (pr, 1);

	balance = P_FLOAT (pr, 4);
	(void) balance; //FIXME

	if (modes & GWAVE_USEOFFSET)
		offset = P_FLOAT (pr, 5);
	else
		offset = 0;

	// Use special shape?
	if (modes & GWAVE_USESHAPE) {
		shape = P_FLOAT (pr, 6);

		if (shape >= -1 && shape <= 1 && shape != 0) {
			if (shape < 0)				// sine/linear mix
			{
				if (shape == -1)		// full linear
					retval = GetLinearWave (inputnum + offset);
				else {
					// Get standard sinus
					retval = sin (2 * M_PI * (inputnum + offset));

					temp = GetLinearWave (inputnum + offset);
					retval =
						(retval * (1 - fabs (shape))) + (temp * fabs (shape));
				}
			} else						// sine/circular mix
			{
				if (shape == 1)			// full circular
					retval = GetCircleWave (inputnum + offset);
				else {
					// Get standard sinus
					retval = sin (2 * M_PI * (inputnum + offset));

					temp = GetCircleWave (inputnum + offset);
					retval = retval * (1 - shape) + temp * shape;
				}
			}
		} else							// 0 or invalid shape
		{
			// Get standard sinus
			retval = sin (2 * M_PI * (inputnum + offset));
		}
	} else								// dont use shape then..
	{
		// Get standard sinus
		retval = sin (2 * M_PI * (inputnum + offset));
	}

	// Use maximum/minimum values?
	if (modes & GWAVE_USEMINMAX) {
		minnum = P_FLOAT (pr, 2);
		maxnum = P_FLOAT (pr, 3);

		retval = minnum + ((retval + 1) / 2) * (maxnum - minnum);
	}
	// Return it!
	R_FLOAT (pr) = retval;
}

/*
	PF_clientsound

	void(entity client) clientsound
*/

static void
PF_clientsound (progs_t *pr)
{
	const char *sample;
	int         channel;
	edict_t    *entity;
	int         volume;
	float       attenuation;

	int         sound_num;
	int         i;
	pr_int_t    ent;
	vec3_t      origin;

	entity = P_EDICT (pr, 0);
	channel = P_FLOAT (pr, 1);
	sample = P_GSTRING (pr, 2);
	volume = P_FLOAT (pr, 3) * 255;
	attenuation = P_FLOAT (pr, 4);

	ent = NUM_FOR_EDICT (pr, entity);

	// If not a client go away
	if (ent > MAX_CLIENTS || ent < 1)
		return;

	if (volume < 0 || volume > 255)
		Sys_Error ("SV_StartSound: volume = %i", volume);

	if (attenuation < 0 || attenuation > 4)
		Sys_Error ("SV_StartSound: attenuation = %f", attenuation);

	if (channel < 0 || channel > 15)
		Sys_Error ("SV_StartSound: channel = %i", channel);

// find precache number for sound
	for (sound_num = 1; sound_num < MAX_SOUNDS
		 && sv.sound_precache[sound_num]; sound_num++)
		if (!strcmp (sample, sv.sound_precache[sound_num]))
			break;

	if (sound_num == MAX_SOUNDS || !sv.sound_precache[sound_num]) {
		Sys_Printf ("SV_StartSound: %s not precacheed\n", sample);
		return;
	}

	channel &= 7;

	channel = (ent << 3) | channel;

	if (volume != DEFAULT_SOUND_PACKET_VOLUME)
		channel |= SND_VOLUME;
	if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION)
		channel |= SND_ATTENUATION;

	// use the entity origin
	VectorCopy (SVvector (entity, origin), origin);

	MSG_WriteByte (&svs.clients[ent - 1].netchan.message, svc_sound);
	MSG_WriteShort (&svs.clients[ent - 1].netchan.message, channel);
	if (channel & SND_VOLUME)
		MSG_WriteByte (&svs.clients[ent - 1].netchan.message, volume);
	if (channel & SND_ATTENUATION)
		MSG_WriteByte (&svs.clients[ent - 1].netchan.message, attenuation * 64);
	MSG_WriteByte (&svs.clients[ent - 1].netchan.message, sound_num);
	for (i = 0; i < 3; i++)
		MSG_WriteCoord (&svs.clients[ent - 1].netchan.message, origin[i]);
}

/*
	PF_touchworld

	void() touchworld
*/

static void
PF_touchworld (progs_t *pr)
{
	edict_t    *self;
	pr_int_t    oself;

	oself = *sv_globals.self;

	self = PROG_TO_EDICT (pr, *sv_globals.self);
	SV_LinkEdict (self, true);

	*sv_globals.self = oself;
}

/*
	CPQW_traceline

	Used for use tracing and shot targeting.
	Traces are blocked by bbox and exact bsp entityes, and also slide box
	entities if the tryents flag is set.

	traceline (vector1, vector2, tryents)
	// float (vector v1, vector v2, float tryents) traceline
*/
#define TL_ANY_SOLID		0
#define TL_BSP_ONLY			1
#define TL_TRIGGERS			3	// scan for triggers
#define TL_EVERYTHING		4	// scan for anything

static void
CPQW_traceline (progs_t *pr)
{
	float      *v1, *v2;
	edict_t    *ent;
	int         nomonsters;
	trace_t     trace;
	static int  tl_to_move[] = {
		MOVE_NORMAL,
		MOVE_NORMAL,
		MOVE_NORMAL,
		MOVE_TRIGGERS,
		MOVE_EVERYTHING,
	};

	v1 = P_VECTOR (pr, 0);
	v2 = P_VECTOR (pr, 1);
	nomonsters = P_FLOAT (pr, 2);
	ent = P_EDICT (pr, 3);

	if (nomonsters < TL_ANY_SOLID || nomonsters > TL_EVERYTHING)
		nomonsters = TL_ANY_SOLID;
	nomonsters = tl_to_move[nomonsters];

	if (sv_antilag->int_val == 2)
		nomonsters |= MOVE_LAGGED;

	trace = SV_Move (v1, vec3_origin, vec3_origin, v2, nomonsters, ent);

	*sv_globals.trace_allsolid = trace.allsolid;
	*sv_globals.trace_startsolid = trace.startsolid;
	*sv_globals.trace_fraction = trace.fraction;
	*sv_globals.trace_inwater = trace.inwater;
	*sv_globals.trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, *sv_globals.trace_endpos);
	VectorCopy (trace.plane.normal, *sv_globals.trace_plane_normal);
	*sv_globals.trace_plane_dist = trace.plane.dist;

	if (trace.ent)
		*sv_globals.trace_ent = EDICT_TO_PROG (pr, trace.ent);
	else
		*sv_globals.trace_ent = EDICT_TO_PROG (pr, sv.edicts);
}

#define CPQW (PR_RANGE_CPQW << PR_RANGE_SHIFT) |

static builtin_t builtins[] = {
	{"CPCW:traceline",		CPQW_traceline,		CPQW 16},
	{"CPQW:getuid",			PF_getuid,			CPQW 83},
	{"CPQW:strcat",			PF_strcat,			CPQW 84},
	{"CPQW:padstr",			PF_padstr,			CPQW 85},
	{"CPQW:colstr",			PF_colstr,			CPQW 86},
	{"CPQW:strcasecmp",		PF_strcasecmp,		CPQW 87},
	{"CPQW:strlen",			PF_strlen,			CPQW 88},
	{"CPQW:getclient",		PF_getclient,		CPQW 89},
	{"CPQW:mutedtime",		PF_mutedtime,		CPQW 90},
	{"CPQW:validatefile",	PF_validatefile,	CPQW 91},
	{"CPQW:putsaytime",		PF_putsaytime,		CPQW 92},
	{"CPQW:makestr",		PF_makestr,			CPQW 93},
	{"CPQW:delstr",			PF_delstr,			CPQW 94},
	{"CPQW:getwave",		PF_getwave,			CPQW 95},
	{"CPQW:clientsound",	PF_clientsound,		CPQW 96},
	{"CPQW:touchworld",		PF_touchworld,		CPQW 97},
	{0}
};

static struct {
	const char *name;
	func_t     *field;
} cpqw_func_list[] = {
	{"ClientCommand",	&cpqw_funcs.ClientCommand},
	{"UserInfoChanged",	&sv_funcs.UserInfoChanged},
};

static int
cpqw_user_cmd (void)
{
	int         argc, i;
	progs_t    *pr = &sv_pr_state;

	if (cpqw_funcs.ClientCommand) {
		argc = Cmd_Argc ();
		if (argc > 7)
			argc = 7;

		*sv_globals.time = sv.time;
		*sv_globals.self = EDICT_TO_PROG (&sv_pr_state, sv_player);

		PR_PushFrame (pr);
		P_FLOAT (pr, 0) = argc;
		for (i = 1; i < argc + 1; i++)
			P_STRING (pr, i) = PR_SetTempString (pr, Cmd_Argv (i - 1));
		for (; i < 8; i++)
			P_STRING (pr, i) = 0;
		pr->pr_argc = 8;
		PR_ExecuteProgram (pr, cpqw_funcs.ClientCommand);
		PR_PopFrame (pr);
		return (int) R_FLOAT (pr);
	}
	return 0;
}

static int
cpqw_load (progs_t *pr)
{
	size_t      i;

	for (i = 0;
		 i < sizeof (cpqw_func_list) / sizeof (cpqw_func_list[0]); i++) {
		dfunction_t *f = PR_FindFunction (pr, cpqw_func_list[i].name);

		*cpqw_func_list[i].field = 0;
		if (f)
			*cpqw_func_list[i].field = (func_t) (f - pr->pr_functions);
	}
	ucmd_unknown = cpqw_user_cmd;
	return 1;
}

void
SV_PR_CPQW_Init (progs_t *pr)
{
	PR_RegisterBuiltins (pr, builtins);
	PR_AddLoadFunc (pr, cpqw_load);
}
