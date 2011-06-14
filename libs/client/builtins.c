#include "QF/progs.h"

static builtin_t builtins[] = {
	{"makevectors",			CSQC_makevectors,		1},
	{"setorigin",			CSQC_setorigin,			2},
	{"setmodel",			CSQC_setmodel,			3},
	{"setsize",				CSQC_setsize,			4},

	{"sound",				CSQC_sound,				8},
	{"error",				CSQC_error,				10},
	{"objerror",			CSQC_objerror,			11},
	{"spawn",				CSQC_spawn,				14},
	{"remove",				CSQC_remove,			15},
	{"traceline",			CSQC_traceline,			16},
//	{"checkclient",			CSQC_no,				17},

	{"precache_sound",		CSQC_precache_sound,	19},
	{"precache_model",		CSQC_precache_model,	20},
//	{"stuffcmd",			CSQC_no,				21},
	{"findradius",			CSQC_findradius,		22},
//	{"bprint",				CSQC_no,				23},
//	{"sprint",				CSQC_no,				24},

	{"walkmove",			CSQC_walkmove,			32},

	{"droptofloor",			CSQC_droptofloor,		34},
	{"lightstyle",			CSQC_lightstyle,		35},

	{"checkbottom",			CSQC_checkbottom,		40},
	{"pointcontents",		CSQC_pointcontents,		41},

//	{"aim",					CSQC_no,				44},

	{"localcmd",			CSQC_localcmd,			46},
	{"changeyaw",			CSQC_changeyaw,			49},

//	{"writebyte",			CSQC_no,				52},
//	{"WriteBytes",			CSQC_no,				-1},
//	{"writechar",			CSQC_no,				53},
//	{"writeshort",			CSQC_no,				54},
//	{"writelong",			CSQC_no,				55},
//	{"writecoord",			CSQC_no,				56},
//	{"writeangle",			CSQC_no,				57},
//	{"WriteCoordV",			CSQC_no,				-1},
//	{"WriteAngleV",			CSQC_no,				-1},
//	{"writestring",			CSQC_no,				58},
//	{"writeentity",			CSQC_no,				59},

//	{"movetogoal",			SV_MoveToGoal,			67},
//	{"precache_file",		CSQC_no,				68},
	{"makestatic",			CSQC_makestatic,		69},
//	{"changelevel",			CSQC_no,				70},

//	{"centerprint",			CSQC_no,				73},
	{"ambientsound",		CSQC_ambientsound,		74},
	{"precache_model2",		CSQC_precache_model,	75},
	{"precache_sound2",		CSQC_precache_sound,	76},
//	{"precache_file2",		CSQC_no,				77},
//	{"setspawnparms",		CSQC_no,				78},

//	{"logfrag",				CSQC_no,				79},
//	{"infokey",				CSQC_no,				80},
//	{"multicast",			CSQC_no,				82},
};
