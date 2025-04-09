/*
	binding.h

	Input Mapping Table management

	Copyright (C) 2001 Zephaniah E. Hull <warp@babylon.d2dc.net>
	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

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

#ifndef __QF_input_binding_h
#define __QF_input_binding_h

#ifndef __QFCC__
#include "QF/listener.h"
#include "QF/mathlib.h"
#endif

/** \defgroup input_bindings Input Bindings
	\ingroup input
*/
///@{

/** Recipe for converting an axis to a floating point value.

	Absolute axes are converted to the 0..1 range for unbalanced axes, and
	the -1..1 range for balanced axes, and then scaled.

	Relative axes are simply converted to floating point and scaled as they
	have no fixed limits.

	Relative axes should have \a minzone and \a maxzone set to 0, or weird
	things will happen.

	\a min and \a max normally come from the system. \a min and \a max being
	equal indicates the axis is relative.

	\a deadzone applies only to balanced axes, thus it doubles as a flag
	for balanced (>= 0) or unbalanced (< 0). However, relative axes are always
	balanced and so \a deadzone < 0 is the same as 0 for relative axes.

	\a curve is applied after the input has been converted to a float, and the
	0..1 or -1..1 ranges for absolute axes.

	\a scale is applied after \a curve for absolute axes, but before \a curve
	for relative axes.
*/
typedef struct in_recipe_s {
	int         min;		///< Axis minimum value (from system)
	int         max;		///< Axis maximum value (from system)
	int         minzone;	///< Size of deadzone near axis minimum
	int         maxzone;	///< Size of deadzone near axis maximum
	int         deadzone;	///< Size of deadzone near axis center (balanced)
	float       curve;		///< Power factor for absolute axes
	float       scale;		///< Final scale factor
} in_recipe_t;

typedef enum {
	ina_set,		///< write the axis value to the destination
	ina_accumulate,	///< add the axis value to the destination
} in_axis_mode;

/** Logical axis.

	Logical axes are the inputs defined by the game on which axis inputs
	(usually "physical" axes) can act. Depending on the mode, the physical
	axis value is either written as-is, or added to the existing value. It is
	the responsibility of the code using the axis to clear the value for
	accumulated inputs.
*/
typedef struct in_axis_s {
	float       value;		///< converted value of the axis
	in_axis_mode mode;		///< method used for updating the destination
	float       abs_input;	///< input from an absolute axis (eg, joystick)
	float       rel_input;	///< input from a relative axis (eg, mouse)
#ifndef __QFCC__
	struct axis_listener_set_s *listeners;
	const char *name;
	const char *description;
#endif
} in_axis_t;

#ifndef __QFCC__
typedef struct axis_listener_set_s LISTENER_SET_TYPE (in_axis_t)
	axis_listener_set_t;

/** Function type for axis listeners.
*/
typedef void (*axis_listener_t) (void *data, const in_axis_t *axis);
#endif

/** Current state of the logical button.

	Captures the current state and any transitions during the last frame.
	Not all combinations are valid (inb_edge_up|inb_down and inb_edge_down
	(no inb_down) are not valid states), but inb_edge_up|inb_edge_down )with
	or without inb_down) is valid as it represents a double transition during
	the frame.
*/
typedef enum {
	inb_down      = 1<<0,	///< button is held
	inb_edge_down = 1<<1,	///< button pressed this frame
	inb_edge_up   = 1<<2,	///< button released this frame
} in_button_state;

/** Logical button.

	Logical buttons are the inputs defined by the game on which button inputs
	(usually "physical" buttons) can act. Up to two button inputs can be
	bound to a logical button. The logical button acts as an or gate where
	either input will put the logical button in the pressed state, and both
	inputs must be inactive for the logical button to be released.
*/
typedef struct in_button_s {
	int         down[2];    ///< button ids holding this button down
	int         state;      ///< in_button_state
#ifndef __QFCC__
	struct button_listener_set_s *listeners;
	const char *name;
	const char *description;
#endif
} in_button_t;

#ifndef __QFCC__
typedef struct button_listener_set_s LISTENER_SET_TYPE (in_button_t)
	button_listener_set_t;

/** Function type for button listeners.
*/
typedef void (*button_listener_t) (void *data, const in_button_t *button);

typedef struct in_axisbinding_s {
	in_recipe_t *recipe;
	in_axis_t   *axis;
} in_axisbinding_t;

typedef enum {
	inb_button,
	inb_command,
} in_button_type;

typedef struct in_buttonbinding_s {
	in_button_type type;
	union {
		in_button_t *button;
		char       *command;
	};
} in_buttonbinding_t;

/** Represent the button's activity in the last frame as a float.

	The detected activity is:
		steady off (up)
		steady on (down)
		off to on (up to down) transition
		on to off )down to up) transition
		pulse on (off-on-off or up-down-up)
		pulse off (on-off-on or down-up-down)
	Any additional transitions are treated as a pulse appropriate for the
	final state of the button.

	\param button	Pointer to the button being tested.
	\return			Float value between 0 (off/up) and 1 (on/down)
	\note			The edge transitions are cleared, so for each frame, this
					is a one-shot function (ie, it is NOT idempotent).
*/
GNU89INLINE inline float IN_ButtonState (in_button_t *button);

/** Test whether a button has been pressed in the last frame.

	Both steady-state on, and brief clicks are detected.

	\param button	Pointer to the button being tested.
	\return			True if the button is currently held or was pulsed on
					in the last frame.
	\note			The edge transitions are cleared, so for each frame, this
					is a one-shot function (ie, it is NOT idempotent).
*/
GNU89INLINE inline int IN_ButtonPressed (in_button_t *button);

