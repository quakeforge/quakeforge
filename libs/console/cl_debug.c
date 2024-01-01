#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/cbuf.h"
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/keys.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/input/event.h"

#include "QF/scene/scene.h"
#include "QF/scene/transform.h"

#include "QF/ui/canvas.h"
#include "QF/ui/imui.h"

#include "QF/plugin/console.h"
#include "QF/plugin/vid_render.h"

#include "cl_console.h"

static in_axis_t deb_move_forward = {
	.mode = ina_set,
	.name = "debug.move.forward",
	.description = "[debug] Move forward (negative) or backward (positive)",
};
static in_axis_t deb_move_side = {
	.mode = ina_set,
	.name = "debug.move.side",
	.description = "[debug] Move right (positive) or left (negative)",
};
static in_axis_t deb_move_up = {
	.mode = ina_set,
	.name = "debug.move.up",
	.description = "[debug] Move up (negative) or down (positive)",
};
static in_axis_t deb_move_pitch = {
	.mode = ina_set,
	.name = "debug.move.pitch",
	.description = "[debug] Pitch axis",
};
static in_axis_t deb_move_yaw = {
	.mode = ina_set,
	.name = "debug.move.yaw",
	.description = "[debug] Yaw axis",
};
static in_axis_t deb_move_roll = {
	.mode = ina_set,
	.name = "debug.move.roll",
	.description = "[debug] Roll axis",
};
static in_axis_t deb_mouse_x = {
	.mode = ina_set,
	.name = "debug.mouse.x",
	.description = "[debug] Mouse X",
};
static in_axis_t deb_mouse_y = {
	.mode = ina_set,
	.name = "debug.mouse.y",
	.description = "[debug] Mouse Y",
};
static in_button_t deb_left = {
	.name = "debug.left",
	.description = "[debug] When active the player is turning left"
};
static in_button_t deb_right = {
	.name = "debug.right",
	.description = "[debug] When active the player is turning right"
};
static in_button_t deb_forward = {
	.name = "debug.forward",
	.description = "[debug] When active the player is moving forward"
};
static in_button_t deb_back = {
	.name = "debug.back",
	.description = "[debug] When active the player is moving backwards"
};
static in_button_t deb_lookup = {
	.name = "debug.lookup",
	.description = "[debug] When active the player's view is looking up"
};
static in_button_t deb_lookdown = {
	.name = "debug.lookdown",
	.description = "[debug] When active the player's view is looking down"
};
static in_button_t deb_moveleft = {
	.name = "debug.moveleft",
	.description = "[debug] When active the player is strafing left"
};
static in_button_t deb_moveright = {
	.name = "debug.moveright",
	.description = "[debug] When active the player is strafing right"
};
static in_button_t deb_up = {
	.name = "debug.moveup",
	.description = "[debug] When active the player is swimming up in a liquid"
};
static in_button_t deb_down = {
	.name = "debug.movedown",
	.description = "[debug] When active the player is swimming down in a liquid"
};
static in_axis_t *deb_in_axes[] = {
	&deb_move_forward,
	&deb_move_side,
	&deb_move_up,
	&deb_move_pitch,
	&deb_move_yaw,
	&deb_move_roll,
	&deb_mouse_x,
	&deb_mouse_y,
	0
};
static in_button_t *deb_in_buttons[] = {
	&deb_left,
	&deb_right,
	&deb_forward,
	&deb_back,
	&deb_lookup,
	&deb_lookdown,
	&deb_moveleft,
	&deb_moveright,
	&deb_up,
	&deb_down,
	0
};

static int debug_event_id;
static int debug_saved_focus;
static int debug_context;
static int debug_saved_context;
static imui_ctx_t *debug_imui;
static int64_t  debug_enable_time;
#define IMUI_context debug_imui

static scene_t *debug_scene;
static transform_t debug_camera_pivot;
static transform_t debug_camera;

bool con_debug;
static cvar_t con_debug_cvar = {
	.name = "con_debug",
	.description =
		"Enable the debug interface",
	.default_value = "false",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_bool, .value = &con_debug },
};
static char *deb_fontname;
static cvar_t deb_fontname_cvar = {
	.name = "deb_fontname",
	.description =
		"Font name/patter for debug interface",
	.default_value = "Consolas",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &deb_fontname },
};
static float deb_fontsize;
static cvar_t deb_fontsize_cvar = {
	.name = "deb_fontsize",
	.description =
		"Font size for debug interface",
	.default_value = "22",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &deb_fontsize },
};

static int deb_xlen = -1;
static int deb_ylen = -1;
static vec4f_t mouse_start;

