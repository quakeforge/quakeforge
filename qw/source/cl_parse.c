/*
	cl_parse.c

	parse a message received from the server

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
#ifdef HAVE_ERRNO_H
# include <errno.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "QF/cbuf.h"
#include "QF/cdaudio.h"
#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/draw.h"
#include "QF/dstring.h"
#include "QF/gib.h"
#include "QF/idparse.h"
#include "QF/msg.h"
#include "QF/progs.h"
#include "QF/plist.h"
#include "QF/quakeio.h"
#include "QF/screen.h"
#include "QF/skin.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/teamplay.h"
#include "QF/va.h"

#include "QF/scene/scene.h"

#include "compat.h"

#include "client/effects.h"
#include "client/particles.h"
#include "client/hud.h"
#include "client/screen.h"
#include "client/temp_entities.h"
#include "client/view.h"
#include "client/world.h"

#include "qw/bothdefs.h"
#include "qw/pmove.h"
#include "qw/protocol.h"

#include "qw/include/cl_cam.h"
#include "qw/include/cl_chat.h"
#include "qw/include/cl_ents.h"
#include "qw/include/cl_http.h"
#include "qw/include/cl_input.h"
#include "qw/include/cl_main.h"
#include "qw/include/cl_parse.h"
#include "qw/include/cl_skin.h"
#include "qw/include/client.h"
#include "qw/include/host.h"
#include "qw/include/map_cfg.h"

const char *svc_strings[] = {
	"svc_bad",
	"svc_nop",
	"svc_disconnect",
	"svc_updatestat",
	"svc_version",				// [long] server version
	"svc_setview",				// [short] entity number
	"svc_sound",				// <see code>
	"svc_time",					// [float] server time
	"svc_print",				// [string] null terminated string
	"svc_stufftext",			// [string] stuffed into client's console
								// buffer the string should be \n terminated
	"svc_setangle",				// [vec3] set view angle to this absolute value

	"svc_serverdata",			// [long] version ...
	"svc_lightstyle",			// [byte] [string]
	"svc_updatename",			// [byte] [string]
	"svc_updatefrags",			// [byte] [short]
	"svc_clientdata",			// <shortbits + data>
	"svc_stopsound",			// <see code>
	"svc_updatecolors",			// [byte] [byte]
	"svc_particle",				// [vec3] <variable>
	"svc_damage",				// [byte] impact [byte] blood [vec3] from

	"svc_spawnstatic",
	"OBSOLETE svc_spawnbinary",
	"svc_spawnbaseline",

	"svc_temp_entity",			// <variable>
	"svc_setpause",
	"svc_signonnum",
	"svc_centerprint",
	"svc_killedmonster",
	"svc_foundsecret",
	"svc_spawnstaticsound",
	"svc_intermission",
	"svc_finale",				// [string] text
	"svc_cdtrack",				// [byte] track
	"svc_sellscreen",
	"svc_smallkick",
	"svc_bigkick",
	"svc_updateping",
	"svc_updateentertime",
	"svc_updatestatlong",
	"svc_muzzleflash",
	"svc_updateuserinfo",
	"svc_download",
	"svc_playerinfo",
	"svc_nails",
	"svc_chokecount",
	"svc_modellist",
	"svc_soundlist",
	"svc_packetentities",
	"svc_deltapacketentities",
	"svc_maxspeed",
	"svc_entgravity",
	"svc_setinfo",
	"svc_serverinfo",
	"svc_updatepl",
	"svc_nails2",						// FIXME from qwex
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL",
	"NEW PROTOCOL"
};

int         oldparsecountmod;
int         parsecountmod;
double      parsecounttime;

int         cl_playerindex, cl_flagindex;
int         cl_h_playerindex, cl_gib1index, cl_gib2index, cl_gib3index;

int         packet_latency[NET_TIMINGS];

int
CL_CalcNet (void)
{
	int			lost, a, i;
	cl_frame_t *frame;

	for (i = cls.netchan.outgoing_sequence - UPDATE_BACKUP + 1;
		 i <= cls.netchan.outgoing_sequence; i++) {
		frame = &cl.frames[i & UPDATE_MASK];
		if (frame->receivedtime == -1) {
			packet_latency[i & NET_TIMINGSMASK] = 9999;		// dropped
		} else if (frame->receivedtime == -2) {
			packet_latency[i & NET_TIMINGSMASK] = 10000;	// choked
		} else if (frame->invalid) {
			packet_latency[i & NET_TIMINGSMASK] = 9998;		// invalid delta
		} else {
			double      d = frame->receivedtime - frame->senttime;
			d = log (d * 1000 + 1) / log (1000);
			d *= d * cl_netgraph_height;
			packet_latency[i & NET_TIMINGSMASK] = d;
		}
	}

	lost = 0;
	for (a = 0; a < NET_TIMINGS; a++) {
		i = (cls.netchan.outgoing_sequence - a) & NET_TIMINGSMASK;
		if (packet_latency[i] == 9999)
			lost++;
	}
	return lost * 100 / NET_TIMINGS;
}

/*
	CL_CheckOrDownloadFile

	Returns true if the file exists, otherwise it attempts
	to start a download from the server.
*/
bool
CL_CheckOrDownloadFile (const char *filename)
{
	QFile	   *f;

	if (strstr (filename, "..")) {
		Sys_Printf ("Refusing to download a path with ..\n");
		return true;
	}
/*
	if (!snd_initialized && strnequal ("sound/", filename, 6)) {
		// don't bother downloading sounds if we can't play them
		return true;
	}
*/
	f = QFS_FOpenFile (filename);
	if (f) {							// it exists, no need to download
		Qclose (f);
		return true;
	}
	// ZOID - can't download when recording
	if (cls.demorecording) {
		Sys_Printf ("%cUnable to download %s in record mode.\n", 3,
					cls.downloadname->str);
		return true;
	}
	// ZOID - can't download when playback
	if (cls.demoplayback)
		return true;

	dstring_copystr (cls.downloadname, filename);
	dstring_copystr (cls.downloadtempname, filename);
	Sys_Printf ("%cDownloading %s...\n", 3, cls.downloadname->str);

	// download to a temp name, and rename to the real name only when done,
	// so if interrupted a runt file wont be left
	QFS_StripExtension (cls.downloadname->str, cls.downloadtempname->str);
	dstring_appendstr (cls.downloadtempname, ".tmp");

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message,
					 va ("download \"%s\"", cls.downloadname->str));

	cls.downloadnumber++;

	return false;
}

