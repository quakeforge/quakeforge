/*
	joy_win.c

	Joystick device driver for Win32

	Copyright (C) 2000 Jeff Teunissen <deek@dusknet.dhs.org>
	Copyright (C) 2000 Jukka Sorjonen <jukka.sorjone@asikkala.fi>

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
// FIXME: THIS IS NOT FINISHED YET

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>

#include "winquake.h"

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/joystick.h"
#include "QF/keys.h"
#include "QF/qargs.h"
#include "QF/sys.h"

#include "compat.h"

// Joystick variables and structures
cvar_t     *joy_sensitivity;			// Joystick sensitivity

// joystick defines and variables
// where should defines be moved?

#define JOY_ABSOLUTE_AXIS	0x00000000	// control like a joystick
#define JOY_RELATIVE_AXIS	0x00000010	// control like a mouse, spinner,
										// trackball
#define JOY_AXIS_X			0
#define JOY_AXIS_Y			1
#define JOY_AXIS_Z			2
#define JOY_AXIS_R			3
#define JOY_AXIS_U			4
#define JOY_AXIS_V			5

enum _ControlList {
	AxisNada = 0, AxisForward, AxisLook, AxisSide, AxisTurn
};

DWORD dwAxisFlags[JOY_MAX_AXES] = {
	JOY_RETURNX, JOY_RETURNY, JOY_RETURNZ, JOY_RETURNR, JOY_RETURNU,
	JOY_RETURNV
};

DWORD dwAxisMap[JOY_MAX_AXES];
DWORD dwControlMap[JOY_MAX_AXES];
PDWORD pdwRawValue[JOY_MAX_AXES];

JOYINFOEX ji;

// none of these cvars are saved over a session
// this means that advanced controller configuration needs to be executed
// each time.  this avoids any problems with getting back to a default usage or
// when changing from one controller to another.  this way at least something
// works.

cvar_t *in_joystick;
cvar_t *joy_name;
cvar_t *joy_advanced;
cvar_t *joy_advaxisx;
cvar_t *joy_advaxisy;
cvar_t *joy_advaxisz;
cvar_t *joy_advaxisr;
cvar_t *joy_advaxisu;
cvar_t *joy_advaxisv;
cvar_t *joy_forwardthreshold;
cvar_t *joy_sidethreshold;
cvar_t *joy_pitchthreshold;
cvar_t *joy_yawthreshold;
cvar_t *joy_forwardsensitivity;
cvar_t *joy_sidesensitivity;
cvar_t *joy_pitchsensitivity;
cvar_t *joy_yawsensitivity;
cvar_t *joy_wwhack1;
cvar_t *joy_wwhack2;

cvar_t *joy_debug;

qboolean joy_advancedinit, joy_haspov;
DWORD joy_oldbuttonstate, joy_oldpovstate;
int  joy_id;
DWORD joy_flags;
DWORD joy_numbuttons;
#if 0
static PDWORD
RawValuePointer (int axis)
{
	switch (axis) {
		case JOY_AXIS_X:
			return &ji.dwXpos;
		case JOY_AXIS_Y:
			return &ji.dwYpos;
		case JOY_AXIS_Z:
			return &ji.dwZpos;
		case JOY_AXIS_R:
			return &ji.dwRpos;
		case JOY_AXIS_U:
			return &ji.dwUpos;
		case JOY_AXIS_V:
			return &ji.dwVpos;
	}
	return NULL;
}

static qboolean
_JOY_Read (void)
{
	memset (&ji, 0, sizeof (ji));
	ji.dwSize = sizeof (ji);
	ji.dwFlags = joy_flags;

	if (joyGetPosEx (joy_id, &ji) == JOYERR_NOERROR) {
		// HACK HACK HACK -- there's a bug in the Logitech Wingman Warrior's
		// DInput driver that causes it to make 32668 the center point
		// instead
		// of 32768
		if (joy_wwhack1->int_val) {
			ji.dwUpos += 100;
		}
		if (joy_debug->int_val) {
			if (ji.dwXpos) Sys_Printf("X: %ld\n",ji.dwXpos);
			if (ji.dwYpos) Sys_Printf("Y: %ld\n",ji.dwYpos);
			if (ji.dwZpos) Sys_Printf("Z: %ld\n",ji.dwZpos);
			if (ji.dwRpos) Sys_Printf("R: %ld\n",ji.dwRpos);
			if (ji.dwUpos) Sys_Printf("U: %ld\n",ji.dwUpos);
			if (ji.dwVpos) Sys_Printf("V: %ld\n",ji.dwVpos);
			if (ji.dwButtons) Sys_Printf("B: %ld\n",ji.dwButtons);
		}
		return true;
	} else {							// read error
		return false;
	}
}
#endif
void
JOY_Read (void)
{
	DWORD       i;
	DWORD       buttonstate, povstate;

	if (!joy_found) {
		return;
	}
	// loop through the joystick buttons
	// key a joystick event or auxillary event for higher number buttons for
	// each state change
	buttonstate = ji.dwButtons;
	for (i = 0; i < joy_numbuttons; i++) {
		if ((buttonstate & (1 << i)) && !(joy_oldbuttonstate & (1 << i))) {
			Key_Event (QFJ_BUTTON1 + i, 0, true);
		}

		if (!(buttonstate & (1 << i)) && (joy_oldbuttonstate & (1 << i))) {
			Key_Event (QFJ_BUTTON1 + i, 0, false);
		}
	}
	joy_oldbuttonstate = buttonstate;

	if (joy_haspov) {
		// convert POV information into 4 bits of state information
		// this avoids any potential problems related to moving from one
		// direction to another without going through the center position
		povstate = 0;
		if (ji.dwPOV != JOY_POVCENTERED) {
			if (ji.dwPOV == JOY_POVFORWARD)
				povstate |= 0x01;
			if (ji.dwPOV == JOY_POVRIGHT)
				povstate |= 0x02;
			if (ji.dwPOV == JOY_POVBACKWARD)
				povstate |= 0x04;
			if (ji.dwPOV == JOY_POVLEFT)
				povstate |= 0x08;
		}
		// determine which bits have changed and key an auxillary event for
		// each change
		for (i = 0; i < 4; i++) {
			if ((povstate & (1 << i)) && !(joy_oldpovstate & (1 << i))) {
				Key_Event (QFJ_BUTTON29 + i, -1, true);
			}

			if (!(povstate & (1 << i)) && (joy_oldpovstate & (1 << i))) {
				Key_Event (QFJ_BUTTON29 + i, -1, false);
			}
		}
		joy_oldpovstate = povstate;
	}
}

static int
JOY_StartupJoystick (void)
{
	int /* i, */ numdevs;
	JOYCAPS     jc;
	MMRESULT    mmr = !JOYERR_NOERROR;

	// assume no joystick
	joy_found = false;

	// abort startup if user requests no joystick
	if (COM_CheckParm ("-nojoy"))
		return -1;

	// verify joystick driver is present
	if ((numdevs = joyGetNumDevs ()) == 0) {
		Sys_Printf ("\njoystick not found -- driver not present\n\n");
		return -1;
	}
	// cycle through the joystick ids for the first valid one
	for (joy_id = 0; joy_id < numdevs; joy_id++) {
		memset (&ji, 0, sizeof (ji));
		ji.dwSize = sizeof (ji);
		ji.dwFlags = JOY_RETURNCENTERED;

		if ((mmr = joyGetPosEx (joy_id, &ji)) == JOYERR_NOERROR)
			break;
	}

	// abort startup if we didn't find a valid joystick
	if (mmr != JOYERR_NOERROR) {
		Sys_Printf ("\njoystick not found -- no valid joysticks (%x)\n\n",
					mmr);
		return -1;
	}
	// get the capabilities of the selected joystick
	// abort startup if command fails
	memset (&jc, 0, sizeof (jc));
	if ((mmr = joyGetDevCaps (joy_id, &jc, sizeof (jc))) != JOYERR_NOERROR) {
		Sys_Printf
			("\njoystick not found -- invalid joystick capabilities (%x)\n\n",
			 mmr);
		return -1;
	}
	// save the joystick's number of buttons and POV status
	joy_numbuttons = jc.wNumButtons;
	joy_haspov = jc.wCaps & JOYCAPS_HASPOV;

	// old button and POV states default to no buttons pressed
	joy_oldbuttonstate = joy_oldpovstate = 0;

	// mark the joystick as available and advanced initialization not
	// completed
	// this is needed as cvars are not available during initialization

	joy_advancedinit = false;
	joy_found = true;
	// FIXME: do this right
	joy_active = true;
	Sys_Printf ("\njoystick detected\n\n");
	return 0;
}