/** Test whether a button was released in the last frame.

	Valid only if the button is still released. A pulsed off does not
	count as being released as the button is still held.

	\param button	Pointer to the button being tested.
	\return			True if the button is currently released and the release
					was in the last frame.
	\note			The edge transitions are cleared, so for each frame, this
					is a one-shot function (ie, it is NOT idempotent).
*/
GNU89INLINE inline int IN_ButtonReleased (in_button_t *button);

/** Update the axis value based on its mode and clear its relative input.

	The absolute and relative inputs are separate because absolute inputs
	usually get written when the input actually changes (and thus must not
	be cleared each frame), while relative inputs indicate a per-frame delta
	and thus must be cleared each frame.

	\param axis  	Pointer to the axis being updated.
	\return         The resulting output value of the axis.
	\note           The relative input (\a rel_input) is zeroed.
*/
GNU89INLINE inline float IN_UpdateAxis (in_axis_t *axis);

/** Update and clamp the axis value (see IN_UpdateAxis())

	Like IN_UpdateAxis(), but clamps the final output to the specified range.
	This is most useful for \a ina_accumulate axes, but can be used to ensure
	\a ina_set axes never exceed a given range.

	The absolute and relative inputs are separate because absolute inputs
	usually get written when the input actually changes (and thus must not
	be cleared each frame), while relative inputs indicate a per-frame delta
	and thus must be cleared each frame.

	\param axis  	Pointer to the axis being updated.
	\param minval	The minimum value to which the axis output will be clamped.
	\param maxval	The minimum value to which the axis output will be clamped.
	\return         The resulting output value of the axis.
	\note           The relative input (\a rel_input) is zeroed. The absolute
					input is not affected by the clamping, only the output
					\a value.
*/
GNU89INLINE inline float IN_ClampAxis (in_axis_t *axis,
									   float minval, float maxval);

#ifndef IMPLEMENT_INPUT_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
float
IN_ButtonState (in_button_t *button)
{
	static const float state_values[8] = {
		// held down for the entire frame
		[inb_down] = 1,
		// released this frame
		[inb_edge_up] = 0,	// instant falloff
		// pressed this frame
		[inb_edge_down|inb_down] = 0.5,
		// pressed and released this frame
		[inb_edge_down|inb_edge_up] = 0.25,
		// released and pressed this frame
		[inb_edge_down|inb_edge_up|inb_down] = 0.75,
	};
	int         state = button->state;
	button->state &= inb_down;	// clear edges, preserve pressed
	return state_values[state & (inb_down|inb_edge_down|inb_edge_up)];
}

#ifndef IMPLEMENT_INPUT_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
int
IN_ButtonPressed (in_button_t *button)
{
	int         state = button->state;
	button->state &= inb_down;	// clear edges, preserve pressed
	// catch even press and release that occurs between frames
	return (state & (inb_down | inb_edge_down)) != 0;
}

#ifndef IMPLEMENT_INPUT_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
int
IN_ButtonReleased (in_button_t *button)
{
	int         state = button->state;
	button->state &= inb_down;	// clear edges, preserve pressed
	// catch only full release (a pulsed on does count as a release)
	return (state & (inb_down | inb_edge_up)) == inb_edge_up;
}

#ifndef IMPLEMENT_INPUT_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
float
IN_UpdateAxis (in_axis_t *axis)
{
	float       prev_value = axis->value;
	switch (axis->mode) {
		case ina_set:
			axis->value = axis->abs_input + axis->rel_input;
			break;
		case ina_accumulate:
			axis->value += axis->abs_input + axis->rel_input;
			break;
	}
	axis->rel_input = 0;
	if (axis->value != prev_value && axis->listeners) {
		LISTENER_INVOKE (axis->listeners, axis);
	}
	return axis->value;
}

#ifndef IMPLEMENT_INPUT_Funcs
GNU89INLINE inline
#else
VISIBLE
#endif
float
IN_ClampAxis (in_axis_t *axis, float minval, float maxval)
{
	float       prev_value = axis->value;
	switch (axis->mode) {
		case ina_set:
			axis->value = axis->abs_input + axis->rel_input;
			break;
		case ina_accumulate:
			axis->value += axis->abs_input + axis->rel_input;
			break;
	}
	axis->rel_input = 0;
	axis->value = bound (minval, axis->value, maxval);
	if (axis->value != prev_value && axis->listeners) {
		LISTENER_INVOKE (axis->listeners, axis);
	}
	return axis->value;
}

void IN_ButtonAction (in_button_t *buttin, int id, int pressed);

int IN_RegisterButton (in_button_t *button);
int IN_UnregisterButton (in_button_t *button);
void IN_ButtonInit (void);
int IN_RegisterAxis (in_axis_t *axis);
int IN_UnregisterAxis (in_axis_t *axis);
void IN_AxisInit (void);
in_button_t *IN_FindButton (const char *name);
void IN_ButtonClearStates (void);
in_axis_t *IN_FindAxis (const char *name);
void IN_AxisClearStates (void);
void IN_ButtonAddListener (in_button_t *button, button_listener_t listener,
						   void *data);
void IN_ButtonRemoveListener (in_button_t *button, button_listener_t listener,
							  void *data);
void IN_AxisAddListener (in_axis_t *axis, axis_listener_t listener,
						 void *data);
void IN_AxisRemoveListener (in_axis_t *axis, axis_listener_t listener,
							void *data);

struct IE_event_s;
int IN_Binding_HandleEvent (const struct IE_event_s *ie_event);
void IN_Binding_Activate (void);
void IN_Binding_Init (void);
struct plitem_s;
void IN_Binding_SaveConfig (struct plitem_s *config);
void IN_Binding_LoadConfig (struct plitem_s *config);

#endif

///@}

#endif//__QF_input_binding_h