static void
CL_NewMap (const char *mapname)
{
	const char *skyname = 0;
	// R_LoadSkys does the right thing with null pointers.
	if (cl.serverinfo) {
		skyname = Info_ValueForKey (cl.serverinfo, "sky");
	}
	CL_World_NewMap (mapname, skyname);
	V_NewScene (&cl.viewstate, cl_world.scene);
	cl.chasestate.worldmodel = cl_world.scene->worldmodel;

	Team_NewMap ();
	Con_NewMap ();
	Sbar_CenterPrint (0);

	Hunk_Check (0);								// make sure nothing is hurt
}

static void
Model_NextDownload (void)
{
	char	   *s;
	int			i;

	if (cls.downloadnumber == 0) {
		Sys_Printf ("Checking models...\n");
		cl.viewstate.time = realtime;
		cl.viewstate.realtime = realtime;
		cl_realtime = realtime;
		cl_frametime = host_frametime;
		CL_UpdateScreen (&cl.viewstate);
		cls.downloadnumber = 1;
	}

	cls.downloadtype = dl_model;
	for (; cl.model_name[cls.downloadnumber][0]; cls.downloadnumber++) {
		s = cl.model_name[cls.downloadnumber];
		if (s[0] == '*')
			continue;							// inline brush model
		if (!CL_CheckOrDownloadFile (s))
			return;								// started a download
	}

	CL_MapCfg (cl.model_name[1]);

	for (i = 1; i < cl.nummodels; i++) {
		const char *info_key = 0;

		if (!cl.model_name[i][0])
			break;

		DARRAY_APPEND (&cl_world.models,
					   Mod_ForName (cl.model_name[i], false));

		if (!cl_world.models.a[i]) {
			Sys_Printf ("%c\nThe required model file '%s' could not be found or"
						" downloaded.\n\n", 3, cl.model_name[i]);
			Sys_Printf ("You may need to download or purchase a %s client "
						"pack in order to play on this server.\n\n",
						qfs_gamedir->gamedir);
			CL_Disconnect ();
			return;
		}

		if (strequal (cl.model_name[i], "progs/player.mdl")
			&& cl_world.models.a[i]->type == mod_mesh) {
			info_key = pmodel_name;
		}
		if (strequal (cl.model_name[i], "progs/eyes.mdl")
			&& cl_world.models.a[i]->type == mod_mesh)
			info_key = emodel_name;

		if (info_key && cl_model_crcs) {
			auto model = cl_world.models.a[i]->model;
			if (!model)
				model = Cache_Get (&cl_world.models.a[i]->cache);
			Info_SetValueForKey (cls.userinfo, info_key, va ("%d",
															 model->crc),
								 0);
			if (!cls.demoplayback) {
				MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
				SZ_Print (&cls.netchan.message, va ("setinfo %s %d",
													info_key, model->crc));
			}
			if (!cl_world.models.a[i]->model)
				Cache_Release (&cl_world.models.a[i]->cache);
		}
	}

	// Something went wrong (probably in the server, probably a TF server)
	// We need to disconnect gracefully.
	if (!cl_world.models.a[1]) {
		Sys_Printf ("\nThe server has failed to provide the map name.\n\n");
		Sys_Printf ("Disconnecting to prevent a crash.\n\n");
		CL_Disconnect ();
		return;
	}

	// all done
	CL_NewMap (cl.model_name[1]);

	// done with modellist, request first of static signon messages
	if (!cls.demoplayback) {
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message,
						 va (prespawn_name, cl.servercount,
							 cl_world.scene->worldmodel->brush.checksum2));
	}
}

static void
Sound_NextDownload (void)
{
	char	   *s;
	int			i;

	if (cls.downloadnumber == 0) {
		Sys_Printf ("Checking sounds...\n");
		cl.viewstate.time = realtime;
		cl.viewstate.realtime = realtime;
		cl_realtime = realtime;
		cl_frametime = host_frametime;
		CL_UpdateScreen (&cl.viewstate);
		cls.downloadnumber = 1;
	}

	cls.downloadtype = dl_sound;
	for (; cl.sound_name[cls.downloadnumber][0];
		 cls.downloadnumber++) {
		s = cl.sound_name[cls.downloadnumber];
		if (!CL_CheckOrDownloadFile (va ("sound/%s", s)))
			return;						// started a download
	}

	for (i = 1; i < cl.numsounds; i++) {
		if (!cl.sound_name[i][0])
			break;
		cl.sound_precache[i] = S_PrecacheSound (cl.sound_name[i]);
	}

	// done with sounds, request models now
	cl_world.models.size = 0;
	DARRAY_APPEND (&cl_world.models, 0);// ind 0 is null model
	cl_playerindex = -1;
	cl_flagindex = -1;
	cl_h_playerindex = -1;
	cl_gib1index = cl_gib2index = cl_gib3index = -1;
	if (!cls.demoplayback) {
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message,
						 va (modellist_name, cl.servercount, 0));
	}
}

static void
CL_RequestNextDownload (void)
{
	switch (cls.downloadtype) {
		case dl_single:
			break;
		case dl_skin:
			Skin_NextDownload ();
			break;
		case dl_model:
			Model_NextDownload ();
			break;
		case dl_sound:
			Sound_NextDownload ();
			break;
		case dl_none:
		default:
			Sys_MaskPrintf (SYS_dev, "Unknown download type.\n");
	}
}