int
JOY_Open (void)
{
	return JOY_StartupJoystick();
//	Cmd_AddCommand ("joyadvancedupdate", JOY_AdvancedUpdate_f, "FIXME: This "
//					"appears to update the joystick poll? No Description");
}

void
JOY_Close (void)
{
}
#if 0
static void
JOY_AdvancedUpdate_f (void)
{
	// called once by JOY_ReadJoystick and by user whenever an update is
	// needed
	// cvars are now available
	int         i;
	DWORD       dwTemp;

	// initialize all the maps
	for (i = 0; i < JOY_MAX_AXES; i++) {
		dwAxisMap[i] = AxisNada;
		dwControlMap[i] = JOY_ABSOLUTE_AXIS;
		pdwRawValue[i] = RawValuePointer (i);
	}

	if (joy_advanced->int_val) {
		// default joystick initialization
		// only 2 axes with joystick control
		dwAxisMap[JOY_AXIS_X] = AxisTurn;
		// dwControlMap[JOY_AXIS_X] = JOY_ABSOLUTE_AXIS;
		dwAxisMap[JOY_AXIS_Y] = AxisForward;
		// dwControlMap[JOY_AXIS_Y] = JOY_ABSOLUTE_AXIS;
	} else {
		if (strcmp (joy_name->string, "joystick") != 0) {
			// notify user of advanced controller
			Sys_Printf ("\n%s configured\n\n", joy_name->string);
		}
		// advanced initialization here
		// data supplied by user via joy_axisn cvars
		dwTemp = joy_advaxisx->int_val;
		dwAxisMap[JOY_AXIS_X] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_X] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = joy_advaxisy->int_val;
		dwAxisMap[JOY_AXIS_Y] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_Y] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = joy_advaxisz->int_val;
		dwAxisMap[JOY_AXIS_Z] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_Z] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = joy_advaxisr->int_val;
		dwAxisMap[JOY_AXIS_R] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_R] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = joy_advaxisu->int_val;
		dwAxisMap[JOY_AXIS_U] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_U] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = joy_advaxisv->int_val;
		dwAxisMap[JOY_AXIS_V] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_V] = dwTemp & JOY_RELATIVE_AXIS;
	}

	// compute the axes to collect from DirectInput
	joy_flags = JOY_RETURNCENTERED | JOY_RETURNBUTTONS | JOY_RETURNPOV;
	for (i = 0; i < JOY_MAX_AXES; i++) {
		if (dwAxisMap[i] != AxisNada) {
			joy_flags |= dwAxisFlags[i];
		}
	}
}

