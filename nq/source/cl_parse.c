
/*
	cl_parse.c

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

	$Id$
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

#include "client.h"
#include "host.h"
#include "QF/cdaudio.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/msg.h"
#include "QF/sys.h"
#include "sbar.h"
#include "QF/screen.h"
#include "server.h"
#include "QF/sound.h" // FIXME: DEFAULT_SOUND_PACKET_*
#include "game.h"
#include "QF/input.h"


char       *svc_strings[] = {
	"svc_bad",
	"svc_nop",
	"svc_disconnect",
	"svc_updatestat",
	"svc_version",						// [long] server version
	"svc_setview",						// [short] entity number
	"svc_sound",						// <see code>
	"svc_time",							// [float] server time
	"svc_print",						// [string] null terminated string
	"svc_stufftext",					// [string] stuffed into client's
	// console buffer
	// the string should be \n terminated
	"svc_setangle",						// [vec3] set the view angle to this
	// absolute value

	"svc_serverinfo",					// [long] version
	// [string] signon string
	// [string]..[0]model cache [string]...[0]sounds cache
	// [string]..[0]item cache
	"svc_lightstyle",					// [byte] [string]
	"svc_updatename",					// [byte] [string]
	"svc_updatefrags",					// [byte] [short]
	"svc_clientdata",					// <shortbits + data>
	"svc_stopsound",					// <see code>
	"svc_updatecolors",					// [byte] [byte]
	"svc_particle",						// [vec3] <variable>
	"svc_damage",						// [byte] impact [byte] blood [vec3]
	// from

	"svc_spawnstatic",
	"OBSOLETE svc_spawnbinary",
	"svc_spawnbaseline",

	"svc_temp_entity",					// <variable>
	"svc_setpause",
	"svc_signonnum",
	"svc_centerprint",
	"svc_killedmonster",
	"svc_foundsecret",
	"svc_spawnstaticsound",
	"svc_intermission",
	"svc_finale",						// [string] music [string] text
	"svc_cdtrack",						// [byte] track [byte] looptrack
	"svc_sellscreen",
	"svc_cutscene"
};


/*
	CL_EntityNum

	This error checks and tracks the total number of entities
*/
entity_t   *
CL_EntityNum (int num)
{
	if (num >= cl.num_entities) {
		if (num >= MAX_EDICTS)
			Host_Error ("CL_EntityNum: %i is an invalid number", num);
		while (cl.num_entities <= num) {
			cl_entities[cl.num_entities].colormap = vid.colormap;
			cl.num_entities++;
		}
	}

	return &cl_entities[num];
}


void
CL_ParseStartSoundPacket (void)
{
	vec3_t      pos;
	int         channel, ent;
	int         sound_num;
	int         volume;
	int         field_mask;
	float       attenuation;
	int         i;

	field_mask = MSG_ReadByte (net_message);

	if (field_mask & SND_VOLUME)
		volume = MSG_ReadByte (net_message);
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;

	if (field_mask & SND_ATTENUATION)
		attenuation = MSG_ReadByte (net_message) / 64.0;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;

	channel = MSG_ReadShort (net_message);
	sound_num = MSG_ReadByte (net_message);

	ent = channel >> 3;
	channel &= 7;

	if (ent > MAX_EDICTS)
		Host_Error ("CL_ParseStartSoundPacket: ent = %i", ent);

	for (i = 0; i < 3; i++)
		pos[i] = MSG_ReadCoord (net_message);

	S_StartSound (ent, channel, cl.sound_precache[sound_num], pos,
				  volume / 255.0, attenuation);
}


/*
	CL_KeepaliveMessage

	When the client is taking a long time to load stuff, send keepalive
	messages so the server doesn't disconnect.
*/
void
CL_KeepaliveMessage (void)
{
	float       time;
	static float lastmsg;
	int         ret;
	sizebuf_t   old;
	byte        olddata[8192];

	if (sv.active)
		return;							// no need if server is local
	if (cls.demoplayback)
		return;

// read messages from server, should just be nops
	old = *net_message->message;
	memcpy (olddata, net_message->message->data, net_message->message->cursize);

	do {
		ret = CL_GetMessage ();
		switch (ret) {
			default:
			Host_Error ("CL_KeepaliveMessage: CL_GetMessage failed");
			case 0:
			break;						// nothing waiting
			case 1:
			Host_Error ("CL_KeepaliveMessage: received a message");
			break;
			case 2:
			if (MSG_ReadByte (net_message) != svc_nop)
				Host_Error ("CL_KeepaliveMessage: datagram wasn't a nop");
			break;
		}
	} while (ret);

	*net_message->message = old;
	memcpy (net_message->message->data, olddata, net_message->message->cursize);

// check time
	time = Sys_DoubleTime ();
	if (time - lastmsg < 5)
		return;
	lastmsg = time;

// write out a nop
	Con_Printf ("--> client to server keepalive\n");

	MSG_WriteByte (&cls.message, clc_nop);
	NET_SendMessage (cls.netcon, &cls.message);
	SZ_Clear (&cls.message);
}