void
CL_FinishDownload (void)
{
	Qclose (cls.download);
	VID_SetCaption (va ("Connecting to %s", cls.servername->str));

	// rename the temp file to it's final name
	if (strcmp (cls.downloadtempname->str, cls.downloadname->str)) {
		dstring_t  *oldn = dstring_new ();
		dstring_t  *newn = dstring_new ();

		if (strncmp (cls.downloadtempname->str, "skins/", 6)) {
			dsprintf (oldn, "%s/%s", qfs_gamedir->dir.def,
					  cls.downloadtempname->str);
			dsprintf (newn, "%s/%s", qfs_gamedir->dir.def,
					  cls.downloadname->str);
		} else {
			dsprintf (oldn, "%s/%s", qfs_gamedir->dir.skins,
					  cls.downloadtempname->str + 6);
			dsprintf (newn, "%s/%s", qfs_gamedir->dir.skins,
					  cls.downloadname->str + 6);
		}
		if (QFS_Rename (oldn->str, newn->str))
			Sys_Printf ("%cfailed to rename %s to %s, %s.\n", 3, oldn->str,
						newn->str, strerror (errno));
		dstring_delete (oldn);
		dstring_delete (newn);
	}

	cls.download = NULL;
	dstring_clearstr (cls.downloadname);
	dstring_clearstr (cls.downloadtempname);
	dstring_clearstr (cls.downloadurl);
	cls.downloadpercent = 0;

	// get another file if needed
	CL_RequestNextDownload ();
}

void
CL_FailDownload (void)
{
	if (cls.download) {
		Qclose (cls.download);
		cls.download = NULL;
	}
	dstring_clearstr (cls.downloadname);
	dstring_clearstr (cls.downloadtempname);
	dstring_clearstr (cls.downloadurl);
	CL_RequestNextDownload ();
}

static int
CL_OpenDownload (void)
{
	dstring_t  *name = dstring_newstr ();
	const char *fname, *path;

	if (strncmp (cls.downloadtempname->str, "skins/", 6) == 0) {
		path = qfs_gamedir->dir.skins;
		fname = cls.downloadtempname->str + 6;
	} else if (strncmp (cls.downloadtempname->str, "progs/", 6) == 0) {
		path = qfs_gamedir->dir.models;
		fname = cls.downloadtempname->str + 6;
	} else if (strncmp (cls.downloadtempname->str, "sound/", 6) == 0) {
		path = qfs_gamedir->dir.sound;
		fname = cls.downloadtempname->str + 6;
	} else if (strncmp (cls.downloadtempname->str, "maps/", 5) == 0) {
		path = qfs_gamedir->dir.maps;
		fname = cls.downloadtempname->str + 5;
	} else {
		path = qfs_gamedir->dir.def;
		fname = cls.downloadtempname->str;
	}
	dsprintf (name, "%s/%s", path, fname);

	cls.download = QFS_WOpen (name->str, 0);
	if (!cls.download) {
		dstring_clearstr (cls.downloadname);
		dstring_clearstr (cls.downloadurl);
		Sys_Printf ("%cFailed to open %s\n", 3, name->str);
		CL_RequestNextDownload ();
		return 0;
	}
	dstring_delete (name);
	return 1;
}

/*
	CL_ParseDownload

	A download message has been received from the server
*/
static void
CL_ParseDownload (void)
{
	int         size, percent;

	// read the data
	size = (short) MSG_ReadShort (net_message);
	percent = MSG_ReadByte (net_message);

	if (cls.demoplayback) {
		if (size > 0)
			net_message->readcount += size;
		dstring_clearstr (cls.downloadname);
		dstring_clearstr (cls.downloadtempname);
		dstring_clearstr (cls.downloadurl);
		return;							// not in demo playback
	}

	if (size == DL_NOFILE) {
		Sys_Printf ("File not found.\n");
		if (cls.download) {
			Sys_Printf ("cls.download shouldn't have been set\n");
			Qclose (cls.download);
			cls.download = NULL;
		}
		dstring_clearstr (cls.downloadname);
		dstring_clearstr (cls.downloadtempname);
		dstring_clearstr (cls.downloadurl);
		CL_RequestNextDownload ();
		return;
	}

	if (size == DL_RENAME) {
		const char *newname = MSG_ReadString (net_message);

		if (strncmp (newname, cls.downloadname->str,
					 strlen (cls.downloadname->str))
			|| strstr (newname + strlen (cls.downloadname->str), "/")) {
			Sys_Printf
				("%cWARNING: server tried to give a strange new name: %s\n", 3,
				 newname);
			CL_RequestNextDownload ();
			return;
		}
		if (cls.download) {
			Qclose (cls.download);
			unlink (cls.downloadname->str);
		}
		dstring_copystr (cls.downloadname, newname);
		Sys_Printf ("%cdownloading to %s\n", 3, cls.downloadname->str);
		return;
	}
	if (size == DL_HTTP) {
#ifdef HAVE_LIBCURL
		const char *url = MSG_ReadString (net_message);
		const char *newname = MSG_ReadString (net_message);

		if (*newname) {
			if (strncmp (newname, cls.downloadname->str,
						 strlen (cls.downloadname->str))
				|| strstr (newname + strlen (cls.downloadname->str), "/")) {
				Sys_Printf
					("%cWARNING: server tried to give a strange new name: %s\n",
					 3, newname);
				CL_RequestNextDownload ();
				return;
			}
			if (cls.download) {
				Qclose (cls.download);
				unlink (cls.downloadname->str);
			}
			dstring_copystr (cls.downloadname, newname);
		}
		dstring_copystr (cls.downloadurl, url);
		Sys_Printf ("%cdownloading %s to %s\n", 3, cls.downloadurl->str,
					cls.downloadname->str);
		CL_HTTP_StartDownload ();
#else
		MSG_ReadString (net_message);
		MSG_ReadString (net_message);
		Sys_Printf ("server sent http redirect but we don't know how to handle"
					"it :(\n");
#endif
		CL_OpenDownload ();
		return;
	}
	// open the file if not opened yet
	if (!cls.download) {
		if (!CL_OpenDownload ()) {
			return;
			net_message->readcount += size;
		}
	}

	Qwrite (cls.download, net_message->message->data + net_message->readcount,
			size);
	net_message->readcount += size;

	if (percent != 100) {
		// request next block
		if (percent != cls.downloadpercent)
			VID_SetCaption (va ("Downloading %s %d%%",
								cls.downloadname->str, percent));
		cls.downloadpercent = percent;

		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, "nextdl");
	} else {
		CL_FinishDownload ();
	}
}

