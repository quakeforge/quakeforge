/*
	xi.c

	xinput input driver

	Copyright (C) 2026 Bill Currie <bill@taniwha.org>
	Please see the file "AUTHORS" for a list of contributors

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

// Partially derived from SDL, but just to be safe...
/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "winquake.h"
#include <xinput.h>

#include "QF/dstring.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "xinput/xi.h"

// defined in mxe's xinput.h
#if 0
typedef struct _XINPUT_CAPABILITIES_EX {
	XINPUT_CAPABILITIES Capabilities;
	WORD VendorId;
	WORD ProductId;
	WORD ProductVersion;
	WORD unk1;
	DWORD unk2;
} XINPUT_CAPABILITIES_EX;
#endif

typedef DWORD(WINAPI *XInputGetState_t) (DWORD dwUserIndex, XINPUT_STATE *pState);
typedef DWORD(WINAPI *XInputSetState_t) (DWORD dwUserIndex, XINPUT_VIBRATION *pVibration);
typedef DWORD(WINAPI *XInputGetCapabilities_t) (DWORD dwUserIndex, DWORD dwFlags,
												XINPUT_CAPABILITIES *pCapabilities);
typedef DWORD(WINAPI *XInputGetCapabilitiesEx_t)(DWORD dwReserved, DWORD dwUserIndex,
												 DWORD dwFlags,
												 XINPUT_CAPABILITIES_EX *pCapabilitiesEx);
typedef DWORD(WINAPI *XInputGetBatteryInformation_t)(DWORD dwUserIndex, BYTE devType,
								 XINPUT_BATTERY_INFORMATION *pBatteryInformation);

static HMODULE xinput_dll = nullptr;
static XInputGetState_t qf_XInputGetState;
static XInputSetState_t qf_XInputSetState;
static XInputGetCapabilities_t qf_XInputGetCapabilities;
static XInputGetCapabilitiesEx_t qf_XInputGetCapabilitiesEx;
static XInputGetBatteryInformation_t qf_XInputGetBatteryInformation;

// apparently, xinput supports up to 4 devices
static xi_device_t *devices[4];
static int64_t last_check[4] = {-10000000, -10000000, -10000000, -10000000};
void (*device_add) (xi_device_t *);
void (*device_remove) (xi_device_t *);

static xi_device_t *
xi_setup_device (int user)
{
	XINPUT_CAPABILITIES_EX cap = {};
	if (qf_XInputGetCapabilitiesEx) {
		qf_XInputGetCapabilitiesEx (1, user, 0, &cap);
	}
	xi_device_t *dev = malloc (sizeof (xi_device_t));
	*dev = (xi_device_t) {
		.user = user,
		.bustype = 3,	//FIXME is this correct?
		.vendor = cap.VendorId,
		.product = cap.ProductId,
		.version = cap.VersionNumber,
		.num_buttons = 16,	// not strictly true (includes dpad), but meh
		.num_axes = 6,
	};
	int len = sizeof (dev->name) - 1;
	strncpy (dev->name, va ("xinput:%d\n", user), len);
	dev->name[len] = 0;

	for (unsigned i = 0; i < countof (dev->buttons); i++) {
		dev->buttons[i] = (button_t) {
			.num = i,
			.xnum = i,
		};
	}
	for (unsigned i = 0; i < countof (dev->axes); i++) {
		dev->axes[i] = (axis_t) {
			.num = i,
			.xnum = i,
			// 0,1,3,4 are sticks, 2,5 are triggers
			.min = ((1 << i) & 0x1b) ? -32768 : 0,
			.max = ((1 << i) & 0x1b) ?  32767 : 255,
		};
	}
	return dev;
}

int __attribute__((const))
xi_check_input (void)
{
	int64_t time = Sys_LongTime ();

	for (unsigned i = 0; i < countof (devices); i++) {
		if (!devices[i]) {
			if (time - last_check[i] < 2 * 1000000) {
				continue;
			}
			last_check[i] = time;
		}
		XINPUT_STATE xi_state;
		DWORD result = qf_XInputGetState (i, &xi_state);
		if (result == ERROR_DEVICE_NOT_CONNECTED) {
			if (devices[i]) {
				if (device_remove) {
					device_remove (devices[i]);
				}
				free (devices[i]);
				devices[i] = 0;
			}
			continue;
		}
		if (!devices[i]) {
			devices[i] = xi_setup_device (i);
			device_add (devices[i]);
		}
		auto dev = devices[i];
		for (int i = 0; i < dev->num_buttons; i++) {
			static byte map[16] = {
				12, 13, 14, 15,
				8,   9,  5,  4,
				6,   7, 10, 11,
				0,   1,  2,  3};
			int state = !!(xi_state.Gamepad.wButtons & (1u << map[i]));
			auto button = &dev->buttons[i];
			bool changed = state != button->state;
			button->state = state;
			if (changed && dev->button_event) {
				dev->button_event (button, dev->data);
			}
		}
#define check_axis(s, field, number, zone) \
		do { \
			int value = s xi_state.Gamepad.field; \
			value = abs (value) < zone ? 0 : value; \
			auto axis = &dev->axes[number]; \
			/*printf ("%d %d %d\n", number, axis->min, axis->max);*/\
			bool changed = value != axis->value; \
			axis->value = value; \
			if (changed && dev->axis_event) { \
				dev->axis_event (axis, dev->data); \
			} \
		} while (false)

		check_axis (+, sThumbLX, 0, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
		check_axis (-, sThumbLY, 1, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
		check_axis (+, bLeftTrigger, 2, XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
		check_axis (+, sThumbRX, 3, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
		check_axis (-, sThumbRY, 4, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
		check_axis (+, bRightTrigger, 5, XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
		//printf ("%d: %5ld :: %04x %3d %3d %6d %6d %6d %6d\n", i,
		//		xi_state.dwPacketNumber, xi_state.Gamepad.wButtons,
		//		xi_state.Gamepad.bLeftTrigger, xi_state.Gamepad.bRightTrigger,
		//		xi_state.Gamepad.sThumbLX, xi_state.Gamepad.sThumbLY,
		//		xi_state.Gamepad.sThumbRX, xi_state.Gamepad.sThumbRY);
	}
	return 0;
}

#define _getprocaddr(sym, name, ...) \
	(qf_##sym = (sym##_t)GetProcAddress(xinput_dll, name))
#define getprocaddr(name, ...) \
	_getprocaddr(name __VA_OPT__(, (LPCSTR)__VA_ARGS__), #name)

int
xi_init (const char *library,
		 void (*dev_add) (xi_device_t *), void (*dev_rem) (xi_device_t *))
{
	xinput_dll = LoadLibrary (library);
	if (!xinput_dll) {
		DWORD       errcode = GetLastError ();
		Sys_Printf ("Couldn't load xinput library %s: %ld\n",
					library, errcode);
	}
	if (!getprocaddr (XInputGetState, 100)) {
		getprocaddr (XInputGetState);
	}
	getprocaddr (XInputSetState);
	getprocaddr (XInputGetCapabilities);
	getprocaddr (XInputGetCapabilitiesEx, 108);
	getprocaddr (XInputGetBatteryInformation);

	if (!qf_XInputGetState || !qf_XInputGetCapabilities) {
		FreeLibrary (xinput_dll);
		xinput_dll = nullptr;
		return -1;
	}
	device_add = dev_add;
	device_remove = dev_rem;
	return 0;
}

void
xi_close (void)
{
	for (unsigned i = 0; i < countof (devices); i++) {
		free (devices[i]);
	}
	if (xinput_dll) {
		FreeLibrary (xinput_dll);
		xinput_dll = nullptr;
	}
}
