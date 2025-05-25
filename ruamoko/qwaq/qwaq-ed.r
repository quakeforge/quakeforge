#include <Array.h>
#include <AutoreleasePool.h>
#include <plist.h>
#include <string.h>
#include <msgbuf.h>
#include <math.h>
#include <imui.h>
#include <qfs.h>
#include <draw.h>
#include <scene.h>
#include <input.h>

#include "gui/editwindow.h"
#include "gui/filewindow.h"
#include "gui/listview.h"
#include "armature.h"
#include "pga3d.h"

static string render_graph_cfg =
#embed "config/qwaq-ed-rg.plist"
;

static string input_cfg =
#embed "config/qwaq-ed-in.cfg"
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
vec2 mouse_start;
bool mouse_dragging_mmb;
bool mouse_dragging_rmb;

void printf (string fmt, ...) = #0;

void init_graphics (plitem_t *config) = #0;
float refresh (scene_t scene) = #0;
void refresh_2d (void (func)(void)) = #0;
void setpalette (void *palette, void *colormap) = #0;
void newscene (scene_t scene) = #0;
void setevents (int (func)(struct IE_event_s *, void *), void *data) = #0;
void setctxcbuf (int ctx) = #0;

void Painter_AddLine (vec2 p1, vec2 p2, float r, vec4 color) = #0;
void Painter_AddCircle (vec2 c, float r, vec4 color) = #0;
void Painter_AddBox (vec2 c, vec2 e, float r, vec4 color) = #0;

imui_ctx_t imui_ctx;
#define IMUI_context imui_ctx
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

@interface MainWindow : Object
{
	Array *clips;
	ListView *clipsView;
	Array *bones;
	ListView *bonesView;

	imui_ctx_t IMUI_context;
	imui_window_t *window;

	scene_t scene;
	transform_t camera;

	int show_armature;

	animstate_t anim;
	entity_t ent;
	transform_t trans;
	armature_t *arm;
	model_t model;
	int num_clips;
	int clip_num;
	float timer;
}
+(MainWindow *) window:(imui_ctx_t)ctx;
-draw;
-setModel:(model_t) model;
-nextClip:(float)frametime;
-(scene_t) scene;
-(transform_t) camera;
@end

@implementation MainWindow

-initWithContext:(imui_ctx_t)ctx
{
	if (!(self = [super init])) {
		return nil;
	}
	IMUI_context = ctx;

	clips = [[Array array] retain];
	clipsView = [[ListView list:"MainWindow:clips" ctx:ctx] retain];

	//bones = [[Array array] retain];
	//bonesView = [[ListView list:"MainWindow:bones" ctx:ctx] retain];

	window = IMUI_NewWindow ("MainWindow");
	IMUI_Window_SetSize (window, {400, 300});

	scene = Scene_NewScene ();

	lightingdata_t ldata = Light_CreateLightingData (scene);
	Light_EnableSun (ldata);
	// sun light
	Light_AddLight (ldata, {
		{ 1, 1, 1, 500 },
		{ -0.48, -0.64, 0.6, 0 },
		{ 0, -0.8, 0.6, 1 },
		{ 0, 0, 1, 0 },
	}, 0);
	// ambient light
	Light_AddLight (ldata, {
		{ 1, 1, 1, 50 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, 1 },
		{ 0, 0, 1, 0 },
	}, 0);
	Scene_SetLighting (scene, ldata);

	entity_t camera_ent = Scene_CreateEntity (scene);
	camera = Entity_GetTransform (camera_ent);
	Scene_SetCamera (scene, camera_ent);
	newscene (scene);

	show_armature = 1;

	return self;
}

+(MainWindow *) window:(imui_ctx_t)ctx
{
	return [[[MainWindow alloc] initWithContext:ctx] autorelease];
}

-(void)dealloc
{
	free_armature (arm);
	[bonesView release];
	[bones release];
	[clipsView release];
	[clips release];
	[super dealloc];
}