static byte *upload_data;
static int	upload_pos, upload_size;

void
CL_NextUpload (void)
{
	byte		buffer[1024];
	int			percent, size, r;

	if (!upload_data)
		return;

	r = upload_size - upload_pos;
	if (r > 768)
		r = 768;
	memcpy (buffer, upload_data + upload_pos, r);
	MSG_WriteByte (&cls.netchan.message, clc_upload);
	MSG_WriteShort (&cls.netchan.message, r);

	upload_pos += r;
	size = upload_size;
	if (!size)
		size = 1;
	percent = upload_pos * 100 / size;
	MSG_WriteByte (&cls.netchan.message, percent);
	SZ_Write (&cls.netchan.message, buffer, r);

	Sys_MaskPrintf (SYS_dev, "UPLOAD: %6d: %d written\n", upload_pos - r, r);

	if (upload_pos != upload_size)
		return;

	Sys_Printf ("Upload completed\n");

	free (upload_data);
	upload_data = 0;
	upload_pos = upload_size = 0;
}

void
CL_StartUpload (byte * data, int size)
{
	if (cls.state < ca_onserver)
		return;							// must be connected

	// override
	if (upload_data)
		free (upload_data);

	Sys_MaskPrintf (SYS_dev, "Upload starting of %d...\n", size);

	upload_data = malloc (size);
	memcpy (upload_data, data, size);
	upload_size = size;
	upload_pos = 0;

	CL_NextUpload ();
}

bool
CL_IsUploading (void)
{
	if (upload_data)
		return true;
	return false;
}

void
CL_StopUpload (void)
{
	if (upload_data)
		free (upload_data);
	upload_data = NULL;
}

// SERVER CONNECTING MESSAGES =================================================

// LordHavoc: BIG BUG-FIX!  Clear baselines each time it connects...
static void
CL_ClearBaselines (void)
{
	int			i;

	i = qw_entstates.num_entities;
	memset (qw_entstates.baseline, 0, i * sizeof (entity_state_t));
	for (i = 0; i < MAX_EDICTS; i++) {
		qw_entstates.baseline[i].colormod = 255;
		qw_entstates.baseline[i].alpha = 255;
		qw_entstates.baseline[i].scale = 16;
		qw_entstates.baseline[i].glow_size = 0;
		qw_entstates.baseline[i].glow_color = 254;
	}
}

static void
CL_ParseServerData (void)
{
	const char *str;
	int			protover;
	//FIXME bool		cflag = false;

	Sys_MaskPrintf (SYS_dev, "Serverdata packet received.\n");

	// wipe the client_state_t struct
	CL_ClearState ();

	// parse protocol version number
	// allow 2.2 and 2.29 demos to play
	protover = MSG_ReadLong (net_message);
	if (protover != PROTOCOL_VERSION
		&& !(cls.demoplayback && (protover <= 26 || protover >= 28)))
		Host_Error ("Server returned version %i, not %i\nYou probably "
					"need to upgrade.\nCheck http://www.quakeworld.net/",
					protover, PROTOCOL_VERSION);

	cl.servercount = MSG_ReadLong (net_message);

	// game directory
	str = MSG_ReadString (net_message);

	if (!strequal (qfs_gamedir->gamedir, str)) {
		// save current config
		Host_WriteConfiguration ();
		//FIXME cflag = true;
		QFS_Gamedir (str);
	}

	if (cls.demoplayback2) {
		realtime = cls.basetime = MSG_ReadFloat (net_message);
		cl.playernum = 31;
		cl.spectator = true;
	} else {
		// parse player slot, high bit means spectator
		cl.playernum = MSG_ReadByte (net_message);
		if (cl.playernum & 128) {
			cl.spectator = true;
			cl.playernum &= ~128;
		}
	}

	cl.viewentity = cl.playernum + 1;
	cl.viewstate.bob_enabled = !cl.spectator;
	Sbar_SetPlayerNum (cl.playernum, cl.spectator);

	// get the full level name
	str = MSG_ReadString (net_message);
	strncpy (cl.levelname, str, sizeof (cl.levelname) - 1);
	Sbar_SetLevelName (cl.levelname, cls.servername->str);
	Sbar_SetGameType (1);

	// get the movevars
	movevars.gravity = MSG_ReadFloat (net_message);
	CL_ParticlesGravity (movevars.gravity);// Gravity for renderer effects
	movevars.stopspeed = MSG_ReadFloat (net_message);
	movevars.maxspeed = MSG_ReadFloat (net_message);
	movevars.spectatormaxspeed = MSG_ReadFloat (net_message);
	movevars.accelerate = MSG_ReadFloat (net_message);
	movevars.airaccelerate = MSG_ReadFloat (net_message);
	movevars.wateraccelerate = MSG_ReadFloat (net_message);
	movevars.friction = MSG_ReadFloat (net_message);
	movevars.waterfriction = MSG_ReadFloat (net_message);
	movevars.entgravity = MSG_ReadFloat (net_message);

	// separate the printfs so the server message can have a color
	Sys_Printf ("%c\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36"
				"\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n",
				3);
	Sys_Printf ("%c%s\n", 2, str);

	// ask for the sound list next
	memset (cl.sound_name, 0, sizeof (cl.sound_name));
	if (!cls.demoplayback) {
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message,
						 va (soundlist_name, cl.servercount, 0));
	}

	// now waiting for downloads, etc
	CL_SetState (ca_onserver);

	CL_ClearBaselines ();
}