static void
con_debug_f (void *data, const cvar_t *cvar)
{
	Con_Show_Mouse (con_debug);
	if (debug_imui) {
		IMUI_SetVisible (debug_imui, con_debug);
		debug_enable_time = Sys_LongTime ();
		if (!con_debug) {
			IE_Set_Focus (debug_saved_focus);
			IMT_SetContext (debug_saved_context);
			r_override_camera = false;
		} else {
			IMUI_SetSize (debug_imui, deb_xlen, deb_ylen);
		}
	}
}

static int
debug_app_window (const IE_event_t *ie_event)
{
	if (deb_xlen != ie_event->app_window.xlen
		|| deb_ylen != ie_event->app_window.ylen) {
		deb_xlen = ie_event->app_window.xlen;
		deb_ylen = ie_event->app_window.ylen;
		IMUI_SetSize (debug_imui, deb_xlen, deb_ylen);
	}
	return 1;
}

static int
capture_mouse_event (const IE_event_t *ie_event)
{
	static IE_mouse_event_t prev_mouse;
	static bool dragging;

	if (ie_event->mouse.type == ie_mousedown
		&& ((ie_event->mouse.buttons ^ prev_mouse.buttons) & 4)) {
		IN_UpdateGrab (1);
		dragging = true;
		mouse_start = (vec4f_t) {
			ie_event->mouse.x,
			ie_event->mouse.y,
		};
	} else if (ie_event->mouse.type == ie_mouseup
			   && ((ie_event->mouse.buttons ^ prev_mouse.buttons) & 4)) {
		IN_UpdateGrab (0);
		dragging = false;
	}

	prev_mouse = ie_event->mouse;
	return !dragging;
}

static int
debug_mouse (const IE_event_t *ie_event)
{
	if (!IMUI_ProcessEvent (debug_imui, ie_event)) {
		return capture_mouse_event (ie_event);
	}
	return 1;
}

static void
close_debug (void)
{
	con_debug = false;
	con_debug_f (0, &con_debug_cvar);
}

static int
debug_key (const IE_event_t *ie_event)
{
	int shift = ie_event->key.shift & ~(ies_capslock | ies_numlock);
	if (ie_event->key.code == QFK_ESCAPE && shift == ies_control) {
		close_debug ();
	}
	IMUI_ProcessEvent (debug_imui, ie_event);
	return 1;
}

static int
debug_event_handler (const IE_event_t *ie_event, void *data)
{
	static int (*handlers[ie_event_count]) (const IE_event_t *ie_event) = {
		[ie_app_window] = debug_app_window,
		[ie_mouse] = debug_mouse,
		[ie_key] = debug_key,
	};
	if ((unsigned) ie_event->type >= ie_event_count
		|| !handlers[ie_event->type]) {
		return IN_Binding_HandleEvent (ie_event);
	}
	return handlers[ie_event->type] (ie_event);
}

void
Con_Debug_InitCvars (void)
{
	Cvar_Register (&con_debug_cvar, con_debug_f, 0);
	Cvar_Register (&deb_fontname_cvar, 0, 0);
	Cvar_Register (&deb_fontsize_cvar, 0, 0);
}

static imui_style_t current_style;
void
Con_Debug_Init (void)
{
	for (int i = 0; deb_in_axes[i]; i++) {
		IN_RegisterAxis (deb_in_axes[i]);
	}
	for (int i = 0; deb_in_buttons[i]; i++) {
		IN_RegisterButton (deb_in_buttons[i]);
	}

	debug_event_id = IE_Add_Handler (debug_event_handler, 0);
	debug_context = IMT_CreateContext ("debug.context");
	debug_imui = IMUI_NewContext (*con_data.canvas_sys,
								  deb_fontname, deb_fontsize);
	IMUI_SetVisible (debug_imui, con_debug);
	IMUI_Style_Fetch (debug_imui, &current_style);

	debug_scene = Scene_NewScene (0);
}

void
Con_Debug_Shutdown (void)
{
	IE_Remove_Handler (debug_event_id);

	Scene_DeleteScene (debug_scene);
}

