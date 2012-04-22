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
//gl_fog.c -- global and volumetric fog

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define NH_DEFINE
#include "namehack.h"

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cmd.h"
#include "QF/qfplist.h"
#include "QF/render.h"
#include "QF/sys.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"

#include "compat.h"
#include "r_internal.h"

//==============================================================================
//
//  GLOBAL FOG
//
//==============================================================================

static float fog_density;
static float fog_red;
static float fog_green;
static float fog_blue;

static float old_density;
static float old_red;
static float old_green;
static float old_blue;

static float fade_time; //duration of fade
static float fade_done; //time when fade will be done

/*
	Fog_Update

	update internal variables
*/
void
glsl_Fog_Update (float density, float red, float green, float blue, float time)
{
	//save previous settings for fade
	if (time > 0) {
		//check for a fade in progress
		if (fade_done > vr_data.realtime) {
			float       f;

			f = (fade_done - vr_data.realtime) / fade_time;
			old_density = f * old_density + (1.0 - f) * fog_density;
			old_red = f * old_red + (1.0 - f) * fog_red;
			old_green = f * old_green + (1.0 - f) * fog_green;
			old_blue = f * old_blue + (1.0 - f) * fog_blue;
		} else {
			old_density = fog_density;
			old_red = fog_red;
			old_green = fog_green;
			old_blue = fog_blue;
		}
	}

	fog_density = density;
	fog_red = red;
	fog_green = green;
	fog_blue = blue;
	fade_time = time;
	fade_done = vr_data.realtime + time;
}

/*
	Fog_FogCommand_f

	handle the 'fog' console command
*/
static void
Fog_FogCommand_f (void)
{
	switch (Cmd_Argc ()) {
		default:
		case 1:
			Sys_Printf ("usage:\n");
			Sys_Printf ("   fog <density>\n");
			Sys_Printf ("   fog <red> <green> <blue>\n");
			Sys_Printf ("   fog <density> <red> <green> <blue>\n");
			Sys_Printf ("current values:\n");
			Sys_Printf ("   \"density\" is \"%f\"\n", fog_density);
			Sys_Printf ("   \"red\" is \"%f\"\n", fog_red);
			Sys_Printf ("   \"green\" is \"%f\"\n", fog_green);
			Sys_Printf ("   \"blue\" is \"%f\"\n", fog_blue);
			break;
		case 2:
			glsl_Fog_Update (max (0.0, atof (Cmd_Argv (1))),
							 fog_red, fog_green, fog_blue, 0.0);
			break;
		case 3: //TEST
			glsl_Fog_Update (max (0.0, atof (Cmd_Argv (1))),
							 fog_red, fog_green, fog_blue,
							 atof (Cmd_Argv (2)));
			break;
		case 4:
			glsl_Fog_Update (fog_density,
							 bound (0.0, atof (Cmd_Argv (1)), 1.0),
							 bound (0.0, atof (Cmd_Argv (2)), 1.0),
							 bound (0.0, atof (Cmd_Argv (3)), 1.0),
							 0.0);
			break;
		case 5:
			glsl_Fog_Update (max (0.0, atof (Cmd_Argv (1))),
							 bound (0.0, atof (Cmd_Argv (2)), 1.0),
							 bound (0.0, atof (Cmd_Argv (3)), 1.0),
							 bound (0.0, atof (Cmd_Argv (4)), 1.0),
							 0.0);
			break;
		case 6: //TEST
			glsl_Fog_Update (max (0.0, atof (Cmd_Argv (1))),
							 bound (0.0, atof (Cmd_Argv (2)), 1.0),
							 bound (0.0, atof (Cmd_Argv (3)), 1.0),
							 bound (0.0, atof (Cmd_Argv (4)), 1.0),
							 atof (Cmd_Argv (5)));
			break;
	}
}

/*
	Fog_ParseWorldspawn

	called at map load
*/
void
glsl_Fog_ParseWorldspawn (plitem_t *worldspawn)
{
	plitem_t   *fog;
	const char *value;

	//initially no fog
	fog_density = 0.0;
	old_density = 0.0;
	fade_time = 0.0;
	fade_done = 0.0;

	if (!worldspawn)
		return; // error
	if ((fog = PL_ObjectForKey (worldspawn, "fog"))
		&& (value = PL_String (fog))) {
		sscanf (value, "%f %f %f %f", &fog_density,
				&fog_red, &fog_green, &fog_blue);
	}
}

/*
	Fog_GetColor

	calculates fog color for this frame, taking into account fade times
*/
float *
glsl_Fog_GetColor (void)
{
	static float c[4];
	float       f;
	int         i;

	if (fade_done > vr_data.realtime) {
		f = (fade_done - vr_data.realtime) / fade_time;
		c[0] = f * old_red + (1.0 - f) * fog_red;
		c[1] = f * old_green + (1.0 - f) * fog_green;
		c[2] = f * old_blue + (1.0 - f) * fog_blue;
		c[3] = 1.0;
	} else {
		c[0] = fog_red;
		c[1] = fog_green;
		c[2] = fog_blue;
		c[3] = 1.0;
	}

	//find closest 24-bit RGB value, so solid-colored sky can match the fog
	//perfectly
	for (i = 0; i < 3; i++)
		c[i] = (float) (rint (c[i] * 255)) / 255.0f;

	return c;
}

/*
	Fog_GetDensity

	returns current density of fog
*/
float
glsl_Fog_GetDensity (void)
{
	float       f;

	if (fade_done > vr_data.realtime) {
		f = (fade_done - vr_data.realtime) / fade_time;
		return f * old_density + (1.0 - f) * fog_density;
	} else {
		return fog_density;
	}
}

/*
	Fog_Init

	called when quake initializes
*/
void
glsl_Fog_Init (void)
{
	Cmd_AddCommand ("fog", Fog_FogCommand_f, "");

	//Cvar_RegisterVariable (&r_vfog, NULL);

	//set up global fog
	fog_density = 0.0;
	fog_red = 0.3;
	fog_green = 0.3;
	fog_blue = 0.3;
}