static void
CL_ParseSoundlist (void)
{
	const char *str;
	int			n;

	// precache sounds
//	memset (cl.sound_precache, 0, sizeof (cl.sound_precache));

	cl.numsounds = MSG_ReadByte (net_message);

	for (;;) {
		str = MSG_ReadString (net_message);
		if (!str[0])
			break;
		cl.numsounds++;
		if (cl.numsounds >= MAX_SOUNDS)
			Host_Error ("Server sent too many sound_precache");
		strcpy (cl.sound_name[cl.numsounds], str);
	}
	cl.numsounds++;

	n = MSG_ReadByte (net_message);

	if (n && !cls.demoplayback) {
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message,
						 va (soundlist_name, cl.servercount, n));
		return;
	}

	cls.downloadnumber = 0;
	cls.downloadtype = dl_sound;
	Sound_NextDownload ();
}

static void
CL_ParseModellist (void)
{
	int			n;
	const char *str;

	// precache models and note certain default indexes
	cl.nummodels = MSG_ReadByte (net_message);

	for (;;) {
		str = MSG_ReadString (net_message);
		if (!str[0])
			break;
		cl.nummodels++;
		if (cl.nummodels >= MAX_MODELS)
			Host_Error ("Server sent too many model_precache");
		strcpy (cl.model_name[cl.nummodels], str);

		if (!strcmp (cl.model_name[cl.nummodels], "progs/player.mdl"))
			cl_playerindex = cl.nummodels;
		else if (!strcmp (cl.model_name[cl.nummodels], "progs/flag.mdl"))
			cl_flagindex = cl.nummodels;
		// for deadbodyfilter & gib filter
		else if (!strcmp (cl.model_name[cl.nummodels], "progs/h_player.mdl"))
			cl_h_playerindex = cl.nummodels;
		else if (!strcmp (cl.model_name[cl.nummodels], "progs/gib1.mdl"))
			cl_gib1index = cl.nummodels;
		else if (!strcmp (cl.model_name[cl.nummodels], "progs/gib2.mdl"))
			cl_gib2index = cl.nummodels;
		else if (!strcmp (cl.model_name[cl.nummodels], "progs/gib3.mdl"))
			cl_gib3index = cl.nummodels;
	}
	cl.nummodels++;

	n = MSG_ReadByte (net_message);

	if (n) {
		if (!cls.demoplayback) {
			MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
			MSG_WriteString (&cls.netchan.message,
							 va (modellist_name, cl.servercount, n));
		}
		return;
	}

	cls.downloadnumber = 0;
	cls.downloadtype = dl_model;
	Model_NextDownload ();
}

static void
CL_ParseStaticSound (void)
{
	int			sound_num;
	float       vol, atten;
	vec4f_t     org = { 0, 0, 0, 1 };

	MSG_ReadCoordV (net_message, (vec_t*)&org);//FIXME
	sound_num = MSG_ReadByte (net_message);
	vol = MSG_ReadByte (net_message) / 255.0;
	atten = MSG_ReadByte (net_message) / 64.0;

	S_StaticSound (cl.sound_precache[sound_num], org, vol, atten);
}

// ACTION MESSAGES ============================================================

static void
CL_ParseStartSoundPacket (void)
{
	float		attenuation;
	int			bits, channel, ent, sound_num, volume;

	bits = MSG_ReadShort (net_message);

	if (bits & SND_VOLUME)
		volume = MSG_ReadByte (net_message);
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;

	if (bits & SND_ATTENUATION)
		attenuation = MSG_ReadByte (net_message) / 64.0;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;

	sound_num = MSG_ReadByte (net_message);

	vec4f_t     pos = { 0, 0, 0, 1 };
	MSG_ReadCoordV (net_message, (vec_t*)&pos);//FIXME

	ent = (bits >> 3) & 1023;
	channel = bits & 7;

	if (ent > MAX_EDICTS)
		Host_Error ("CL_ParseStartSoundPacket: ent = %i", ent);

	S_StartSound (ent, channel, cl.sound_precache[sound_num], pos,
				  volume / 255.0, attenuation);
}

/*
	CL_ParseClientdata

	Server information pertaining to only this client, sent every frame
*/
void
CL_ParseClientdata (void)
{
	float		latency;
	cl_frame_t *frame;
	int			i;

	// calculate simulated time of message
	oldparsecountmod = parsecountmod;

	i = cls.netchan.incoming_acknowledged;

	cl.parsecount = i;
	i &= UPDATE_MASK;
	parsecountmod = i;
	frame = &cl.frames[i];
	if (cls.demoplayback2)
		frame->senttime = realtime - host_frametime;	//realtime;
	parsecounttime = cl.frames[i].senttime;

	frame->receivedtime = realtime;

	// calculate latency
	latency = frame->receivedtime - frame->senttime;

	if (!(latency < 0 || latency > 1.0)) {
		// drift the average latency towards the observed latency
		if (latency < cls.latency)
			cls.latency = latency;
		else
			cls.latency += 0.001;		// drift up, so correction is needed
	}
}

static void
CL_ProcessUserInfo (int slot, player_info_t *player)
{
	while (!(player->name = Info_Key (player->userinfo, "name"))) {
		const char *name = "";
		if (player->userid) {
			name = va ("user-%i [exploit]", player->userid);
		}
		Info_SetValueForKey (player->userinfo, "name", name, 1);
	}
	const char *tc = Info_ValueForKey (player->userinfo, "topcolor");
	const char *bc = Info_ValueForKey (player->userinfo, "bottomcolor");
	player->topcolor = tc ? atoi (tc) : TOP_COLOR;
	player->bottomcolor = bc ? atoi (bc) : BOTTOM_COLOR;

	while (!(player->team = Info_Key (player->userinfo, "team"))) {
		Info_SetValueForKey (player->userinfo, "team", "", 1);
	}
	while (!(player->skinname = Info_Key (player->userinfo, "skin"))) {
		Info_SetValueForKey (player->userinfo, "skin", "", 1);
	}
	while (!(player->chat = Info_Key (player->userinfo, "chat"))) {
		Info_SetValueForKey (player->userinfo, "chat", "0", 1);
	}

	const char *spec = Info_ValueForKey (player->userinfo, "*spectator");
	player->spectator = spec && *spec;

	player->skin = mod_funcs->skin_set (player->skinname->value);

	Sbar_UpdateInfo (slot);
}