static void
color_window (void)
{
	static imui_window_t style_editor = {
		.name = "Style Colors",
		.xpos = 75,
		.ypos = 75,
		.is_open = false,
		.auto_fit = true,
	};
	static int style_selection;
	static int style_mode;
	static int style_color;

	UI_Window (&style_editor) {
		if (style_editor.is_collapsed) {
			continue;
		}
		UI_Vertical {
			UI_Horizontal {
				UI_FlexibleSpace ();
				UI_Radio (&style_selection, 0, "Background");
				UI_FlexibleSpace ();
				UI_Radio (&style_selection, 1, "Foreground");
				UI_FlexibleSpace ();
				UI_Radio (&style_selection, 2, "Text");
				UI_FlexibleSpace ();
			}

			UI_Horizontal {
				UI_FlexibleSpace ();
				UI_Radio (&style_mode, 0, "Normal");
				UI_FlexibleSpace ();
				UI_Radio (&style_mode, 1, "Hot");
				UI_FlexibleSpace ();
				UI_Radio (&style_mode, 2, "Active");
				UI_FlexibleSpace ();
			}

			if (style_selection == 0) {
				style_color = current_style.background.color[style_mode];
			} else if (style_selection == 1) {
				style_color = current_style.foreground.color[style_mode];
			} else if (style_selection == 2) {
				style_color = current_style.text.color[style_mode];
			}

			imui_style_t style;
			IMUI_Style_Fetch (debug_imui, &style);
			auto bg_save = style.background.normal;
			auto fg_save = style.foreground.normal;
			UI_Style (0) for (int y = 0; y < 16; y++) {
				UI_Horizontal for (int x = 0; x < 16; x++) {
					if (!x) UI_FlexibleSpace ();
					int c = y * 16 + x;
					int ic = y * 16 + (15-x);
					style.background.normal = c;
					style.foreground.normal = c == style_color ? c : ic;
					IMUI_Style_Update (debug_imui, &style);
					UI_Radio (&style_color, c, va (0, "##color_%x%x", y, x));
					if (x == 15) {
						style.background.normal = bg_save;
						style.foreground.normal = fg_save;
						IMUI_Style_Update (debug_imui, &style);
						UI_FlexibleSpace ();
					}
				}
			}
		}
		if (style_selection == 0) {
			current_style.background.color[style_mode] = style_color;
		} else if (style_selection == 1) {
			current_style.foreground.color[style_mode] = style_color;
		} else if (style_selection == 2) {
			current_style.text.color[style_mode] = style_color;
		}
	}
}

static transform_t
create_debug_camera (void)
{
	ecs_system_t ssys = { .reg = debug_scene->reg, .base = debug_scene->base };
	debug_camera_pivot = Transform_New (ssys, nulltransform);
	debug_camera = Transform_New (ssys, debug_camera_pivot);
	return debug_camera;
}

static void
camera_mouse_first_person (void)
{
	vec4f_t     delta_pos = {};
	vec4f_t     delta_rot = {};

	delta_pos[0] -= IN_UpdateAxis (&deb_move_forward);
	delta_pos[1] -= IN_UpdateAxis (&deb_move_side);
	delta_pos[2] -= IN_UpdateAxis (&deb_move_up);

	float dt = *con_data.frametime;

	vec4f_t     rot = Transform_GetLocalRotation (debug_camera_pivot);
	delta_pos = qvmulf (rot, delta_pos) * dt;
	vec4f_t     pos = Transform_GetLocalPosition (debug_camera_pivot);
	Transform_SetLocalPosition (debug_camera_pivot, pos + delta_pos);

	delta_rot[PITCH] -= IN_UpdateAxis (&deb_move_pitch);
	delta_rot[YAW] -= IN_UpdateAxis (&deb_move_yaw);
	delta_rot[ROLL] -= IN_UpdateAxis (&deb_move_roll);
	delta_rot *= dt;
	vec4f_t     drot;
	AngleQuat ((vec_t*)&delta_rot, (vec_t*)&drot);
	vec4f_t r = normalf (qmulf (rot, drot));
	Transform_SetLocalRotation (debug_camera_pivot, r);
}