struct model_s **snd_worldmodel = &cl.worldmodel;


void
CL_ParseServerInfo (void)
{
	char       *str;
	int         i;
	int         nummodels, numsounds;
	char        model_precache[MAX_MODELS][MAX_QPATH];
	char        sound_precache[MAX_SOUNDS][MAX_QPATH];

	Con_DPrintf ("Serverinfo packet received.\n");

// wipe the client_state_t struct
//
	CL_ClearState ();

	// parse protocol version number
	i = MSG_ReadLong (net_message);
	if (i != PROTOCOL_VERSION) {
		Con_Printf ("Server returned version %i, not %i", i, PROTOCOL_VERSION);
		return;
	}
// parse maxclients
	cl.maxclients = MSG_ReadByte (net_message);
	if (cl.maxclients < 1 || cl.maxclients > MAX_SCOREBOARD) {
		Con_Printf ("Bad maxclients (%u) from server\n", cl.maxclients);
		return;
	}
	cl.scores = Hunk_AllocName (cl.maxclients * sizeof (*cl.scores), "scores");

// parse gametype
	cl.gametype = MSG_ReadByte (net_message);

// parse signon message
	str = MSG_ReadString (net_message);
	strncpy (cl.levelname, str, sizeof (cl.levelname) - 1);

// seperate the printfs so the server message can have a color
	Con_Printf
		("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
	Con_Printf ("%c%s\n", 2, str);

// first we go through and touch all of the precache data that still
// happens to be in the cache, so precaching something else doesn't
// needlessly purge it

// precache models
	memset (cl.model_precache, 0, sizeof (cl.model_precache));
	for (nummodels = 1;; nummodels++) {
		str = MSG_ReadString (net_message);
		if (!str[0])
			break;
		if (nummodels == MAX_MODELS) {
			Con_Printf ("Server sent too many model precaches\n");
			return;
		}
		strcpy (model_precache[nummodels], str);
		Mod_TouchModel (str);
	}

// precache sounds
	memset (cl.sound_precache, 0, sizeof (cl.sound_precache));
	for (numsounds = 1;; numsounds++) {
		str = MSG_ReadString (net_message);
		if (!str[0])
			break;
		if (numsounds == MAX_SOUNDS) {
			Con_Printf ("Server sent too many sound precaches\n");
			return;
		}
		strcpy (sound_precache[numsounds], str);
		S_TouchSound (str);
	}

// now we try to load everything else until a cache allocation fails

	for (i = 1; i < nummodels; i++) {
		cl.model_precache[i] = Mod_ForName (model_precache[i], false);
		if (cl.model_precache[i] == NULL) {
			Con_Printf ("Model %s not found\n", model_precache[i]);
			return;
		}
		CL_KeepaliveMessage ();
	}

	S_BeginPrecaching ();
	for (i = 1; i < numsounds; i++) {
		cl.sound_precache[i] = S_PrecacheSound (sound_precache[i]);
		CL_KeepaliveMessage ();
	}
	S_EndPrecaching ();


// local state
	cl_entities[0].model = cl.worldmodel = cl.model_precache[1];

	R_NewMap ();

	Hunk_Check ();						// make sure nothing is hurt

	noclip_anglehack = false;			// noclip is turned off at start    
}


int         bitcounts[16];


/*
	CL_ParseUpdate

	Parse an entity update message from the server
	If an entities model or origin changes from frame to frame, it must be
	relinked.  Other attributes can change without relinking.
*/
void
CL_ParseUpdate (int bits)
{
	int         i;
	model_t    *model;
	int         modnum;
	qboolean    forcelink;
	entity_t   *ent;
	int         num;
	int         skin;

	if (cls.signon == SIGNONS - 1) {	// first update is the final signon
		// stage
		cls.signon = SIGNONS;
		CL_SignonReply ();
	}

	if (bits & U_MOREBITS) {
		i = MSG_ReadByte (net_message);
		bits |= (i << 8);
	}

	if (bits & U_LONGENTITY)
		num = MSG_ReadShort (net_message);
	else
		num = MSG_ReadByte (net_message);

	ent = CL_EntityNum (num);

	for (i = 0; i < 16; i++)
		if (bits & (1 << i))
			bitcounts[i]++;

	if (ent->msgtime != cl.mtime[1])
		forcelink = true;				// no previous frame to lerp from
	else
		forcelink = false;

	ent->msgtime = cl.mtime[0];

	if (bits & U_MODEL) {
		modnum = MSG_ReadByte (net_message);
		if (modnum >= MAX_MODELS)
			Host_Error ("CL_ParseModel: bad modnum");
	} else
		modnum = ent->baseline.modelindex;

	model = cl.model_precache[modnum];
	if (model != ent->model) {
		ent->model = model;
		// automatic animation (torches, etc) can be either all together
		// or randomized
		if (model) {
			if (model->synctype == ST_RAND)
				ent->syncbase = (float) (rand () & 0x7fff) / 0x7fff;
			else
				ent->syncbase = 0.0;
		} else
			forcelink = true;			// hack to make null model players
		// work
		if (num > 0 && num <= cl.maxclients)
			R_TranslatePlayerSkin (num - 1);
	}

	if (bits & U_FRAME)
		ent->frame = MSG_ReadByte (net_message);
	else
		ent->frame = ent->baseline.frame;

	if (bits & U_COLORMAP)
		i = MSG_ReadByte (net_message);
	else
		i = ent->baseline.colormap;
	if (!i)
		ent->colormap = vid.colormap;
	else {
		if (i > cl.maxclients)
			Sys_Error ("i >= cl.maxclients");
		ent->colormap = cl.scores[i - 1].translations;
	}

	if (bits & U_SKIN)
		skin = MSG_ReadByte (net_message);
	else
		skin = ent->baseline.skin;
	if (skin != ent->skinnum) {
		ent->skinnum = skin;
		if (num > 0 && num <= cl.maxclients)
			R_TranslatePlayerSkin (num - 1);
	}

	if (bits & U_EFFECTS)
		ent->effects = MSG_ReadByte (net_message);
	else
		ent->effects = ent->baseline.effects;

// shift the known values for interpolation
	VectorCopy (ent->msg_origins[0], ent->msg_origins[1]);
	VectorCopy (ent->msg_angles[0], ent->msg_angles[1]);

	if (bits & U_ORIGIN1)
		ent->msg_origins[0][0] = MSG_ReadCoord (net_message);
	else
		ent->msg_origins[0][0] = ent->baseline.origin[0];
	if (bits & U_ANGLE1)
		ent->msg_angles[0][0] = MSG_ReadAngle (net_message);
	else
		ent->msg_angles[0][0] = ent->baseline.angles[0];

	if (bits & U_ORIGIN2)
		ent->msg_origins[0][1] = MSG_ReadCoord (net_message);
	else
		ent->msg_origins[0][1] = ent->baseline.origin[1];
	if (bits & U_ANGLE2)
		ent->msg_angles[0][1] = MSG_ReadAngle (net_message);
	else
		ent->msg_angles[0][1] = ent->baseline.angles[1];

	if (bits & U_ORIGIN3)
		ent->msg_origins[0][2] = MSG_ReadCoord (net_message);
	else
		ent->msg_origins[0][2] = ent->baseline.origin[2];
	if (bits & U_ANGLE3)
		ent->msg_angles[0][2] = MSG_ReadAngle (net_message);
	else
		ent->msg_angles[0][2] = ent->baseline.angles[2];

	if (bits & U_NOLERP)
		ent->forcelink = true;

	if (forcelink) {					// didn't have an update last message
		VectorCopy (ent->msg_origins[0], ent->msg_origins[1]);
		VectorCopy (ent->msg_origins[0], ent->origin);
		VectorCopy (ent->msg_angles[0], ent->msg_angles[1]);
		VectorCopy (ent->msg_angles[0], ent->angles);
		ent->forcelink = true;
	}
}


void
CL_ParseBaseline (entity_t *ent)
{
	int         i;

	ent->baseline.modelindex = MSG_ReadByte (net_message);
	ent->baseline.frame = MSG_ReadByte (net_message);
	ent->baseline.colormap = MSG_ReadByte (net_message);
	ent->baseline.skin = MSG_ReadByte (net_message);
	for (i = 0; i < 3; i++) {
		ent->baseline.origin[i] = MSG_ReadCoord (net_message);
		ent->baseline.angles[i] = MSG_ReadAngle (net_message);
	}
}


/*
	CL_ParseClientdata

	Server information pertaining to this client only
*/
void
CL_ParseClientdata (int bits)
{
	int         i, j;

	if (bits & SU_VIEWHEIGHT)
		cl.viewheight = MSG_ReadChar (net_message);
	else
		cl.viewheight = DEFAULT_VIEWHEIGHT;

	if (bits & SU_IDEALPITCH)
		cl.idealpitch = MSG_ReadChar (net_message);
	else
		cl.idealpitch = 0;

	VectorCopy (cl.mvelocity[0], cl.mvelocity[1]);
	for (i = 0; i < 3; i++) {
		if (bits & (SU_PUNCH1 << i))
			cl.punchangle[i] = MSG_ReadChar (net_message);
		else
			cl.punchangle[i] = 0;
		if (bits & (SU_VELOCITY1 << i))
			cl.mvelocity[0][i] = MSG_ReadChar (net_message) * 16;
		else
			cl.mvelocity[0][i] = 0;
	}

// [always sent]    if (bits & SU_ITEMS)
	i = MSG_ReadLong (net_message);

	if (cl.items != i) {				// set flash times
		Sbar_Changed ();
		for (j = 0; j < 32; j++)
			if ((i & (1 << j)) && !(cl.items & (1 << j)))
				cl.item_gettime[j] = cl.time;
		cl.items = i;
	}

	cl.onground = (bits & SU_ONGROUND) != 0;
	cl.inwater = (bits & SU_INWATER) != 0;

	if (bits & SU_WEAPONFRAME)
		cl.stats[STAT_WEAPONFRAME] = MSG_ReadByte (net_message);
	else
		cl.stats[STAT_WEAPONFRAME] = 0;

	if (bits & SU_ARMOR)
		i = MSG_ReadByte (net_message);
	else
		i = 0;
	if (cl.stats[STAT_ARMOR] != i) {
		cl.stats[STAT_ARMOR] = i;
		Sbar_Changed ();
	}

	if (bits & SU_WEAPON)
		i = MSG_ReadByte (net_message);
	else
		i = 0;
	if (cl.stats[STAT_WEAPON] != i) {
		cl.stats[STAT_WEAPON] = i;
		Sbar_Changed ();
	}

	i = MSG_ReadShort (net_message);
	if (cl.stats[STAT_HEALTH] != i) {
		cl.stats[STAT_HEALTH] = i;
		Sbar_Changed ();
	}

	i = MSG_ReadByte (net_message);
	if (cl.stats[STAT_AMMO] != i) {
		cl.stats[STAT_AMMO] = i;
		Sbar_Changed ();
	}

	for (i = 0; i < 4; i++) {
		j = MSG_ReadByte (net_message);
		if (cl.stats[STAT_SHELLS + i] != j) {
			cl.stats[STAT_SHELLS + i] = j;
			Sbar_Changed ();
		}
	}

	i = MSG_ReadByte (net_message);

	if (standard_quake) {
		if (cl.stats[STAT_ACTIVEWEAPON] != i) {
			cl.stats[STAT_ACTIVEWEAPON] = i;
			Sbar_Changed ();
		}
	} else {
		if (cl.stats[STAT_ACTIVEWEAPON] != (1 << i)) {
			cl.stats[STAT_ACTIVEWEAPON] = (1 << i);
			Sbar_Changed ();
		}
	}
}


void
CL_NewTranslation (int slot)
{
	int         i, j;
	int         top, bottom;
	byte       *dest, *source;

	if (slot > cl.maxclients)
		Sys_Error ("CL_NewTranslation: slot > cl.maxclients");
	dest = cl.scores[slot].translations;
	source = vid.colormap;
	memcpy (dest, vid.colormap, sizeof (cl.scores[slot].translations));
	top = cl.scores[slot].colors & 0xf0;
	bottom = (cl.scores[slot].colors & 15) << 4;
	R_TranslatePlayerSkin (slot);

	for (i = 0; i < VID_GRADES; i++, dest += 256, source += 256) {
		if (top < 128)					// the artists made some backwards
			// ranges.  sigh.
			memcpy (dest + TOP_RANGE, source + top, 16);
		else
			for (j = 0; j < 16; j++)
				dest[TOP_RANGE + j] = source[top + 15 - j];

		if (bottom < 128)
			memcpy (dest + BOTTOM_RANGE, source + bottom, 16);
		else
			for (j = 0; j < 16; j++)
				dest[BOTTOM_RANGE + j] = source[bottom + 15 - j];
	}
}


void
CL_ParseStatic (void)
{
	entity_t   *ent;
	int         i;

	i = cl.num_statics;
	if (i >= MAX_STATIC_ENTITIES)
		Host_Error ("Too many static entities");
	ent = &cl_static_entities[i];
	cl.num_statics++;
	CL_ParseBaseline (ent);

// copy it to the current state
	ent->model = cl.model_precache[ent->baseline.modelindex];
	ent->frame = ent->baseline.frame;
	ent->colormap = vid.colormap;
	ent->skinnum = ent->baseline.skin;
	ent->effects = ent->baseline.effects;

	VectorCopy (ent->baseline.origin, ent->origin);
	VectorCopy (ent->baseline.angles, ent->angles);
	R_AddEfrags (ent);
}


void
CL_ParseStaticSound (void)
{
	vec3_t      org;
	int         sound_num, vol, atten;
	int         i;

	for (i = 0; i < 3; i++)
		org[i] = MSG_ReadCoord (net_message);
	sound_num = MSG_ReadByte (net_message);
	vol = MSG_ReadByte (net_message);
	atten = MSG_ReadByte (net_message);

	S_StaticSound (cl.sound_precache[sound_num], org, vol, atten);
}


#define SHOWNET(x) if(cl_shownet->int_val==2)Con_Printf ("%3i:%s\n", net_message->readcount-1, x);

int snd_viewentity;

void
CL_ParseServerMessage (void)
{
	int         cmd;
	int         i;

// if recording demos, copy the message out
//
	if (cl_shownet->int_val == 1)
		Con_Printf ("%i ", net_message->message->cursize);
	else if (cl_shownet->int_val == 2)
		Con_Printf ("------------------\n");

	cl.onground = false;				// unless the server says otherwise 

// parse the message
//
	MSG_BeginReading (net_message);

	while (1) {
		if (net_message->badread)
			Host_Error ("CL_ParseServerMessage: Bad server message");

		cmd = MSG_ReadByte (net_message);

		if (cmd == -1) {
			SHOWNET ("END OF MESSAGE");
			return;						// end of message
		}
		// if the high bit of the command byte is set, it is a fast update
		if (cmd & 128) {
			SHOWNET ("fast update");
			CL_ParseUpdate (cmd & 127);
			continue;
		}

		SHOWNET (svc_strings[cmd]);

		// other commands
		switch (cmd) {
			default:
				Host_Error ("CL_ParseServerMessage: Illegible server message\n");
				break;

			case svc_nop:
//				Con_Printf ("svc_nop\n");
				break;

			case svc_time:
				cl.mtime[1] = cl.mtime[0];
				cl.mtime[0] = MSG_ReadFloat (net_message);
				break;

			case svc_clientdata:
				i = MSG_ReadShort (net_message);
				CL_ParseClientdata (i);
				break;

			case svc_version:
				i = MSG_ReadLong (net_message);
				if (i != PROTOCOL_VERSION)
					Host_Error
						("CL_ParseServerMessage: Server is protocol %i instead of %i\n",
						 i, PROTOCOL_VERSION);
				break;

			case svc_disconnect:
				Host_EndGame ("Server disconnected\n");

			case svc_print:
				Con_Printf ("%s", MSG_ReadString (net_message));
				break;

			case svc_centerprint:
				SCR_CenterPrint (MSG_ReadString (net_message));
				break;

			case svc_stufftext:
				Cbuf_AddText (MSG_ReadString (net_message));
				break;

			case svc_damage:
				V_ParseDamage ();
				break;

			case svc_serverinfo:
				CL_ParseServerInfo ();
				vid.recalc_refdef = true;	// leave intermission full screen
				break;

			case svc_setangle:
				for (i = 0; i < 3; i++)
					cl.viewangles[i] = MSG_ReadAngle (net_message);
				break;

			case svc_setview:
				cl.viewentity = MSG_ReadShort (net_message);
				snd_viewentity = cl.viewentity; // FIXME: evil hack
				break;

			case svc_lightstyle:
				i = MSG_ReadByte (net_message);
				if (i >= MAX_LIGHTSTYLES)
					Sys_Error ("svc_lightstyle > MAX_LIGHTSTYLES");
				strcpy (cl_lightstyle[i].map, MSG_ReadString (net_message));
				cl_lightstyle[i].length = strlen (cl_lightstyle[i].map);
				break;

			case svc_sound:
				CL_ParseStartSoundPacket ();
				break;

			case svc_stopsound:
				i = MSG_ReadShort (net_message);
				S_StopSound (i >> 3, i & 7);
				break;

			case svc_updatename:
				Sbar_Changed ();
				i = MSG_ReadByte (net_message);
				if (i >= cl.maxclients)
					Host_Error
						("CL_ParseServerMessage: svc_updatename > MAX_SCOREBOARD");
					strcpy (cl.scores[i].name, MSG_ReadString (net_message));
				break;

			case svc_updatefrags:
				Sbar_Changed ();
				i = MSG_ReadByte (net_message);
				if (i >= cl.maxclients)
					Host_Error
						("CL_ParseServerMessage: svc_updatefrags > MAX_SCOREBOARD");
				cl.scores[i].frags = MSG_ReadShort (net_message);
				break;

			case svc_updatecolors:
				Sbar_Changed ();
				i = MSG_ReadByte (net_message);
				if (i >= cl.maxclients)
					Host_Error
						("CL_ParseServerMessage: svc_updatecolors > MAX_SCOREBOARD");
				cl.scores[i].colors = MSG_ReadByte (net_message);
				CL_NewTranslation (i);
				break;

			case svc_particle:
				R_ParseParticleEffect ();
				break;

			case svc_spawnbaseline:
				i = MSG_ReadShort (net_message);
				// must use CL_EntityNum() to force cl.num_entities up
				CL_ParseBaseline (CL_EntityNum (i));
				break;
			case svc_spawnstatic:
				CL_ParseStatic ();
				break;
			case svc_temp_entity:
				CL_ParseTEnt ();
				break;

			case svc_setpause:
			{
				cl.paused = MSG_ReadByte (net_message);

				if (cl.paused) {
					CDAudio_Pause ();
					IN_HandlePause (true);
					VID_HandlePause (true);
				} else {
					VID_HandlePause (false);
					IN_HandlePause (false);
					CDAudio_Resume ();
				}
			}
			break;

			case svc_signonnum:
				i = MSG_ReadByte (net_message);
				if (i <= cls.signon)
					Host_Error ("Received signon %i when at %i", i, cls.signon);
				cls.signon = i;
				CL_SignonReply ();
				break;

			case svc_killedmonster:
				cl.stats[STAT_MONSTERS]++;
				break;

			case svc_foundsecret:
				cl.stats[STAT_SECRETS]++;
				break;

			case svc_updatestat:
				i = MSG_ReadByte (net_message);
				if (i < 0 || i >= MAX_CL_STATS)
					Sys_Error ("svc_updatestat: %i is invalid", i);
				cl.stats[i] = MSG_ReadLong (net_message);;
				break;

			case svc_spawnstaticsound:
				CL_ParseStaticSound ();
				break;

			case svc_cdtrack:
				cl.cdtrack = MSG_ReadByte (net_message);
				cl.looptrack = MSG_ReadByte (net_message);
				if ((cls.demoplayback || cls.demorecording)
					&& (cls.forcetrack != -1))
					CDAudio_Play ((byte) cls.forcetrack, true);
				else
					CDAudio_Play ((byte) cl.cdtrack, true);
				break;

			case svc_intermission:
				cl.intermission = 1;
				cl.completed_time = cl.time;
				vid.recalc_refdef = true;	// go to full screen
				break;

			case svc_finale:
				cl.intermission = 2;
				cl.completed_time = cl.time;
				vid.recalc_refdef = true;	// go to full screen
				SCR_CenterPrint (MSG_ReadString (net_message));
				break;

			case svc_cutscene:
				cl.intermission = 3;
				cl.completed_time = cl.time;
				vid.recalc_refdef = true;	// go to full screen
				SCR_CenterPrint (MSG_ReadString (net_message));
				break;

			case svc_sellscreen:
				Cmd_ExecuteString ("help", src_command);
				break;
		}
	}
}
