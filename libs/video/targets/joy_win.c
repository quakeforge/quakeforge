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
float joy_sensitivity;
static cvar_t joy_sensitivity_cvar = {
	.name = "joy_sensitivity",
	.description =
		"Joystick sensitivity",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &joy_sensitivity },
};

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

char *in_joystick;
static cvar_t in_joystick_cvar = {
	.name = "joystick",
	.description =
		"FIXME: No Description",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = 0, .value = &in_joystick },
};
char *joy_name;
static cvar_t joy_name_cvar = {
	.name = "joyname",
	.description =
		"FIXME: No Description",
	.default_value = "joystick",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &joy_name },
};
int joy_advanced;
static cvar_t joy_advanced_cvar = {
	.name = "joyadvanced",
	.description =
		"FIXME: No Description",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &joy_advanced },
};
int joy_advaxisx;
static cvar_t joy_advaxisx_cvar = {
	.name = "joyadvaxisx",
	.description =
		"FIXME: No Description",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &joy_advaxisx },
};
int joy_advaxisy;
static cvar_t joy_advaxisy_cvar = {
	.name = "joyadvaxisy",
	.description =
		"FIXME: No Description",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &joy_advaxisy },
};
int joy_advaxisz;
static cvar_t joy_advaxisz_cvar = {
	.name = "joyadvaxisz",
	.description =
		"FIXME: No Description",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &joy_advaxisz },
};
int joy_advaxisr;
static cvar_t joy_advaxisr_cvar = {
	.name = "joyadvaxisr",
	.description =
		"FIXME: No Description",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &joy_advaxisr },
};
int joy_advaxisu;
static cvar_t joy_advaxisu_cvar = {
	.name = "joyadvaxisu",
	.description =
		"FIXME: No Description",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &joy_advaxisu },
};
int joy_advaxisv;
static cvar_t joy_advaxisv_cvar = {
	.name = "joyadvaxisv",
	.description =
		"FIXME: No Description",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &joy_advaxisv },
};
float joy_forwardthreshold;
static cvar_t joy_forwardthreshold_cvar = {
	.name = "joyforwardthreshold",
	.description =
		"FIXME: No Description",
	.default_value = "0.15",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &joy_forwardthreshold },
};
float joy_sidethreshold;
static cvar_t joy_sidethreshold_cvar = {
	.name = "joysidethreshold",
	.description =
		"FIXME: No Description",
	.default_value = "0.15",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &joy_sidethreshold },
};
float joy_pitchthreshold;
static cvar_t joy_pitchthreshold_cvar = {
	.name = "joypitchthreshold",
	.description =
		"FIXME: No Description",
	.default_value = "0.15",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &joy_pitchthreshold },
};
float joy_yawthreshold;
static cvar_t joy_yawthreshold_cvar = {
	.name = "joyyawthreshold",
	.description =
		"FIXME: No Description",
	.default_value = "0.15",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &joy_yawthreshold },
};
float joy_forwardsensitivity;
static cvar_t joy_forwardsensitivity_cvar = {
	.name = "joyforwardsensitivity",
	.description =
		"FIXME: No Description",
	.default_value = "-1.0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &joy_forwardsensitivity },
};
float joy_sidesensitivity;
static cvar_t joy_sidesensitivity_cvar = {
	.name = "joysidesensitivity",
	.description =
		"FIXME: No Description",
	.default_value = "-1.0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &joy_sidesensitivity },
};
float joy_pitchsensitivity;
static cvar_t joy_pitchsensitivity_cvar = {
	.name = "joypitchsensitivity",
	.description =
		"FIXME: No Description",
	.default_value = "1.0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &joy_pitchsensitivity },
};
float joy_yawsensitivity;
static cvar_t joy_yawsensitivity_cvar = {
	.name = "joyyawsensitivity",
	.description =
		"FIXME: No Description",
	.default_value = "-1.0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &joy_yawsensitivity },
};
int joy_wwhack1;
static cvar_t joy_wwhack1_cvar = {
	.name = "joywwhack1",
	.description =
		"FIXME: No Description",
	.default_value = "0.0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &joy_wwhack1 },
};
float joy_wwhack2;
static cvar_t joy_wwhack2_cvar = {
	.name = "joywwhack2",
	.description =
		"FIXME: No Description",
	.default_value = "0.0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &joy_wwhack2 },
};