static void
CL_UpdateUserinfo (void)
{
	int				slot, uid;
	const char	   *info;
	player_info_t  *player;

	slot = MSG_ReadByte (net_message);
	if (slot >= MAX_CLIENTS)
		Host_Error
			("CL_ParseServerMessage: svc_updateuserinfo > MAX_SCOREBOARD");

	player = &cl.players[slot];
	uid = MSG_ReadLong (net_message);
	info = MSG_ReadString (net_message);
	if (*info) {
		// a totally empty userinfo string should not be possible
		player->userid = uid;
		if (player->userinfo)
			Info_Destroy (player->userinfo);
		player->userinfo = Info_ParseString (info, MAX_INFO_STRING, 0);
		CL_ProcessUserInfo (slot, player);
		CL_Chat_Check_Name (Info_ValueForKey (player->userinfo, "name"), slot);
	} else {
		// the server dropped the client
		CL_Chat_User_Disconnected (player->userid);
		if (player->userinfo)
			Info_Destroy (player->userinfo);
		memset (player, 0, sizeof (*player));
	}
}

static void
CL_SetInfo (void)
{
	char			key[MAX_MSGLEN], value[MAX_MSGLEN];
	int				flags, slot;
	player_info_t  *player;

	slot = MSG_ReadByte (net_message);
	if (slot >= MAX_CLIENTS)
		Host_Error ("CL_ParseServerMessage: svc_setinfo > MAX_SCOREBOARD");

	player = &cl.players[slot];

	strncpy (key, MSG_ReadString (net_message), sizeof (key) - 1);
	key[sizeof (key) - 1] = 0;
	strncpy (value, MSG_ReadString (net_message), sizeof (value) - 1);
	key[sizeof (value) - 1] = 0;

	if (!player->userinfo)
		player->userinfo = Info_ParseString ("", MAX_INFO_STRING, 0);

	flags = !strequal (key, "name");
	flags |= strequal (key, "team") << 1;
	Info_SetValueForKey (player->userinfo, key, value, flags);

	CL_ProcessUserInfo (slot, player);

	Sys_MaskPrintf (SYS_dev, "SETINFO %s: %s=%s\n", player->name->value, key,
					value);
}

static void
CL_ServerInfo (void)
{
	char		key[MAX_MSGLEN], value[MAX_MSGLEN];

	strncpy (key, MSG_ReadString (net_message), sizeof (key) - 1);
	key[sizeof (key) - 1] = 0;
	strncpy (value, MSG_ReadString (net_message), sizeof (value) - 1);
	key[sizeof (value) - 1] = 0;

	Sys_MaskPrintf (SYS_dev, "SERVERINFO: %s=%s\n", key, value);

	Info_SetValueForKey (cl.serverinfo, key, value, 0);
	if (strequal (key, "chase")) {
		cl.viewstate.chase = atoi (value);
	} else if (strequal (key, "cshifts")) {
		cl.sv_cshifts = atoi (value);
	} else if (strequal (key, "no_pogo_stick")) {
		Cvar_Set ("no_pogo_stick", value);
		cl.no_pogo_stick = no_pogo_stick;
	} else if (strequal (key, "teamplay")) {
		cl.teamplay = atoi (value);
		Sbar_SetTeamplay (cl.teamplay);
	} else if (strequal (key, "watervis")) {
		cl.viewstate.watervis = atoi (value);
	} else if (strequal (key, "fpd")) {
		cl.fpd = atoi (value);
	} else if (strequal (key, "fbskins")) {
		cl.fbskins = atoi (value);
//	} else if (strequal (key, "*z_ext") {
//		cl.z_ext = atoi (value);
//	} else if (strequal (key, "pm_bunnyspeedcap") {
//		cl.bunnyspeedcap = atof (value);
//	} else if (strequal (key, "pm_slidefix") {
//		movevars.slidefix = atoi (value);
//	} else if (strequal (key, "pm_ktjump") {
//		movevars.ktjump = atof (value);
//		FIXME: need to set to 0.5 otherwise, outside of else structure
	}
}

static void
CL_SetStat (int stat, int value)
{
	if (stat < 0 || stat >= MAX_CL_STATS)
		Host_Error ("CL_SetStat: %i is invalid", stat);

	if (cls.demoplayback2) {
		cl.players[cls.lastto].stats[stat] = value;
		if (Cam_TrackNum () != cls.lastto)
			return;
	}

	switch (stat) {
		case STAT_ITEMS:
#define IT_POWER (IT_QUAD | IT_SUIT | IT_INVULNERABILITY | IT_INVISIBILITY)
			cl.viewstate.powerup_index = (cl.stats[STAT_ITEMS]&IT_POWER) >> 19;
			break;
		case STAT_HEALTH:
			if (cl_player_health_e->func)
				GIB_Event_Callback (cl_player_health_e, 1,
									va ("%i", value));
			if (value <= 0)
				Team_Dead ();
			break;
	}
	cl.stats[stat] = value;
	cl.viewstate.weapon_model = cl_world.models.a[cl.stats[STAT_WEAPON]];
	if (cl.stdver) {
		cl.viewstate.height = cl.stats[STAT_VIEWHEIGHT];
	} else {
		cl.viewstate.height = DEFAULT_VIEWHEIGHT;	// view height
	}
	Sbar_UpdateStats (stat);
}

static void
CL_ParseMuzzleFlash (void)
{
	int			i;
	player_state_t *pl;

	i = MSG_ReadShort (net_message);

	if ((unsigned int) (i - 1) >= MAX_CLIENTS)
		return;

	pl = &cl.frames[parsecountmod].playerstate[i - 1];
	pl->muzzle_flash = true;
}

#define SHOWNET(x) \
	if (cl_shownet == 2) \
		Sys_Printf ("%3i:%s\n", net_message->readcount - 1, x);

int			received_framecount;

