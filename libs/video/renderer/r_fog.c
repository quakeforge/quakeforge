/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others

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
//r_fog.c -- global and volumetric fog

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cmd.h"
#include "QF/plist.h"
#include "QF/render.h"
#include "QF/sys.h"

#include "compat.h"
#include "r_internal.h"

//==============================================================================
//
//  GLOBAL FOG
//
//==============================================================================

static vec4f_t fog;
static vec4f_t old_fog;

static float fade_time; //duration of fade
static float fade_done; //time when fade will be done

static void
fog_update (vec4f_t newfog, float time)
{
	//save previous settings for fade
	if (time > 0) {
		//check for a fade in progress
		if (fade_done > r_data->realtime) {
			float       f;

			f = (fade_done - r_data->realtime) / fade_time;
			old_fog = f * old_fog + (1 - f) * fog;
		} else {
			old_fog = fog;
		}
	}

	fog = newfog;
	fade_time = time;
	fade_done = r_data->realtime + time;
}
void
Fog_Update (float density, float red, float green, float blue, float time)
{
	fog_update ((vec4f_t) { red, green, blue, density }, time);
}

/*
	Fog_FogCommand_f

	handle the 'fog' console command
*/
static void
Fog_FogCommand_f (void)
{
	vec4f_t newfog = fog;
	float   time = 0;

	switch (Cmd_Argc ()) {
		default:
		case 1:
			Sys_Printf ("usage:\n");
			Sys_Printf ("   fog <density>\n");
			Sys_Printf ("   fog <red> <green> <blue>\n");
			Sys_Printf ("   fog <density> <red> <green> <blue>\n");
			Sys_Printf ("current values:\n");
			Sys_Printf ("   \"density\" is \"%f\"\n", fog[3]);
			Sys_Printf ("   \"red\" is \"%f\"\n", fog[0]);
			Sys_Printf ("   \"green\" is \"%f\"\n", fog[1]);
			Sys_Printf ("   \"blue\" is \"%f\"\n", fog[2]);
			return;
		case 2:
			newfog[3] = atof (Cmd_Argv(1));
			break;
		case 3: //TEST
			newfog[3] = atof (Cmd_Argv(1));
			time = atof (Cmd_Argv(2));
			break;
		case 4:
			newfog[0] = atof (Cmd_Argv(1));
			newfog[1] = atof (Cmd_Argv(2));
			newfog[2] = atof (Cmd_Argv(3));
			break;
		case 5:
			newfog[3] = atof (Cmd_Argv(1));
			newfog[0] = atof (Cmd_Argv(2));
			newfog[1] = atof (Cmd_Argv(3));
			newfog[2] = atof (Cmd_Argv(4));
			break;
		case 6: //TEST
			newfog[3] = atof (Cmd_Argv(1));
			newfog[0] = atof (Cmd_Argv(2));
			newfog[1] = atof (Cmd_Argv(3));
			newfog[2] = atof (Cmd_Argv(4));
			time = atof (Cmd_Argv(5));
			break;
	}
	newfog[3] = max (0, newfog[3]);
	newfog[0] = bound (0, newfog[0], 1);
	newfog[1] = bound (0, newfog[1], 1);
	newfog[2] = bound (0, newfog[2], 1);
	fog_update (newfog, time);
}

/*
	Fog_ParseWorldspawn

	called at map load
*/
void
Fog_ParseWorldspawn (plitem_t *worldspawn)
{
	plitem_t   *fog_item;
	const char *value;

	//initially no fog
	fog[3] = 0;
	old_fog[3] = 0;

	if (!worldspawn)
		return; // error
	if ((fog_item = PL_ObjectForKey (worldspawn, "fog"))
		&& (value = PL_String (fog_item))) {
		sscanf (value, "%f %f %f %f", &fog[3], &fog[0], &fog[1], &fog[2]);
	}
}

vec4f_t
Fog_Get (void)
{
	vec4f_t fc = fog;

	if (fade_done > r_data->realtime) {
		float f = (fade_done - r_data->realtime) / fade_time;
		fc = f * old_fog + (1 - f) * fog;
	}

	//find closest 24-bit RGB value, so solid-colored sky can match the fog
	//perfectly
	vec4f_t t = vfloor4f (fc * 255 + 0.5) / 255;
	// magic scaling factor for fog density, I don't know why 64 (maybe half
	// of a 128 pixel block?)
	t[3] = fc[3] / 64;
	return t;
}

/*
	Fog_GetColor

	calculates fog color for this frame, taking into account fade times
*/
void
Fog_GetColor (quat_t fogcolor)
{
	float       f;
	int         i;

	if (fade_done > r_data->realtime) {
		f = (fade_done - r_data->realtime) / fade_time;
		vec4f_t fc = f * old_fog + (1 - f) * fog;
		VectorCopy (fc, fogcolor);
		fogcolor[3] = 1;
	} else {
		VectorCopy (fog, fogcolor);
		fogcolor[3] = 1;
	}

	//find closest 24-bit RGB value, so solid-colored sky can match the fog
	//perfectly
	for (i = 0; i < 3; i++)
		fogcolor[i] = (float) (rint (fogcolor[i] * 255)) / 255.0f;
}

/*
	Fog_GetDensity

	returns current density of fog
*/
float
Fog_GetDensity (void)
{
	float       f;

	if (fade_done > r_data->realtime) {
		f = (fade_done - r_data->realtime) / fade_time;
		return f * old_fog[3] + (1 - f) * fog[3];
	} else {
		return fog[3];
	}
}

/*
	Fog_Init

	called when quake initializes
*/
void
Fog_Init (void)
{
	Cmd_AddCommand ("fog", Fog_FogCommand_f, "");

	//Cvar_RegisterVariable (&r_vfog, NULL);

	//set up global fog
	fog = (vec4f_t) {0.3, 0.3, 0.3, 0.3};
}