void
JOY_Init_Cvars(void)
{
	// joystick variables
	joy_device = Cvar_Get ("joy_device", "none", CVAR_ROM, 0,
						   "Joystick device");
	joy_enable = Cvar_Get ("joy_enable", "1", CVAR_ARCHIVE, 0,
						   "Joystick enable flag");
	joy_sensitivity = Cvar_Get ("joy_sensitivity", "1", CVAR_ARCHIVE, 0, "Joystick sensitivity");
	in_joystick =  Cvar_Get ("joystick", "0", CVAR_ARCHIVE, 0, "FIXME: No "
							 "Description");
	joy_name = Cvar_Get ("joyname", "joystick", CVAR_NONE, 0, "FIXME: No "
						 "Description");
	joy_advanced = Cvar_Get ("joyadvanced", "0", CVAR_NONE, 0, "FIXME: No "
							 "Description");
	joy_advaxisx = Cvar_Get ("joyadvaxisx", "0", CVAR_NONE, 0, "FIXME: No "
							 "Description");
	joy_advaxisy = Cvar_Get ("joyadvaxisy", "0", CVAR_NONE, 0, "FIXME: No "
							 "Description");
	joy_advaxisz = Cvar_Get ("joyadvaxisz", "0", CVAR_NONE, 0, "FIXME: No "
							 "Description");
	joy_advaxisr = Cvar_Get ("joyadvaxisr", "0", CVAR_NONE, 0, "FIXME: No "
							 "Description");
	joy_advaxisu = Cvar_Get ("joyadvaxisu", "0", CVAR_NONE, 0, "FIXME: No "
							 "Description");
	joy_advaxisv = Cvar_Get ("joyadvaxisv", "0", CVAR_NONE, 0, "FIXME: No "
							 "Description");
	joy_forwardthreshold = Cvar_Get ("joyforwardthreshold", "0.15", CVAR_NONE,
									 0, "FIXME: No Description");
	joy_sidethreshold = Cvar_Get ("joysidethreshold", "0.15", CVAR_NONE, 0,
								  "FIXME: No Description");
	joy_pitchthreshold = Cvar_Get ("joypitchthreshold", "0.15", CVAR_NONE, 0,
								   "FIXME: No Description");
	joy_yawthreshold = Cvar_Get ("joyyawthreshold", "0.15", CVAR_NONE, 0,
								 "FIXME: No Description");
	joy_forwardsensitivity = Cvar_Get ("joyforwardsensitivity", "-1.0",
									   CVAR_NONE, 0, "FIXME: No Description");
	joy_sidesensitivity = Cvar_Get ("joysidesensitivity", "-1.0", CVAR_NONE,
									0, "FIXME: No Description");
	joy_pitchsensitivity = Cvar_Get ("joypitchsensitivity", "1.0", CVAR_NONE,
									 0, "FIXME: No Description");
	joy_yawsensitivity = Cvar_Get ("joyyawsensitivity", "-1.0", CVAR_NONE, 0,
								   "FIXME: No Description");
	joy_wwhack1 = Cvar_Get ("joywwhack1", "0.0", CVAR_NONE, 0, "FIXME: No "
							"Description");
	joy_wwhack2 = Cvar_Get ("joywwhack2", "0.0", CVAR_NONE, 0, "FIXME: No "
							"Description");

	joy_debug = Cvar_Get ("joy_debug", "0.0", CVAR_NONE, 0, "FIXME: No "
						  "Description");
	return;
}
#endif