static vec4f_t
trackball_vector (vec4f_t xy)
{
	// xy is already in -1..1 order of magnitude range (ie, might be a bit
	// over due to screen aspect)
	// This is very similar to blender's trackball calculation (based on it,
	// really)
	float r = 1;
	float t = r * r / 2;
	float d = dotf (xy, xy)[0];
	vec4f_t vec = xy;
	if (d < t) {
		// less than 45 degrees around the sphere from the viewer facing
		// pole, so map the mouse point to the sphere
		vec[2] = sqrt (r * r - d);
	} else {
		// beyond 45 degrees around the sphere from the veiwer facing
		// pole, so the slope is rapidly approaching infinity or the mouse
		// point may miss the sphere entirely, so instead map the mouse
		// point to the hyperbolic cone pointed towards the viewer. The
		// cone and sphere are tangential at the 45 degree latitude
		vec[2] = t / sqrt (d);
	}
	return (vec4f_t) { vec[2], vec[0], vec[1] };
}
static float trackball_sensitivity = 0.1;
#define sphere_scale 1.0f
static void
camera_mouse_trackball (void)
{
	vec4f_t     delta = {
		IN_UpdateAxis (&deb_mouse_x),
		IN_UpdateAxis (&deb_mouse_y),
	};
	float       size = min (deb_xlen, deb_ylen) / 2;
	vec4f_t     center = { deb_xlen, deb_ylen }; center /= 2;
	vec4f_t     start = mouse_start - center;
	vec4f_t     end = start + delta;
	start = trackball_vector (start / (size * sphere_scale));
	end = trackball_vector (end / (size * sphere_scale));
	vec4f_t     drot = crossf (end, start);
	float       ma = dotf (drot, drot)[0];
	if (ma < 1e-7) {
		return;
	}
	drot /= sqrtf (ma);
	float       dist = sqrtf (dotf (delta, delta)[0]) * trackball_sensitivity;
	dist /= size;
	drot *= sinf (dist);
	drot[3] = cosf (dist);

	vec4f_t     rot = Transform_GetLocalRotation (debug_camera_pivot);
	vec4f_t r = normalf (qmulf (rot, drot));
	Transform_SetLocalRotation (debug_camera_pivot, r);
}

static void
hs (void)
{
	IMUI_Spacer (debug_imui, imui_size_pixels, 10, imui_size_expand, 100);
}

static imui_window_t system_info_window = {
	.name = "System Info##window",
	.xpos = 50,
	.ypos = 50,
	.auto_fit = true,
};

static imui_window_t cam_window = {
	.name = "Debug Camera",
	.auto_fit = true,
};

static imui_window_t inp_window = {
	.name = "Debug Input",
	.auto_fit = true,
};

static imui_window_t debug_menu = {
	.name = "Debug##menu",
	.group_offset = 0,
	.is_open = true,
	.no_collapse = true,
	.auto_fit = true,
};

static imui_window_t renderer_menu = {
	.name = "Renderer##menu",
	.group_offset = 1,
	.reference_gravity = grav_northwest,
	.anchor_gravity = grav_southwest,
	.auto_fit = true,
};

static void
menu_bar (void)
{
	UI_MenuBar (&debug_menu) {
		if (UI_MenuItem ("System Info##menu_item")) {
			system_info_window.is_open = true;
		}
		hs ();
		if (UI_MenuItem ("Camera")) {
			cam_window.is_open = true;
		}
		hs ();
		if (UI_MenuItem ("Input")) {
			inp_window.is_open = true;
		}
		if (r_funcs->debug_ui) {
			hs ();
			// create the menu so the renderer can access it
			UI_Menu (&renderer_menu);
		}
	}
}

static void
camera_window (void)
{
	static int cam_state;
	static int cam_mode;

	int old_mode = cam_mode;
	UI_Window (&cam_window) {
		if (cam_window.is_collapsed) {
			continue;
		}
		UI_Horizontal {
			UI_Radio (&cam_state, 0, "Game##camera");
			IMUI_Spacer (debug_imui, imui_size_pixels, 10,
						 imui_size_expand, 100);
			UI_Radio (&cam_state, 1, "Debug##camera");
			UI_FlexibleSpace ();
		}
		if (cam_state) {
			UI_Horizontal {
				UI_Radio (&cam_mode, 0, "First Person##camera");
				IMUI_Spacer (debug_imui, imui_size_pixels, 10,
							 imui_size_expand, 100);
				UI_Radio (&cam_mode, 1, "Trackball##camera");
//				IMUI_Spacer (debug_imui, imui_size_pixels, 10,
//							 imui_size_expand, 100);
//				UI_Radio (&cam_mode, 2, "Turntable##camera");
				UI_FlexibleSpace ();
			}
			UI_Horizontal {
				UI_Checkbox (&r_lock_viewleaf, "Lock VIS##camera");
				UI_FlexibleSpace ();
			}
		}
		if (r_override_camera) {
			if (cam_mode == 0) {
				camera_mouse_first_person();
			} else if (cam_mode == 1) {
				camera_mouse_trackball ();
			}
		}
	}
	if (cam_state) {
		if (!Transform_Valid (r_camera)) {
			r_camera = create_debug_camera ();
		}
		if (!r_override_camera || old_mode != cam_mode) {
			// the camera is still whatever the client set it to (or what
			// the renderer last used if switching modes)
			static vec4f_t forward = {1, 0, 0, 0};
			static vec4f_t up = {0, 0, 1, 0};
			static vec4f_t qident = {0, 0, 0, 1};

			vec4f_t f = r_data->refdef->camera[0];	//X is forward
			vec4f_t u = r_data->refdef->camera[2];	//Z is up
			vec4f_t q1 = qrotf (forward, f);
			vec4f_t up1 = qvmulf (q1, up);
			u -= dotf (u, f) * f; // ensure orthogonal (should be, but...)
			vec4f_t q2 = qrotf (up1, u);
			vec4f_t q = qmulf (q2, q1);

			Transform_SetLocalRotation (debug_camera_pivot, q);

			vec4f_t pos = r_data->refdef->camera[3];
			float dist = cam_mode == 0 ? 0 : 128;
			vec4f_t offs = dist * Transform_Forward (debug_camera_pivot);

			Transform_SetLocalPosition (debug_camera_pivot, pos + offs);

			Transform_SetLocalRotation (debug_camera, qident);
			Transform_SetLocalPosition (debug_camera, (vec4f_t) {-dist,0,0,1});
		}
		r_override_camera = true;
	} else {
		r_override_camera = false;
	}
}