void
CL_ParseServerMessage (void)
{
	int         cmd = 0, i, j;
	int         update_pings = 0;
	const char *str;
	static dstring_t *stuffbuf;
	TEntContext_t tentCtx = {
		cl.viewstate.player_origin, cl.viewentity
	};

	received_framecount = host_framecount;
	cl.viewstate.last_servermessage = realtime;
	CL_ClearProjectiles ();

	// if recording demos, copy the message out
	if (cl_shownet == 1)
		Sys_Printf ("%i ", net_message->message->cursize);
	else if (cl_shownet == 2)
		Sys_Printf ("------------------ %d\n",
					cls.netchan.incoming_acknowledged);

	CL_ParseClientdata ();

	// parse the message
	while (1) {
		if (net_message->badread)
			Host_Error ("CL_ParseServerMessage: Bad server message: %s\n",
						svc_strings[cmd]);

		cmd = MSG_ReadByte (net_message);

		if (cmd == -1) {
			net_message->readcount++;	// so the EOM SHOWNET has the right
										// value
			SHOWNET ("END OF MESSAGE");
			break;						// end of message
		}

		SHOWNET (va ("%s(%d)", svc_strings[cmd], cmd));

		// other commands
		switch (cmd) {
			default:
				Host_Error ("CL_ParseServerMessage: Illegible server "
							"message: %d\n", cmd);
				break;

			case svc_nop:
				break;

			case svc_disconnect:
				if (cls.state == ca_connected)
					Host_EndGame ("Server disconnected\n"
								  "Server version may not be compatible");
				else
					Host_EndGame ("Server disconnected\n");

			case svc_updatestat:
				i = MSG_ReadByte (net_message);
				j = MSG_ReadByte (net_message);
				CL_SetStat (i, j);
				break;

			//   svc_version
			//   svc_setview

			case svc_sound:
				CL_ParseStartSoundPacket ();
				break;

			//   svc_time

			case svc_print:
			{
				dstring_t  *p = 0;

				i = MSG_ReadByte (net_message);
				str = MSG_ReadString (net_message);
				if (i == PRINT_CHAT) {
					if (!CL_Chat_Allow_Message (str))
						break;
					// TODO: cl_nofake 2 -- accept fake messages from teammates

					if (cl_nofake) {
						char	*c;

						p = dstring_strdup (str);
						for (c = p->str; *c; c++) {
							if (*c == '\r')
								*c = '#';
						}
						str = p->str;
					}
					Con_SetOrMask (128);
					S_LocalSound ("misc/talk.wav");
					if (cl_chat_e->func)
						GIB_Event_Callback (cl_chat_e, 1, str);
					Team_ParseChat (str);
				}
				if (str[0]) {
					if (str[0] <= 3) {
						Sys_Printf ("%s", str);
					} else {
						Sys_Printf ("%c%s", 3, str);
					}
				}
				if (p)
					dstring_delete (p);
				Con_SetOrMask (0);
				break;
			}
			case svc_stufftext:
				str = MSG_ReadString (net_message);
				if (str[strlen (str) - 1] == '\n') {
					if (stuffbuf && stuffbuf->str[0]) {
						Sys_MaskPrintf (SYS_dev, "stufftext: %s%s\n",
										stuffbuf->str, str);
						Cbuf_AddText (cl_stbuf, stuffbuf->str);
						dstring_clearstr (stuffbuf);
					} else {
						Sys_MaskPrintf (SYS_dev, "stufftext: %s\n", str);
					}
					Cbuf_AddText (cl_stbuf, str);
				} else {
					Sys_MaskPrintf (SYS_dev, "partial stufftext: %s\n", str);
					if (!stuffbuf)
						stuffbuf = dstring_newstr ();
					dstring_appendstr (stuffbuf, str);
				}
				break;

			case svc_setangle:
			{
				vec_t      *dest = cl.viewstate.player_angles;
				vec3_t      dummy;

				if (cls.demoplayback2) {
					j = MSG_ReadByte (net_message);
//					fixangle |= 1 << j;
					if (j != Cam_TrackNum ())
						dest = dummy;
				}
				MSG_ReadAngleV (net_message, dest);
				break;
			}
			case svc_serverdata:
				// make sure any stuffed commands are done
				if (stuffbuf && stuffbuf->str[0]) {
					Cbuf_AddText (cl_stbuf, stuffbuf->str);
					dstring_clearstr (stuffbuf);
				}
				Cbuf_Execute_Stack (cl_stbuf);
				CL_ParseServerData ();
				break;

			case svc_lightstyle:
				i = MSG_ReadByte (net_message);
				if (i >= MAX_LIGHTSTYLES)
					Host_Error ("svc_lightstyle > MAX_LIGHTSTYLES");
				strcpy (cl.lightstyle[i].map, MSG_ReadString (net_message));
				cl.lightstyle[i].length = strlen (cl.lightstyle[i].map);
				if (cl.lightstyle[i].length) {
					int         total = 0;

					cl.lightstyle[i].peak = 'a';
					for (j = 0; j < cl.lightstyle[i].length; j++) {
						total += cl.lightstyle[i].map[j] - 'a';
						cl.lightstyle[i].peak = max (cl.lightstyle[i].peak,
													 cl.lightstyle[i].map[j]);
					}
					total /= cl.lightstyle[i].length;
					cl.lightstyle[i].average = total + 'a';
				} else {
					cl.lightstyle[i].average = 'm';
					cl.lightstyle[i].peak = 'm';
				}
				break;

			//   svc_updatename

			case svc_updatefrags:
				i = MSG_ReadByte (net_message);
				if (i >= MAX_CLIENTS)
					Host_Error ("CL_ParseServerMessage: svc_updatefrags > "
								"MAX_SCOREBOARD");
				cl.players[i].frags = (short) MSG_ReadShort (net_message);
				Sbar_UpdateFrags (i);
				break;

			//   svc_clientdata

			case svc_stopsound:
				i = MSG_ReadShort (net_message);
				S_StopSound (i >> 3, i & 7);
				break;

			//   svc_updatecolors
			//   svc_particle

			case svc_damage:
				V_ParseDamage (net_message, &cl.viewstate);
				// put sbar face into pain frame
				Sbar_Damage (cl.time);
				break;

			case svc_spawnstatic:
				CL_ParseStatic (net_message, 1);
				break;

			//   svc_spawnbinary

			case svc_spawnbaseline:
				i = MSG_ReadShort (net_message);
				CL_ParseBaseline (net_message, &qw_entstates.baseline[i], 1);
				break;

			case svc_temp_entity:
				CL_ParseTEnt_qw (net_message, cl.time, &tentCtx);
				break;

			case svc_setpause:
				r_data->paused = cl.paused = MSG_ReadByte (net_message);
				if (cl.paused)
					CDAudio_Pause ();
				else
					CDAudio_Resume ();
				break;

			//   svc_signonnum

			case svc_centerprint:
				str = MSG_ReadString (net_message);
				Sbar_CenterPrint (str);
				break;

			case svc_killedmonster:
				cl.stats[STAT_MONSTERS]++;
				Sbar_UpdateStats (STAT_MONSTERS);
				break;

			case svc_foundsecret:
				cl.stats[STAT_SECRETS]++;
				Sbar_UpdateStats (STAT_SECRETS);
				break;

			case svc_spawnstaticsound:
				CL_ParseStaticSound ();
				break;

			case svc_intermission:
				Sys_MaskPrintf (SYS_dev, "svc_intermission\n");

				cl.intermission = 1;
				SCR_SetFullscreen (1);
				cl.completed_time = realtime;
				Sys_MaskPrintf (SYS_dev, "intermission simorg: ");
				MSG_ReadCoordV (net_message, (vec_t*)&cl.viewstate.player_origin);//FIXME
				cl.viewstate.player_origin[3] = 1;
				Sys_MaskPrintf (SYS_dev, VEC4F_FMT,
								VEC4_EXP (cl.viewstate.player_origin));
				Sys_MaskPrintf (SYS_dev, "\nintermission simangles: ");
				MSG_ReadAngleV (net_message, cl.viewstate.player_angles);
				cl.viewstate.player_angles[ROLL] = 0;			// FIXME @@@
				Sys_MaskPrintf (SYS_dev, "%f %f %f",
								VectorExpand (cl.viewstate.player_angles));
				Sys_MaskPrintf (SYS_dev, "\n");
				cl.viewstate.velocity = (vec4f_t) { };

				// automatic fraglogging (by elmex)
				// XXX: Should this _really_ called here?
				if (!cls.demoplayback)
					Sbar_LogFrags (cl.time);
				break;

			case svc_finale:
				cl.intermission = 2;
				SCR_SetFullscreen (1);
				cl.completed_time = realtime;
				str = MSG_ReadString (net_message);
				Sbar_CenterPrint (str);
				break;

			case svc_cdtrack:
				cl.cdtrack = MSG_ReadByte (net_message);
				CDAudio_Play ((byte) cl.cdtrack, true);
				break;

			case svc_sellscreen:
				Cmd_ExecuteString ("help", src_command);
				break;

			//   svc_cutscene (same value as svc_smallkick)

			case svc_smallkick:
				cl.viewstate.decay_punchangle = 1;
				cl.viewstate.punchangle = (vec4f_t) {
					// -2 degrees pitch
					0, -0.0174524064, 0, 0.999847695
				};
				break;

			case svc_bigkick:
				cl.viewstate.decay_punchangle = 1;
				cl.viewstate.punchangle = (vec4f_t) {
					// -4 degrees pitch
					0, -0.0348994967, 0, 0.999390827
				};
				break;

			case svc_updateping:
				i = MSG_ReadByte (net_message);
				if (i >= MAX_CLIENTS)
					Host_Error ("CL_ParseServerMessage: svc_updateping > "
								"MAX_SCOREBOARD");
				cl.players[i].ping = MSG_ReadShort (net_message);
				update_pings = 1;
				break;

			case svc_updateentertime:
				// time is sent over as seconds ago
				i = MSG_ReadByte (net_message);
				if (i >= MAX_CLIENTS)
					Host_Error ("CL_ParseServerMessage: svc_updateentertime "
								"> MAX_SCOREBOARD");
				cl.players[i].entertime = realtime - MSG_ReadFloat
					(net_message);
				break;

			case svc_updatestatlong:
				i = MSG_ReadByte (net_message);
				j = MSG_ReadLong (net_message);
				CL_SetStat (i, j);
				break;

			case svc_muzzleflash:
				CL_ParseMuzzleFlash ();
				break;

			case svc_updateuserinfo:
				CL_UpdateUserinfo ();
				break;

			case svc_download:
				CL_ParseDownload ();
				break;

			case svc_playerinfo:
				CL_ParsePlayerinfo ();
				break;

			case svc_nails:
				CL_ParseProjectiles (net_message, false, &tentCtx);
				break;

			case svc_chokecount:		// some preceding packets were choked
				i = MSG_ReadByte (net_message);
				for (j = 0; j < i; j++)
					cl.frames[(cls.netchan.incoming_acknowledged - 1 - j) &
							  UPDATE_MASK].receivedtime = -2;
				break;

			case svc_modellist:
				CL_ParseModellist ();
				break;

			case svc_soundlist:
				CL_ParseSoundlist ();
				break;

			case svc_packetentities:
				CL_ParsePacketEntities (false);
				break;

			case svc_deltapacketentities:
				CL_ParsePacketEntities (true);
				break;

			case svc_maxspeed:
				movevars.maxspeed = MSG_ReadFloat (net_message);
				break;

			case svc_entgravity:
				movevars.entgravity = MSG_ReadFloat (net_message);
				break;

			case svc_setinfo:
				CL_SetInfo ();
				break;

			case svc_serverinfo:
				CL_ServerInfo ();
				break;

			case svc_updatepl:
				i = MSG_ReadByte (net_message);
				if (i >= MAX_CLIENTS)
					Host_Error ("CL_ParseServerMessage: svc_updatepl > "
								"MAX_SCOREBOARD");
				cl.players[i].pl = MSG_ReadByte (net_message);
				update_pings = 1;
				break;

			case svc_nails2:			// FIXME from qwex
				CL_ParseProjectiles (net_message, true, &tentCtx);
				break;
		}
	}
	if (update_pings) {
		Sbar_UpdatePings ();
	}

	CL_SetSolidEntities ();
}
