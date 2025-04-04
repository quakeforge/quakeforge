#include <plist.h>
#include <string.h>
#include <msgbuf.h>
#include <math.h>
#include <imui.h>
#include <qfs.h>
#include <draw.h>
#include <scene.h>
#include <input.h>

#include "armature.h"
#include "pga3d.h"

static string render_graph_cfg =
#embed "qwaq-ed-rg.plist"
;

static string input_cfg =
#embed "qwaq-ed-in.cfg"
;

int in_context;
in_axis_t *cam_move_forward;
in_axis_t *cam_move_side;
in_axis_t *cam_move_up;
in_axis_t *cam_move_pitch;
in_axis_t *cam_move_yaw;
in_axis_t *cam_move_roll;
in_axis_t *mouse_x;
in_axis_t *mouse_y;

void printf (string fmt, ...) = #0;

void init_graphics (plitem_t *config) = #0;
float refresh (scene_t scene) = #0;
void refresh_2d (void (func)(void)) = #0;
void setpalette (void *palette, void *colormap) = #0;
void newscene (scene_t scene) = #0;
void setevents (void (func)(struct IE_event_s *, void *), void *data) = #0;
void setctxcbuf (int ctx) = #0;

imui_ctx_t imui_ctx;
#define IMUI_context imui_ctx
imui_window_t *main_window;
imui_window_t *style_editor;

imui_style_t current_style = {
	.background = {
		.normal = 0x14,
		.hot = 0x14,
		.active = 0x14,
	},
	.foreground = {
		.normal = 0x18,
		.hot = 0x1f,
		.active = 0x1c,
	},
	.text = {
		.normal = 0xfd,
		.hot = 0xfe,
		.active = 0xea,
	},
};

double realtime = double (1ul<<32);
float frametime;

void
camera_first_person (transform_t camera, float dt, state_t *state)
{
	vector dpos = {};
	dpos.x -= IN_UpdateAxis (cam_move_forward);
	dpos.y -= IN_UpdateAxis (cam_move_side);
	dpos.z -= IN_UpdateAxis (cam_move_up);

	vector drot = {};
	drot.x -= IN_UpdateAxis (cam_move_roll);
	drot.y -= IN_UpdateAxis (cam_move_pitch);
	drot.z -= IN_UpdateAxis (cam_move_yaw);

	dpos *= 0.1;
	drot *= ((float)M_PI / 360);
	state.B = {
		.bvect = (PGA.bvect) drot,
		.bvecp = (PGA.bvecp) dpos,
	};
	//float h = dt;
	//use a constant dt instead of actual frame time because the input
	//values accumulate over time and thus already include frame time.
	float h = 0.02f;
	auto ds = dState (*state);
	state.M += h * ds.M;
	state.B += h * ds.B;
	state.M = normalize (state.M);
	set_transform (state.M, camera, "");
}

entity_t mrfixit_ent;
transform_t mrfixit_trans;
armature_t *mrfixit_arm;