static void
input_window (void)
{
	UI_Window (&inp_window) {
		if (inp_window.is_collapsed) {
			continue;
		}
		for (int i = 0; deb_in_axes[i]; i++) {
			auto axis = deb_in_axes[i];
			UI_Horizontal {
				UI_Label (axis->name);
				UI_FlexibleSpace ();
				UI_Labelf ("%8g", axis->rel_input);
			}
		}
	}
}

static bool close_debug_pressed = false;
static void
system_info (void)
{
	UI_Window (&system_info_window) {
		if (system_info_window.is_collapsed) {
			continue;
		}
		UI_Vertical {
			UI_Horizontal {
				if (UI_Button ("Close Debug")) {
					close_debug_pressed = true;
				}
				IMUI_Spacer (debug_imui, imui_size_pixels, 10,
							 imui_size_expand, 100);
				if (UI_Button ("Screenshot")) {
					Cbuf_AddText (con_data.cbuf, "screenshot\n");
				}
				UI_FlexibleSpace ();
			}
#if 0
			for (int i = 0; deb_in_axes[i]; i++) {
				UI_Horizontal {
					UI_Label (deb_in_axes[i]->name);
					UI_FlexibleSpace ();
					//IN_UpdateAxis (deb_in_axes[i]);
					UI_Labelf ("%6.3f", deb_in_axes[i]->value);
				}
			}
#endif
			static int64_t prev_time;
			static int64_t min_delta = INT64_MAX;
			static int64_t max_delta = -INT64_MAX;
			int64_t cur_time = Sys_LongTime ();
			int64_t delta = cur_time - prev_time;
			prev_time = cur_time;
			min_delta = min (min_delta, delta);
			max_delta = max (max_delta, delta);
			UI_Horizontal {
				UI_Label ("frame: ");
				UI_FlexibleSpace ();
				UI_Labelf ("%'7"PRIu64"\u03bcs##frame.time", min_delta);
				UI_Labelf ("%'7"PRIu64"\u03bcs##frame.time", delta);
				UI_Labelf ("%'7"PRIu64"\u03bcs##frame.time", max_delta);
			}
			UI_Horizontal {
				UI_FlexibleSpace ();
				if (UI_Button ("Reset##timings")) {
					max_delta = -INT64_MAX;
					min_delta = INT64_MAX;
				}
			}
			UI_Horizontal {
				UI_Labelf ("mem: %'zd", Sys_CurrentRSS ());
				UI_FlexibleSpace ();
			}
			UI_FlexibleSpace ();
		}
	}
}

void
Con_Debug_Draw (void)
{
	if (debug_enable_time && Sys_LongTime () - debug_enable_time > 1000) {
		debug_saved_focus = IE_Get_Focus ();
		IE_Set_Focus (debug_event_id);
		debug_saved_context = IMT_GetContext ();
		IMT_SetContext (debug_context);
		debug_enable_time = 0;
	}

	IMUI_BeginFrame (debug_imui);
	IMUI_Style_Update (debug_imui, &current_style);
	menu_bar ();
	system_info ();
	color_window ();
	camera_window ();
	input_window ();
	if (r_funcs->debug_ui) {
		r_funcs->debug_ui (debug_imui);
	}

	if (close_debug_pressed) {
		close_debug_pressed = false;
		close_debug ();
	}

	IMUI_Draw (debug_imui);
}