int joy_debug;
static cvar_t joy_debug_cvar = {
	.name = "joy_debug",
	.description =
		"FIXME: No Description",
	.default_value = "0.0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &joy_debug },
};

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
		if (joy_wwhack1) {
			ji.dwUpos += 100;
		}
		if (joy_debug) {
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

	if (joy_advanced) {
		// default joystick initialization
		// only 2 axes with joystick control
		dwAxisMap[JOY_AXIS_X] = AxisTurn;
		// dwControlMap[JOY_AXIS_X] = JOY_ABSOLUTE_AXIS;
		dwAxisMap[JOY_AXIS_Y] = AxisForward;
		// dwControlMap[JOY_AXIS_Y] = JOY_ABSOLUTE_AXIS;
	} else {
		if (strcmp (joy_name, "joystick") != 0) {
			// notify user of advanced controller
			Sys_Printf ("\n%s configured\n\n", joy_name);
		}
		// advanced initialization here
		// data supplied by user via joy_axisn cvars
		dwTemp = joy_advaxisx;
		dwAxisMap[JOY_AXIS_X] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_X] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = joy_advaxisy;
		dwAxisMap[JOY_AXIS_Y] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_Y] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = joy_advaxisz;
		dwAxisMap[JOY_AXIS_Z] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_Z] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = joy_advaxisr;
		dwAxisMap[JOY_AXIS_R] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_R] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = joy_advaxisu;
		dwAxisMap[JOY_AXIS_U] = dwTemp & 0x0000000f;
		dwControlMap[JOY_AXIS_U] = dwTemp & JOY_RELATIVE_AXIS;
		dwTemp = joy_advaxisv;
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
	Cvar_Register (&joy_device_cvar, 0, 0);
	Cvar_Register (&joy_enable_cvar, 0, 0);
	Cvar_Register (&joy_sensitivity_cvar, 0, 0);
	Cvar_Register (&in_joystick_cvar, 0, 0);
	Cvar_Register (&joy_name_cvar, 0, 0);
	Cvar_Register (&joy_advanced_cvar, 0, 0);
	Cvar_Register (&joy_advaxisx_cvar, 0, 0);
	Cvar_Register (&joy_advaxisy_cvar, 0, 0);
	Cvar_Register (&joy_advaxisz_cvar, 0, 0);
	Cvar_Register (&joy_advaxisr_cvar, 0, 0);
	Cvar_Register (&joy_advaxisu_cvar, 0, 0);
	Cvar_Register (&joy_advaxisv_cvar, 0, 0);
	Cvar_Register (&joy_forwardthreshold_cvar, 0, 0);
	Cvar_Register (&joy_sidethreshold_cvar, 0, 0);
	Cvar_Register (&joy_pitchthreshold_cvar, 0, 0);
	Cvar_Register (&joy_yawthreshold_cvar, 0, 0);
	Cvar_Register (&joy_forwardsensitivity_cvar, 0, 0);
	Cvar_Register (&joy_sidesensitivity_cvar, 0, 0);
	Cvar_Register (&joy_pitchsensitivity_cvar, 0, 0);
	Cvar_Register (&joy_yawsensitivity_cvar, 0, 0);
	Cvar_Register (&joy_wwhack1_cvar, 0, 0);
	Cvar_Register (&joy_wwhack2_cvar, 0, 0);

	Cvar_Register (&joy_debug_cvar, 0, 0);
	return;
}
#endif