-draw
{
	if (ent && anim && arm && show_armature) {
		qfa_update_anim (anim, frametime * 0.02);
		qfa_get_pose_motors (anim, arm.pose);
		draw_armature (camera, arm, trans);
	}

	imui_style_t style = {};//FIXME qfcc bug
	IMUI_Style_Fetch (IMUI_context, &style);
	UI_Window (window) {
		if (IMUI_Window_IsCollapsed (window)) {
			continue;
		}
		UI_Horizontal {
			UI_Checkbox (&show_armature, "Show Armature");
			UI_FlexibleSpace ();
		}
		UI_Horizontal {
			IMUI_Layout_SetYSize (IMUI_context, imui_size_expand, 100);
			UI_SetFill (style.background.normal);
			[bonesView draw];
			[clipsView draw];
		}
	}
	float s = sin((float)(realtime - double(1ul << 32)));
	float c = cos((float)(realtime - double(1ul << 32)));
	Painter_AddCircle ({300 + 100 * c, 300 + 100 * s}, 20, {0.8, 0.9, 0.1, 1});
	Painter_AddBox ({300, 300}, {25, 5}, 5, {1, .8, 0, 1});
	Painter_AddLine ({300, 300}, {400, 450}, 30 * s * s, {0.8, 0.4, 0.3, 1});
	return self;
}

-setAnim:(animstate_t) anim
{
	self.anim = anim;
	Entity_SetAnimstate (ent, anim);
	return self;
}

-setModel:(model_t) model
{
	self.model = model;

	printf ("model: %p %d\n", model, Model_NumClips (model));
	if (!ent) {
		ent = Scene_CreateEntity (scene);
		trans = Entity_GetTransform (ent);
	}
	Entity_SetModel (ent, model);
	free_armature (arm);
	arm = make_armature (model);
	//Transform_SetLocalRotation (trans, { 0, 0, 1, 0});
	num_clips = Model_NumClips (model);
	clip_num = -1;
	timer = 0;

	//[bones removeAllObjects];
	//for (int i = 0; i < arm.num_joints; i++) {
	//	[bones addObject:[ListItem item:arm.joints[i].name ctx:IMUI_context]];
	//}
	//[bonesView setItems:bones];

	[clips removeAllObjects];
	for (int i = 0; i < num_clips; i++) {
		auto clipinfo = Model_GetClipInfo (model, i);
		[clips addObject:[ListItem item:clipinfo.name ctx:IMUI_context]];
	}
	[clipsView setItems:clips];

	return self;
}

-nextClip:(float)frametime
{
	if (!ent) {
		return self;
	}
	int selected = [clipsView selected];
	if (selected >= 0) {
		clip_num = selected;
	} else {
		timer -= frametime;
		if (timer <= 0) {
			timer = 5;
			clip_num += 1;
			if (clip_num >= num_clips) {
				clip_num = 0;
			}
		}
	}

	auto anim = Entity_GetAnimation (ent);
	anim.frame = clip_num;
	Entity_SetAnimation (ent, anim);
	return self;
}

-(scene_t) scene
{
	return scene;
}

-(transform_t) camera
{
	return camera;
}
@end

void
camera_first_person (transform_t camera, state_t *state)
{
	vector dpos = {};
	dpos.x -= IN_UpdateAxis (cam_move_forward);
	dpos.y -= IN_UpdateAxis (cam_move_side);
	dpos.z -= IN_UpdateAxis (cam_move_up);

	vector drot = {};
	drot.x -= IN_UpdateAxis (cam_move_roll);
	drot.y -= IN_UpdateAxis (cam_move_pitch);
	drot.z -= IN_UpdateAxis (cam_move_yaw);

	dpos *= 0.01;
	drot *= ((float)M_PI / 360);
	state.B = {
		.bvect = (PGA.bvect) drot,
		.bvecp = (PGA.bvecp) dpos,
	};
	//use a constant dt instead of actual frame time because the input
	//values accumulate over time and thus already include frame time.
	float h = 0.02f;
	auto ds = dState (*state);
	state.M += h * ds.M;
	state.B += h * ds.B;
	state.M = normalize (state.M);
}