static void
color_window (void)
{
	if (!style_editor) {
		style_editor = IMUI_NewWindow ("style");
	}
	static int style_selection;
	static int style_mode;
	static int style_color;
	UI_Window (style_editor) {
		if (IMUI_Window_IsCollapsed (style_editor)) {
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
			imui_style_t style = {};//FIXME qfcc bug
			IMUI_Style_Fetch (imui_ctx, &style);
			auto bg_save = style.background.normal;
			auto fg_save = style.foreground.normal;
			UI_Style (nil) for (int y = 0; y < 16; y++) {
				UI_Horizontal for (int x = 0; x < 16; x++) {
					if (!x) UI_FlexibleSpace ();
					int c = y * 16 + x;
					int ic = y * 16 + (15-x);
					style.background.normal = c;
					style.foreground.normal = c == style_color ? c : ic;
					IMUI_Style_Update (imui_ctx, &style);
					UI_Radio (&style_color, c, sprintf ("##color_%x%x", y, x));
					if (x == 15) {
						style.background.normal = bg_save;
						style.foreground.normal = fg_save;
						IMUI_Style_Update (imui_ctx, &style);
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

transform_t camera;

void
draw_2d (void)
{
	int         width = Draw_Width ();
	int         height = Draw_Height ();
	Draw_String (8, height - 8,
				 sprintf ("%5.2f\xd0\xd2\xc0\xc2", frametime*1000));

	Entity_GetPoseMotors (mrfixit_ent, mrfixit_arm.pose, realtime);
	draw_armature (camera, mrfixit_arm, mrfixit_trans);

	IMUI_SetSize (imui_ctx, Draw_Width (), Draw_Height ());
	IMUI_BeginFrame (imui_ctx);
	IMUI_Style_Update (imui_ctx, &current_style);

	UI_Window (main_window) {
		if (IMUI_Window_IsCollapsed (main_window)) {
			continue;
		}
		UI_FlexibleSpace();
	}
	//color_window ();
	IMUI_Draw (imui_ctx);
}

void
event_hander (struct IE_event_s *event, void *data)
{
	switch (event.type) {
		case ie_add_device:
			printf ("add %d: %s\n", event.device.devid,
					IN_GetDeviceName (event.device.devid));
			break;
		case ie_remove_device:
			printf ("rem %d: %s\n", event.device.devid,
					IN_GetDeviceName (event.device.devid));
			break;
		default:
			if (!IMUI_ProcessEvent (imui_ctx, event)) {
				IN_Binding_HandleEvent (event);
			}
	}
}

void
setup_bindings (void)
{
	in_context = IMT_CreateContext ("scene");
	IMT_SetContext (in_context);
	setctxcbuf (in_context);

	cam_move_forward = IN_CreateAxis ("move.forward", "Camera Fore/Aft");
	cam_move_side = IN_CreateAxis ("move.side", "Camera Left/Right");
	cam_move_up = IN_CreateAxis ("move.up", "Camera Up/Down");
	cam_move_pitch = IN_CreateAxis ("move.pitch", "Camera Pitch");
	cam_move_yaw = IN_CreateAxis ("move.yaw", "Camera Yaw");
	cam_move_roll = IN_CreateAxis ("move.roll", "Camera Roll");
	mouse_x = IN_CreateAxis ("mouse.x", "Mouse X");
	mouse_y = IN_CreateAxis ("mouse.y", "Mouse Y");

	plitem_t *config = PL_GetPropertyList (input_cfg);
	IN_LoadConfig (config);
	PL_Release (config);
}

typedef struct qfm_channel_s {
	uint data;
	float base;
	float scale;
} qfm_channel_t;

string field_names[] = {
	"translate.x",
	"translate.y",
	"translate.z",
	"name",
	"rotate.x",
	"rotate.y",
	"rotate.z",
	"rotate.w",
	"scale.x",
	"scale.y",
	"scale.z",
	"parent",
};

int
main (int argc, string *argv)
{
	plitem_t *config = PL_GetPropertyList (render_graph_cfg);

	IN_LoadConfig (config);
	init_graphics (config);
	PL_Release (config);

	IN_SendConnectedDevices ();

	setup_bindings ();

	//Draw_SetScale (1);
	imui_ctx = IMUI_NewContext ("Consolas", 22);

	main_window = IMUI_NewWindow ("main");
	IMUI_Window_SetSize (main_window, 300, 100);

	refresh_2d (draw_2d);
	setevents (event_hander, nil);

	scene_t scene = Scene_NewScene ();

	entity_t camera_ent = Scene_CreateEntity (scene);
	camera = Entity_GetTransform (camera_ent);
	Scene_SetCamera (scene, camera_ent);

	int key_devid = IN_FindDeviceId ("core:keyboard");
	int lctrl_key = IN_GetButtonNumber (key_devid, "Control_L");
	int rctrl_key = IN_GetButtonNumber (key_devid, "Control_R");
	int q_key = IN_GetButtonNumber (key_devid, "q");


	state_t camera_state = {
		.M = make_motor ({ -4, 0, 3, 0, }, { 0, 0.316227766, 0, 0.948683298 }),
	};

	model_t mrfixit = Model_Load ("progs/mrfixit.iqm");
	printf ("mrfixit: %p\n", mrfixit);
	mrfixit_ent = Scene_CreateEntity (scene);
	mrfixit_trans = Entity_GetTransform (mrfixit_ent);
	Entity_SetModel (mrfixit_ent, mrfixit);
	mrfixit_arm = make_armature (mrfixit);
	Transform_SetLocalRotation (mrfixit_trans, { 0, 0, 1, 0});
	auto clipinfo = Model_GetClipInfo (mrfixit, 0);
	printf ("%s %u %u %u\n", clipinfo.name, clipinfo.num_frames,
			clipinfo.num_channels, clipinfo.channel_type);
	uint count = clipinfo.num_frames * clipinfo.num_channels;
	uint size = (count + 1) / 2;
	void *framedata = obj_malloc (size);
	qfm_channel_t *channels = obj_malloc (clipinfo.num_channels
										  * sizeof (qfm_channel_t));
	Model_GetChannelInfo (mrfixit, channels);
	Model_GetFrameData (mrfixit, 0, framedata);
	int num_joints = Model_NumJoints (mrfixit);
	qfm_joint_t *joints = obj_malloc (num_joints * sizeof (qfm_joint_t));
	Model_GetJoints (mrfixit, joints);
	auto msgbuf = MsgBuf_New (framedata, (int)count * 2);
	for (uint frame = 0; frame < clipinfo.num_frames; frame++) {
		printf ("frame %d\n", frame);
		for (uint chan = 0; chan < clipinfo.num_channels; chan++) {
			int d = MsgBuf_ReadShort (msgbuf);
			int j = channels[chan].data / 48;
			int o = (channels[chan].data % 48) / 4;
			printf ("%4d %5d %9f @ %2d:%d %s:%s\n", chan, d,
					channels[chan].base + channels[chan].scale * d, j, o,
					joints[j].name, field_names[o]);
		}
	}

	lightingdata_t ldata = Light_CreateLightingData (scene);
	Light_EnableSun (ldata);
	Light_AddLight (ldata, {
		{ 1, 1, 1, 500 },
		{ -0.48, -0.64, 0.6, 0 },
		{ 0, -0.8, 0.6, 1 },
		{ 0, 0, 1, 0 },
	}, 0);
	Scene_SetLighting (scene, ldata);
	newscene (scene);

	while (true) {
		frametime = refresh (scene);
		realtime += frametime;

		camera_first_person (camera, frametime, &camera_state);

		in_buttoninfo_t info[2] = {};
		IN_GetButtonInfo (key_devid, lctrl_key, &info[0]);
		IN_GetButtonInfo (key_devid, q_key, &info[1]);
		if (info[0].state && info[1].state) {
			break;
		}
	}
	return 0;
}