void
camera_mouse_first_person (transform_t camera, state_t *state)
{
	vector dpos = {};
	vector drot = {};
	drot.y -= IN_UpdateAxis (mouse_y);
	drot.z -= IN_UpdateAxis (mouse_x);

	dpos *= 0.1;
	drot *= ((float)M_PI / 36);
	state.B = {
		.bvect = (PGA.bvect) drot,
		.bvecp = (PGA.bvecp) dpos,
	};
	//use a constant dt instead of actual frame time because the input
	//values accumulate over time and thus already include frame time.
	float h = 0.02f;
	auto ds = dState (*state);
	state.M += h * ds.M;
	state.B += h * ds.B;
	state.M = normalize (state.M);
}

static PGA.vec
trackball_vector (vec2 xy)
{
	// xy is already in -1..1 order of magnitude range (ie, might be a bit
	// over due to screen aspect)
	// This is very similar to blender's trackball calculation (based on it,
	// really)
	float r = 1;
	float t = r * r / 2;
	float d = xy • xy;
	vec3 vec = vec3 (xy, 0);
	if (d < t) {
		// less than 45 degrees around the sphere from the viewer facing
		// pole, so map the mouse point to the sphere
		vec.z = sqrt (r * r - d);
	} else {
		// beyond 45 degrees around the sphere from the veiwer facing
		// pole, so the slope is rapidly approaching infinity or the mouse
		// point may miss the sphere entirely, so instead map the mouse
		// point to the hyperbolic cone pointed towards the viewer. The
		// cone and sphere are tangential at the 45 degree latitude
		vec.z = t / sqrt (d);
	}
	auto v = vec4(vec.zxy, 0);
	auto p = (PGA.vec) v;
	return p;
}

#define min(x,y) ((x) < (y) ? (x) : (y))
static float trackball_sensitivity = 10.0f;
#define sphere_scale 1.0f
void
camera_mouse_trackball (transform_t camera, state_t *state)
{
	vec2 delta = {
		IN_UpdateAxis (mouse_x),
		IN_UpdateAxis (mouse_y),
	};
	int         width = Draw_Width ();
	int         height = Draw_Height ();

	float       size = min (width, height) / 2;
	vec2        center = vec2 (width, height) / 2;
	vec2        m_start = mouse_start - center;
	vec2        m_end = m_start + delta;
	auto        start = trackball_vector (m_start / (size * sphere_scale));
	auto        end = trackball_vector (m_end / (size * sphere_scale));
	bivector_t  drot = (start ∧ end) * 0.5f;
	@algebra(PGA) {
		auto p = e123 + 5 * e032;
#if 1 //FIXME qfcc bug
		state.B = {
			.bvect = ((drot • p) * p).bvect,
			.bvecp = ((drot • p) * p).bvecp,
		};
#else
		state.B = (drot • p) * p;
#endif
	}
	auto ds = dState (*state);
	state.M += ds.M;
	state.B += ds.B;
	state.M = normalize (state.M);
}

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

Array *windows;

void
draw_2d (void)
{
	int         width = Draw_Width ();
	int         height = Draw_Height ();
	Draw_String (8, height - 8,
				 sprintf ("%5.2f\xd0\xd2\xc0\xc2", frametime*1000));

	IMUI_SetSize (imui_ctx, Draw_Width (), Draw_Height ());
	IMUI_BeginFrame (imui_ctx);
	IMUI_Style_Update (imui_ctx, &current_style);

	[windows makeObjectsPerformSelector: @selector (draw)];
	//color_window ();
	IMUI_Draw (imui_ctx);
}

static int
capture_mouse_event (struct IE_event_s *event)
{
	static IE_mouse_event_t prev_mouse;

	if (event.mouse.type == ie_mousedown
		&& ((event.mouse.buttons ^ prev_mouse.buttons) & 2)) {
		IN_UpdateGrab (1);
		mouse_dragging_mmb = true;
		mouse_start = vec2 ( event.mouse.x, event.mouse.y);
	} else if (event.mouse.type == ie_mouseup
			   && ((event.mouse.buttons ^ prev_mouse.buttons) & 2)) {
		IN_UpdateGrab (0);
		mouse_dragging_mmb = false;
	}

	if (event.mouse.type == ie_mousedown
		&& ((event.mouse.buttons ^ prev_mouse.buttons) & 4)) {
		IN_UpdateGrab (1);
		mouse_dragging_rmb = true;
		mouse_start = vec2 ( event.mouse.x, event.mouse.y);
	} else if (event.mouse.type == ie_mouseup
			   && ((event.mouse.buttons ^ prev_mouse.buttons) & 4)) {
		IN_UpdateGrab (0);
		mouse_dragging_rmb = false;
	}

	prev_mouse = event.mouse;
	return !(mouse_dragging_mmb | mouse_dragging_rmb);
}

int
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
		case ie_mouse:
			if ((mouse_dragging_mmb | mouse_dragging_rmb)
				|| !IMUI_ProcessEvent (imui_ctx, event)) {
				return capture_mouse_event (event);
			}
			break;
		default:
			if (!IMUI_ProcessEvent (imui_ctx, event)) {
				return IN_Binding_HandleEvent (event);
			}
	}
	return 0;
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

static AutoreleasePool *autorelease_pool;
static void
arp_start (void)
{
	autorelease_pool = [[AutoreleasePool alloc] init];
}
static void
arp_end (void)
{
	[autorelease_pool release];
	autorelease_pool = nil;
}

int
main (int argc, string *argv)
{
	arp_start ();

	plitem_t *config = PL_GetPropertyList (render_graph_cfg);

	IN_LoadConfig (config);
	init_graphics (config);
	PL_Release (config);

	IN_SendConnectedDevices ();

	setup_bindings ();

	//Draw_SetScale (1);
	imui_ctx = IMUI_NewContext ("Consolas", 22);

	refresh_2d (draw_2d);
	setevents (event_hander, nil);

	int key_devid = IN_FindDeviceId ("core:keyboard");
	int lctrl_key = IN_GetButtonNumber (key_devid, "Control_L");
	int rctrl_key = IN_GetButtonNumber (key_devid, "Control_R");
	int q_key = IN_GetButtonNumber (key_devid, "q");


	state_t camera_state = {
		.M = make_motor ({ -4, 0, 3, 0, }, { 0, 0.316227766, 0, 0.948683298 }),
	};

#if 0
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
#endif

	windows = [[Array array] retain];

	auto main_window = [MainWindow window:imui_ctx];
	[windows addObject:main_window];
	[windows addObject:[FileWindow openFile:"*.r" at:"." ctx:imui_ctx]];
	[windows addObject:[EditWindow openFile:"gizmo.r" ctx:imui_ctx]];

	[main_window setModel:Model_Load ("progs/girl14.iqm")];

	cliphandle_t clips[] = {
		qfa_find_clip ("girl14:rig|standing walk right_RM"),
		qfa_find_clip ("girl14:rig|standing taunt battlecry_RM"),
		qfa_find_clip ("girl14:rig|standing taunt chest thump_RM"),
	};
	int num_clips = countof (clips);
	armhandle_t arma = qfa_find_armature ("girl14");
	animstate_t anim = qfa_create_animation (clips, num_clips, arma, nil);
	for (int i = 0; i < num_clips; i++) {
		qfa_set_clip_loop (anim, i, true);
	}
	qfa_reset_anim (anim);
	[main_window setAnim:anim];

	while (true) {
		arp_end ();
		arp_start ();

		frametime = refresh ([main_window scene]);
		realtime += frametime;

		[main_window nextClip:frametime];

		auto camera = [main_window camera];
		camera_first_person (camera, &camera_state);
		if (mouse_dragging_mmb) {
			camera_mouse_trackball (camera, &camera_state);
		}
		if (mouse_dragging_rmb) {
			camera_mouse_first_person (camera, &camera_state);
		}
		set_transform (camera_state.M, camera, "");

		in_buttoninfo_t info[2] = {};
		IN_GetButtonInfo (key_devid, lctrl_key, &info[0]);
		IN_GetButtonInfo (key_devid, q_key, &info[1]);
		if (info[0].state && info[1].state) {
			break;
		}
	}
	[windows release];
	arp_end ();
	return 0;
}
