#include <AutoreleasePool.h>
#include <PropertyList.h>
#include <msgbuf.h>
#include <QF/keys.h>

#include "gui/editwindow.h"
#include "gui/filewindow.h"
#include "gui/listview.h"
#include "gui/nodepanel.h"
#include "gui/window.h"
#include "armature.h"
#include "camera.h"
#include "gizmo.h"
#include "mainmenu.h"
#include "physics.h"
#include "player.h"
#include "playercam.h"
#include "qwaq-ed.h"

#include "shader/planetary.h"

void traceon() = #0;
void traceoff() = #0;

static string scene_plist =
#embed "config/scene.plist"
;

static string render_graph_cfg =
#embed "config/qwaq-ed-rg.plist"
;

static string input_cfg =
#embed "config/qwaq-ed-in.cfg"
;

static string atmosphere_shader =
#embed "ruamoko/qwaq/shader/atmosphere.r.spv"
;

static string pixpal_shader =
#embed "ruamoko/qwaq/shader/pixpal.r.spv"
;

static string planetary_shader =
#embed "ruamoko/qwaq/shader/planetary.r.spv"
;

static string pbr_brdf_shader =
#embed "ruamoko/qwaq/shader/pbr_brdf.r.spv"
;

static string pbr_conv_shader =
#embed "ruamoko/qwaq/shader/pbr_conv.r.spv"
;

int in_context;
in_axis_t *cam_move_forward;
in_axis_t *cam_move_side;
in_axis_t *cam_move_up;
in_axis_t *cam_move_pitch;
in_axis_t *cam_move_yaw;
in_axis_t *cam_move_roll;
in_button_t *cam_next;
in_button_t *cam_prev;
in_axis_t *mouse_x;
in_axis_t *mouse_y;

in_axis_t *move_forward;
in_axis_t *move_side;
in_axis_t *move_up;
in_axis_t *move_pitch;
in_axis_t *move_yaw;
in_axis_t *move_roll;
in_axis_t *look_forward;
in_axis_t *look_right;
in_axis_t *look_up;
in_button_t *shift;
in_button_t *move_jump;
in_button_t *target_lock;

vec2 mouse_start;
bool mouse_dragging_mmb;
bool mouse_dragging_rmb;

bool draw_editor_overlay = true;
bool quit_editor = false;

void printf (string fmt, ...) = #0;

typedef struct component_s {
	uint size;
	void (*create) (void *comp, uint ent);
	void (*destroy) (void *comp, uint ent);
	void (*rangeid) ();// not supported yet
	string name;
	void *data;
	void (*ui) (void *comp, uint ent);
} component_t;

void init_graphics (plitem_t *config, int num_components,
					component_t *components) = #0;
uint load_resource (string name) = #0;
uint find_resource (string name) = #0;
int res_is_cubemap (uint id) = #0;
float refresh (scene_t scene) = #0;
void refresh_2d (void (func)(void)) = #0;
void setpalette (void *palette, void *colormap) = #0;
void newscene (scene_t scene) = #0;
void set_sky_id (uint texid) = #0;

void setevents (int (func)(struct IE_event_s *, void *), void *data) = #0;
void setctxcbuf (int ctx) = #0;
void addcbuftxt (string txt) = #0;

void Gizmo_AddSphere (vec4 c, float r, vec4 color) = #0;
void Gizmo_AddCapsule (vec4 p1, vec4 p2, float r, vec4 color) = #0;
void Gizmo_AddBrush (vec4 orig, vec4 mins, vec4 maxs,
					 int num_nodes, gizmo_node_t *nodes, vec4 color) = #0;
void Gizmo_AddPlane (vec4 p, vec4 s, vec4 t,
					 vec4 gcol, vec4 scol, vec4 tcol) = #0;

void Painter_AddLine (vec2 p1, vec2 p2, float r, vec4 color) = #0;
void Painter_AddCircle (vec2 c, float r, vec4 color) = #0;
void Painter_AddBox (vec2 c, vec2 e, float r, vec4 color) = #0;
void Painter_AddBezier (vec2 p0, vec2 p1, vec2 p2, vec2 p3, float r,
						vec4 color) = #0;

[no_va_list]
void Render_SetJobBlackboardVar (string job, string name, ...) = #0;
[no_va_list]
void Render_SetBlackboardVar (string name, ...) = #0;
void Render_RunJob (string name) = #0;
void Render_UpdateBuffer (string name, ulong offset, void *data,
						  ulong size) = #0;
ulong Render_BufferAddress (string name) = #0;
ulong Render_BufferOffset (string name) = #0;
ulong Render_BufferSize (string name) = #0;

static component_t qwaq_components[] = {
	[qent_state] = {
		.size = sizeof (state_t),
		.name = "state",
	},
	[qent_body] = {
		.size = sizeof (body_t),
		.name = "body",
	},
	[qent_transform] = {
		.size = sizeof (transform_t),
		.name = "transform",
	},
	[qent_collider] = {
		.size = sizeof (collider_t),
		.name = "collider",
	},
	[qent_grav] = {
		.size = sizeof (bool),
		.name = "grav",
	},
};

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

@class MainMenu;
MainMenu *main_menu;

@interface MainWindow : Window <ListView>
{
	Array *clips;
	ListView *clipsView;
	Array *bones;
	ListView *bonesView;

	scene_t scene;

	Camera *active_camera;
	uint active_camera_index;
	Array  *cameras;

	state_t camera_state;

	int show_armature;

	animstate_t anim;
	animstate_t root_anim;
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
@end

@implementation MainWindow

-initWithContext:(imui_ctx_t)ctx
{
	if (!(self = [super initWithContext:ctx name:"MainWindow"])) {
		return nil;
	}

	clips = [[Array array] retain];
	clipsView = [[ListView list:"MainWindow:clips" ctx:ctx] retain];
	[clipsView setTarget:self];

	bones = [[Array array] retain];
	bonesView = [[ListView list:"MainWindow:bones" ctx:ctx] retain];

	IMUI_Window_SetSize (window, {400, 300});

	scene = Scene_NewScene ();

	lightingdata_t ldata = Light_CreateLightingData (scene);
	Light_EnableSun (ldata);
	// sun light
	Light_AddLight (ldata, {
		{ 1, 1, 1, 500 },
		{ -0.48, -0.64, 0.6, 0 },
		{ 0.48, 0.64, -0.6, -1 },
		{ 0, 0, 1, 0 },
	}, 0);
	// ambient light
	Light_AddLight (ldata, {
		{ 1, 1, 1, 248 },
		{ 0, 0, 0, 0 },
		{ 0, 0, 0, -1 },
		{ 0, 0, 1, 0 },
	}, 0);
	Light_AddLight (ldata, {
		{ 1, 1, 1, 300 },
		{ 0, 4, 4, 1 },
		{ 0, -0.6, -0.8, 0.9848 },
		{ 0, 0, 1, 0 },
	}, 0);
	Light_AddLight (ldata, {
		{99,99,99, 300 },
		{15, 0,10, 1 },
		{ 0, 0, 1,-1 },
		{ 0, 0, 1, 0 },
	}, 0);
	Scene_SetLighting (scene, ldata);

	cameras = [[Array array] retain];

	active_camera_index = [cameras count];
	active_camera = [[Camera inScene:scene] retain];
	[cameras addObject:active_camera];

	camera_state = {
		.M = make_motor ({ -4, 0, 3, 0, }, { 0, 0.316227766, 0, 0.948683298 }),
	};

	IMP imp;
	imp = [self methodForSelector: @selector (nextCamera:)];
	IN_ButtonAddListener (cam_next, imp, self);
	imp = [self methodForSelector: @selector (prevCamera:)];
	IN_ButtonAddListener (cam_prev, imp, self);

	Scene_SetCamera (scene, [active_camera entity]);
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

-addCamera:(Camera *) camera
{
	[cameras addObject:camera];
	return self;
}

-(void) nextCamera:(in_button_t *)button
{
	if (button.state & inb_edge_down) {
		button.state &= inb_down;

		uint old_cam = active_camera_index;
		if (++active_camera_index >= [cameras count]) {
			active_camera_index = 0;
		}
		if (active_camera_index != old_cam) {
			active_camera = [cameras objectAtIndex:active_camera_index];
			Scene_SetCamera (scene, [active_camera entity]);
		}
	}
}

-(void) prevCamera:(in_button_t *)button
{
	if (button.state & inb_edge_down) {
		button.state &= inb_down;

		uint old_cam = active_camera_index;
		if ((int) --active_camera_index < 0) {
			active_camera_index = [cameras count] - 1;
		}
		if (active_camera_index < 0) {
			[self error:"no cameras!"];
		}
		if (active_camera_index != old_cam) {
			active_camera = [cameras objectAtIndex:active_camera_index];
			Scene_SetCamera (scene, [active_camera entity]);
		}
	}
}

-updateCamera
{
	camera_first_person (&camera_state);
	if (mouse_dragging_mmb) {
		camera_mouse_trackball (&camera_state);
	}
	if (mouse_dragging_rmb) {
		camera_mouse_first_person (&camera_state);
	}
	Camera *camera = [cameras objectAtIndex:0];
	[camera setTransformFromMotor:camera_state.M];

	[cameras makeObjectsPerformSelector: @selector(drawExcept:)
							 withObject: active_camera];
	return self;
}

static vec4 axis_points[][2] = {
	{{0, 0, 0, 1}, {1, 0, 0, 1}},
	{{0, 0, 0, 1}, {0, 1, 0, 1}},
	{{0, 0, 0, 1}, {0, 0, 1, 1}},
};

static int axis_colors[] = { 12, 10, 9 };

static gizmo_node_t tetra_brush[] = {
	{ .plane = { 1,-1,-1,-0.5}, .children= {-1, 1} },
	{ .plane = {-1, 1,-1,-0.5}, .children= {-1, 2} },
	{ .plane = { 1, 1, 1,-0.5}, .children= {-1, 3} },
	{ .plane = {-1,-1, 1,-0.5}, .children= {-1,-2} },
};

static gizmo_node_t covered_step[] = {
	{ .plane = {1, 0, 0, 1   }, .children = { 1, -1} },
	{ .plane = {1, 0, 0,-1   }, .children = {-1,  2} },
	{ .plane = {0, 1, 0, 1   }, .children = { 3, -1} },
	{ .plane = {0, 1, 0,-1   }, .children = {-1,  4} },
	{ .plane = {0, 0, 1, 1   }, .children = { 5, -1} },
	{ .plane = {0, 0, 1,-1.5 }, .children = {-1,  6} },

	{ .plane = {0, 0, 1, 0   }, .children = { 7, -2} },
	{ .plane = {0, 0, 1,-0.75}, .children = { 9,  8} },
	{ .plane = {1, 0, 0, 0   }, .children = {-2, -1} },
	{ .plane = {1, 0, 0, 0.5 }, .children = {-1, 10} },
	{ .plane = {0, 0, 1,-1   }, .children = {-2, -1} },
};

-draw
{
	if (![super draw]) {
		return nil;
	}
	if (ent && anim && arm && show_armature) {
		qfa_update_anim (anim, frametime);
		if (root_anim) {
			qfa_update_anim (root_anim, frametime);
			qfa_get_pose_motors (anim, arm.pose);

			auto E = make_motor (Transform_GetWorldPosition (trans),
								 Transform_GetWorldRotation (trans));
			arm_motor_t M;
			qfa_get_pose_motors (root_anim, &M);
			M.m = E * M.m;
			auto cam = Entity_GetTransform ([active_camera entity]);
			draw_armature (cam, arm, M);
			for (int i = 0; i < 3; i++) {
				auto p1 = E * (point_t) axis_points[i][0] * ~E;
				auto p2 = E * (point_t) axis_points[i][1] * ~E;
				draw_3dline (cam, (vec4) p1, (vec4) p2, axis_colors[i]);
			}
		}
	}

	imui_style_t style;
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

	// Frog
	Gizmo_AddSphere ({-2     , -2     , 0.5f }, 0.5f , vec4(0.7, 0.9, 0.4, 0.9));
	Gizmo_AddSphere ({-2+0.3f, -2+0.3f, 0.1f }, 0.1f , vec4(0.7, 0.9, 0.4, 0.9));
	Gizmo_AddSphere ({-2+0.3f, -2-0.3f, 0.1f }, 0.1f , vec4(0.7, 0.9, 0.4, 0.9));
	Gizmo_AddSphere ({-2-0.3f, -2-0.3f, 0.1f }, 0.1f , vec4(0.7, 0.9, 0.4, 0.9));
	Gizmo_AddSphere ({-2-0.3f, -2+0.3f, 0.1f }, 0.1f , vec4(0.7, 0.9, 0.4, 0.9));
	Gizmo_AddSphere ({-2     , -2+0.4f, 0.75f}, 0.2f , vec4(0.7, 0.9, 0.4, 0.9));
	Gizmo_AddSphere ({-2+0.1f, -2+0.5f, 0.9f }, 0.05f, vec4(0.7, 0.9, 0.4, 0.9));
	Gizmo_AddSphere ({-2-0.1f, -2+0.5f, 0.9f }, 0.05f, vec4(0.7, 0.9, 0.4, 0.9));

	Gizmo_AddSphere ({1, 0.5, 3}, 0.5, vec4(0.8, 0.9, 0.1, 0.8));
	Gizmo_AddSphere ({1.5,-0.75, 3}, 1, vec4(0.6, 0.9, 0.5, 0.8));
	Gizmo_AddCapsule ({-1.5,-0.75, 3}, { 1, 1.75, 5.5}, 0.25, vec4(0.9, 0.5, 0.6, 0.8));
	Gizmo_AddCapsule ({-1.5,-0.75, 3}, {-4, 1.75, 5.5}, 0.25, vec4(0.6, 0.5, 0.9, 0.8));
	Gizmo_AddCapsule ({-1.5,-0.75, 3}, { 1,-3.25, 5.5}, 0.25, vec4(0.9, 0.5, 0.9, 0.8));
	Gizmo_AddCapsule ({-1.5,-0.75, 3}, {-4,-3.25, 5.5}, 0.25, vec4(0.6, 0.5, 0.6, 0.8));
	Gizmo_AddCapsule ({-1.5,-0.75, 3}, { 1, 1.75, 0.5}, 0.25, vec4(0.9, 0.5, 0.6, 0.8));
	Gizmo_AddCapsule ({-1.5,-0.75, 3}, {-4, 1.75, 0.5}, 0.25, vec4(0.6, 0.5, 0.9, 0.8));
	Gizmo_AddCapsule ({-1.5,-0.75, 3}, { 1,-3.25, 0.5}, 0.25, vec4(0.9, 0.5, 0.9, 0.8));
	Gizmo_AddCapsule ({-1.5,-0.75, 3}, {-4,-3.25, 0.5}, 0.25, vec4(0.6, 0.5, 0.6, 0.8));

	Gizmo_AddBrush ({-1, 1, 1}, {-0.5,-0.5,-0.5}, {0.5, 0.5, 0.5},
					countof (tetra_brush), tetra_brush,
					vec4(0xba, 0xda, 0x55, 255)/255);
	Gizmo_AddBrush ({-2,-2, 2}, {-1.5,-1.5,-1.5}, {1.5, 1.5, 1.5},
					countof (covered_step), covered_step,
					vec4(0xba, 0xda, 0x55, 255)/255);
	//float s = sin((float)(realtime - double(1ul << 32)));
	//float c = cos((float)(realtime - double(1ul << 32)));
	//Painter_AddCircle ({300 + 100 * c, 300 + 100 * s}, 20, {0.8, 0.9, 0.1, 1});
	//Painter_AddBox ({300, 300}, {25, 5}, 5, {1, .8, 0, 1});
	//Painter_AddLine ({300, 300}, {400, 450}, 30 * s * s, {0.8, 0.4, 0.3, 1});
	//Painter_AddBezier ({300, 300}, {400 + 150 * c, 300 + 150 * s},
	//				   {300 + 100 * (c * c - s * s), 450 + 100 * 2 * c * s},
	//				   {400, 450}, 10 + 10 * s, {0.85, 0.5, 0.77, 1});
	return self;
}

-setAnim:(animstate_t) anim
{
	self.anim = anim;
	Entity_SetAnimstate (ent, anim);
	return self;
}

-setRootAnim:(animstate_t) anim
{
	self.root_anim = anim;
	return self;
}

-setModel:(model_t) model
{
	self.model = model;
	qfa_extract_root_motion (model);

	printf ("model: %p %d\n", model, Model_NumClips (model));
	if (!ent) {
		ent = Scene_CreateEntity (scene);
		trans = Entity_GetTransform (ent);
		Transform_SetLocalPosition (trans, {0.5, 0.5, 0, 1});
		add_target (ent);
	}
	Entity_SetModel (ent, model);
	free_armature (arm);
	arm = make_armature (model);
	//Transform_SetLocalRotation (trans, { 0, 0, 1, 0});
	num_clips = Model_NumClips (model);
	clip_num = -1;
	timer = 0;

	[bones removeAllObjects];
	for (int i = 0; i < arm.num_joints; i++) {
		string name = *(string*)&arm.joints[i].name;//FIXME
		[bones addObject:[ListItem item:name ctx:IMUI_context]];
	}
	[bonesView setItems:bones];

	[clips removeAllObjects];
	for (int i = 0; i < num_clips; i++) {
		auto clipinfo = Model_GetClipInfo (model, i);
		[clips addObject:[ListItem item:clipinfo.name ctx:IMUI_context]];
	}
	[clipsView setItems:clips];

	auto clipinfo = Model_GetClipInfo (model, 0);
	cliphandle_t clips[] = {
		qfa_find_clip ("girl11a:" + clipinfo.name),
	};
	int num_clips = countof (clips);
	armhandle_t arma = qfa_find_armature ("girl11a");
	animstate_t anim = qfa_create_animation (clips, num_clips, arma, nil);
	for (int i = 0; i < num_clips; i++) {
		qfa_set_clip_loop (anim, i, true);
	}
	qfa_reset_anim (anim);
	[self setAnim:anim];

	cliphandle_t root_clips[] = {
		qfa_find_clip ("rm|girl11a:" + clipinfo.name),
	};
	armhandle_t root_arma = qfa_find_armature ("rm|girl11a");
	if (root_arma) {
		animstate_t root_anim = qfa_create_animation (root_clips, num_clips,
													  root_arma, nil);
		for (int i = 0; i < num_clips; i++) {
			qfa_set_clip_loop (root_anim, i, true);
		}
		qfa_reset_anim (root_anim);
		[self setRootAnim:root_anim];
	}

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
	anim.debug_bone = [bonesView selected];
	Entity_SetAnimation (ent, anim);
	return self;
}

-(scene_t) scene
{
	return scene;
}

-(Camera *) camera
{
   return active_camera;
}


-(void)itemSelected:(int) item in:(Array *)items
{
	if (items == clips) {
		string name = [[clips objectAtIndex:item] name];
		name = Model_Name (model) + ":" + name;
		qfa_set_anim_clip (anim, 0, qfa_find_clip (name));
		qfa_set_anim_clip (root_anim, 0, qfa_find_clip ("rm|" + name));
		qfa_set_clip_loop (anim, 0, true);
		qfa_set_clip_loop (root_anim, 0, true);
	}
}

-(void)itemAccepted:(int) item in:(Array *)items
{
	[self itemSelected:item in:items];
}
@end

//FIXME having PGA.group_mask(0xc) here and then providing a defintion causes
//a segfault in qfcc
@generic (genObj = [PGA.group_mask(0xe)]) {

genObj
sqrt (genObj x)
{
	auto a = x + 1;
	return a / sqrt (a • ~a);
}

};

@overload
PGA.group_mask(0xc)
sqrt (PGA.group_mask(0xc) x)
{
	return (x + x.scalar) / 2;
}

//void
motor_t
camera_lookat (point_t eye, point_t target, point_t up)
{
//sqrt( b / a ) = +- b * normalize( b + a )
	@algebra (PGA) {
		point_t eye_0 = e123;
		point_t eye_fwd = e032;
		point_t eye_up = e021;
		auto l0 = (eye_0 ∨ eye_fwd);
		auto p0 = (eye_0 ∨ eye_fwd ∨ eye_up);

		auto l = -(eye ∨ target);
		auto p = (eye ∨ target ∨ up);
		float f = (p • p) / (l•~l);
		if (f < 0.005) {
			// looking (nearly) parallel (or anti-parallel) to the up vector,
			// so fall back (smoothly) to the reference plane
			f = f / 0.005;
			p = f * p + (1 - f) * p0;
		}
		p /= sqrt (p • p);
		l /= sqrt (l • ~l);

		auto T = sqrt(-eye * eye_0);
		auto Tm = l * T * l0 * ~T;
		Tm = normalize (Tm);
		motor_t R;
		if (Tm.scalar < -0.5) {
			// looking backwards along the reference forward direction
			// Rotate 180 around an axis in the reference plane that's
			// perpendicular to the reference forward direction, calculate
			// the rotation to get to that, then undo the 180 degree rotation
			auto A = ((⋆(p0 * e0123) ∧ ⋆(l0 * e0)) • eye) * eye;
			if ((A • Tm).scalar < 0) {
				A = ~A;
			}
			Tm = A * l * ~A * T * l0 * ~T;
			Tm = normalize (Tm);
			R = ~A * sqrt(Tm) * T;
		} else {
			R = sqrt(Tm) * T;
		}
		//FIXME scalar+bvect isn't accepted by full motors for normalize
		motor_t pp = p * R * p0 * ~R;
		auto Rm = normalize (pp);
		motor_t L;
		if (Rm.scalar < -0.5) {
			// The target plane is "almost" anti-parallel to the reference
			// plane, so rotate it 180 around the target line, calculate the
			// needed rotation, then undo the 180 degree rotation
			p = l * p * ~l;
			pp = p * R * p0 * ~R;
			Rm = normalize (pp);
			L = ~l * sqrt(Rm) * R;
		} else {
			L = sqrt(Rm) * R;
		}
		return normalize (L);
	}
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
			imui_style_t style;
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

void
draw_2d (void)
{
	int         width = Draw_Width ();
	int         height = Draw_Height ();
	//\xd0\xd2\xc0\xc2
	Draw_String (8, height - 8, sprintf ("%5.2f", frametime*1000));

	if (draw_editor_overlay) {
		IMUI_SetSize (imui_ctx, Draw_Width (), Draw_Height ());
		IMUI_BeginFrame (imui_ctx);
		IMUI_Style_Update (imui_ctx, &current_style);

		[main_menu draw];
		[Window drawWindows];
		//color_window ();
		IMUI_Draw (imui_ctx);
	}
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
			if (event.type == ie_key && event.key.code == QFK_ESCAPE
				&& (event.key.shift & ies_control)) {
				addcbuftxt ("toggleconsole\n");
				return 1;
			}
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

	cam_move_forward = IN_CreateAxis ("cam.move.forward", "Camera Fore/Aft");
	cam_move_side = IN_CreateAxis ("cam.move.side", "Camera Left/Right");
	cam_move_up = IN_CreateAxis ("cam.move.up", "Camera Up/Down");
	cam_move_pitch = IN_CreateAxis ("cam.move.pitch", "Camera Pitch");
	cam_move_yaw = IN_CreateAxis ("cam.move.yaw", "Camera Yaw");
	cam_move_roll = IN_CreateAxis ("cam.move.roll", "Camera Roll");
	cam_next = IN_CreateButton ("cam.next", "Camera Next");
	cam_prev = IN_CreateButton ("cam.prev", "Camera Prev");
	mouse_x = IN_CreateAxis ("mouse.x", "Mouse X");
	mouse_y = IN_CreateAxis ("mouse.y", "Mouse Y");

	move_forward = IN_CreateAxis ("move.forward", "Player Move Fore/Aft");
	move_side = IN_CreateAxis ("move.side", "Player Move Left/Right");
	move_up = IN_CreateAxis ("move.up", "Player Move Up/Down");
	move_pitch = IN_CreateAxis ("move.pitch", "Player Pitch");
	move_yaw = IN_CreateAxis ("move.yaw", "Player Yaw");
	move_roll = IN_CreateAxis ("move.roll", "Player Roll");
	shift = IN_CreateButton ("shift", "Player shift");
	move_jump = IN_CreateButton ("move.jump", "Player Jump");
	look_forward = IN_CreateAxis ("look.forward", "Player Look Forward");
	look_right = IN_CreateAxis ("look.right", "Player Look Right");
	look_up = IN_CreateAxis ("look.up", "Player Look Up");
	target_lock = IN_CreateButton ("target.lock", "Player Target Lock");

	plitem_t *config = PL_GetPropertyList (input_cfg);
	IN_LoadConfig (config);
	PL_Release (config);
}

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


void
draw_quadsphere_gizmos (int key_devid, int bspace, int subdiv,
						msgbuf_t quadsphere, qf_mesh_t qsmesh, mat4 mat)
{
	static uint base = 0;
	static double inc_time = -1;
	static double key_time = -1;
	in_buttoninfo_t info = {};
	IN_GetButtonInfo (key_devid, bspace, &info);
	if (realtime > key_time && info.state) {
		key_time = realtime + 0.2;
		base += 4 * (1 << (2 * subdiv));
	}
	if (0&&realtime > inc_time) {
		inc_time = realtime + 3;
		base += 4 * (1 << (2 * subdiv));
	}
	if (base >= qsmesh.adjacency.count) {
		base = 0;
	}
	//float len = 0;
	uint count = 0;
	if (subdiv > 0) {
		count = 4u * (1 << (2 * subdiv));
	} else {
		count = qsmesh.adjacency.count;
	}
	for (uint i = 0; i < count; i++) {
		int adjacency = qsmesh.adjacency.offset;
		int verts = qsmesh.vertices.offset;
		int offset = adjacency + (i + base) * sizeof (quarteredge_t);
		quarteredge_t h[2];
		MsgBuf_ReadSeek (quadsphere, offset, msg_set);
		MsgBuf_ReadBytes (quadsphere, &h[0], sizeof (quarteredge_t));
		//printf ("%d: %d %d %d\n", i + base, h[0].twin, h[0].vert, h[0].edge);
//#define NEXT(i) h[0].next
#define NEXT(i) (((i) & ~3) | (((i)+1) & 3))
		offset = adjacency + NEXT(i + base) * sizeof (quarteredge_t);
		MsgBuf_ReadSeek (quadsphere, offset, msg_set);
		MsgBuf_ReadBytes (quadsphere, &h[1], sizeof (quarteredge_t));
		vec4 v[2] = {'0 0 0 1', '0 0 0 1'};
		vec4 n = {};
		offset = verts + h[0].vert * sizeof (vec3) * 2;
		MsgBuf_ReadSeek (quadsphere, offset, msg_set);
		MsgBuf_ReadBytes (quadsphere, &v[0], sizeof (vec3));
		MsgBuf_ReadBytes (quadsphere, &n, sizeof (vec3));
		offset = verts + h[1].vert * sizeof (vec3) * 2;
		MsgBuf_ReadSeek (quadsphere, offset, msg_set);
		MsgBuf_ReadBytes (quadsphere, &v[1], sizeof (vec3));
		v[0] = mat * v[0];
		v[1] = mat * v[1];
		//printf ("%d %q %q\n", i, v[0], v[1]);
		Gizmo_AddCapsule (v[0], v[1], 3e3,//0.005,// { 1, 0, 0, 0});
						  vec4 (i & 1, (i>>1)&1, (i>>2)&1, -1)*0.5+0.5);
		//auto d = v[1] - v[0];
		//float l = d • d;
		//if (l > len) len = l;
	}
	//printf ("max len: %g\n", sqrt(len));
}

bool
check_keys (int key_devid, int lctrl_key, int lalt_key, int q_key, int e_key)
{
	static bool editor_key_pressed = false;

	in_buttoninfo_t info[4] = {};
	IN_GetButtonInfo (key_devid, lctrl_key, &info[0]);
	IN_GetButtonInfo (key_devid, lalt_key, &info[1]);
	IN_GetButtonInfo (key_devid, q_key, &info[2]);
	IN_GetButtonInfo (key_devid, e_key, &info[3]);
	if (info[0].state && info[2].state) {
		return true;
	}
	if (info[0].state && info[1].state && info[3].state) {
		if (!editor_key_pressed) {
			draw_editor_overlay = !draw_editor_overlay;
		}
		editor_key_pressed = true;
	} else {
		editor_key_pressed = false;
	}
	return false;
}

@interface EntityInit : Object
{
@public
	string name;
	string model;
	string mesh;
	string entqueue;
	string texture;
	bool mesh_flag;
	bool grav_flag;
	uint submesh_mask;
	vec4 mesh_param;
	vec4 position;
	quaternion rotation;
	vec4 scale;
	bool target;
	float invDensity;
	bool from_shape;
	vec3 vel;
	vec3 avel;

	struct {
		vec4        plane;
		vec3        offset;
		float       radius;
		vec3        axis;
		col_type_t  type;
	} collider;
}
@end

@implementation EntityInit : Object
-(void)setDefaultIvars
{
	position = '0 0 0 1';
	rotation = '0 0 0 1';
	scale = '1 1 1 1';
}
@end

void
load_scene (plitem_t *scene_item, scene_t scene)
{
	id scene_plist = [[PLItem fromItem:scene_item] retain];
	int count = [scene_plist count];
	for (int i = 0; i < count; i++) {
		id ent_item = [scene_plist getObjectAtIndex:i];
		EntityInit *ent_init = [EntityInit fromPropertyList:[ent_item item]];
		printf ("name: %s\n", ent_init.name);
		printf ("model: %s\n", ent_init.model);
		printf ("position: %q\n", ent_init.position);
		printf ("rotation: %q\n", ent_init.rotation);
		printf ("scale: %q\n", ent_init.scale);
		printf ("target: %s\n", ent_init.target ? "true" : "false");
		printf ("invDensity: %g\n", ent_init.invDensity);

		entity_t ent = Scene_CreateEntity (scene);
		transform_t xform = Entity_GetTransform (ent);
		Transform_SetLocalTransform (xform, ent_init.scale,
									 ent_init.rotation, ent_init.position);
		msgbuf_t mesh = nil;
		model_t model = nil;
		uint entqueue = Scene_Entqueue (scene, ent_init.entqueue);
		uint texture = find_resource (ent_init.texture);
		switch (ent_init.mesh) {
		case "create_quadsphere":
			mesh = create_quadsphere (ent_init.mesh_flag);
			break;
		case "create_ico":
			mesh = create_ico ();
			break;
		case "create_block":
			mesh = create_block (ent_init.mesh_param.xyz);
			break;
		default:
			if (ent_init.model) {
				model = Model_Load (ent_init.model);
			}
			break;
		}
		bool have_collider = false;
		collider_t collider = {
			.type = ent_init.collider.type,
		};
		switch (ent_init.collider.type) {
		case col_plane:
			auto plane = (plane_t) ent_init.collider.plane;
			// if the normal is 0, then no collider was specified
			if (plane • plane) {
				have_collider = true;
				collider.plane = plane;
				// force infinite mass
				ent_init.invDensity = 0;
			}
			break;
		case col_ball:
			have_collider = true;
			collider.ball.offset = ent_init.collider.offset;
			collider.ball.radius = ent_init.collider.radius;
			break;
		case col_capsule:
			have_collider = true;
			collider.capsule.offset = ent_init.collider.offset;
			collider.capsule.radius = ent_init.collider.radius;
			collider.capsule.axis = ent_init.collider.axis;
			break;
		}
		uint e = ~0u;//FIXME
		if (mesh || have_collider) {
			e = new_entity ();
			set_component (e, qent_transform, &xform);
		}
		if (have_collider) {
			max_collider_ents++;
			set_component (e, qent_collider, &collider);
			body_t body = {};//infinite mass
			body.R = 1;
			if (ent_init.from_shape) {
				switch (collider.type) {
				case col_plane:
					body = calc_inertia_plane (collider, ent_init.invDensity);
					break;
				case col_ball:
					body = calc_inertia_ball (collider, ent_init.invDensity);
					break;
				case col_capsule:
					body = calc_inertia_capsule (collider, ent_init.invDensity);
					break;
				}
			} else if (mesh) {
				// create body data from mesh
				body = calc_inertia_tensor (mesh, ent_init.invDensity);
			} else {
				//FIXME build from model?
			}
			set_component (e, qent_body, &body);
			state_t state = {
				.M = make_motor (ent_init.position, ent_init.rotation),
				.B = (PGA.bvect) ent_init.avel + (PGA.bvecp) ent_init.vel,
			};
			state.M = state.M * ~body.R;
			state.B = body.R * state.B * ~body.R;
			set_component (e, qent_state, &state);
			set_update (e, update_physics);
		}
		if (e != ~0u && ent_init.grav_flag) {
			set_component (e, qent_grav, &ent_init.grav_flag);
		}
		if (mesh) {
			model = Model_LoadMesh (ent_init.name, mesh);
			MsgBuf_Delete (mesh);
		}
		if (model) {
			Entity_SetModel (ent, model);
			Entity_SetSubmeshMask (ent, ent_init.submesh_mask);
			if (entqueue != ~0u) {
				Entity_SetEntqueue (ent, entqueue);
			}
			if (texture != ~0u) {
				Entity_SetTextureID (ent, texture);
			}
		}
		if (ent_init.target) {
			add_target (ent);
		}

		arp_end ();
		arp_start ();
	}

	collider_ents = obj_malloc (sizeof (uint) * max_collider_ents);
}

void
create_pbr_stuff (uint skyid)
{
	Render_RunJob ("pbr_brdf");
	Render_SetBlackboardVar ("sampleCount", 1024);
	Render_SetBlackboardVar ("conv_size", uvec2(512,512));
	if (res_is_cubemap (skyid)) {
		uint defenv = find_resource ("default_magenta");
		Render_SetBlackboardVar ("control", 1);
		Render_SetJobBlackboardVar ("pbr_conv", "pbr_cube_id", skyid);
		Render_SetJobBlackboardVar ("pbr_conv", "pbr_env_id", defenv);
	} else {
		uint defcube = find_resource ("default_magenta_cube");
		Render_SetBlackboardVar ("control", 0);
		Render_SetJobBlackboardVar ("pbr_conv", "pbr_cube_id", defcube);
		Render_SetJobBlackboardVar ("pbr_conv", "pbr_env_id", skyid);
	}
	Render_RunJob ("pbr_conv");

	uint tex_ibl = find_resource ("diff_cube");
	uint tex_lut = find_resource ("brdf");
	printf ("tex_ibl: %08x tex_lut: %08x\n", tex_ibl, tex_lut);
	Render_SetJobBlackboardVar ("main", "pbr_irradiance", tex_ibl);
	Render_SetJobBlackboardVar ("main", "pbr_brdf_lut", tex_lut);
}

int
main (int argc, string *argv)
{
	float early_exit = 0;
	if (argc > 1) {
		early_exit = strtof (argv[1], nil);
	}

	arp_start ();

	plitem_t *config = PL_GetPropertyList (render_graph_cfg);
	if (!config) {
		return 1;
	}
	init_graphics (config, qent_comp_count, qwaq_components);
	PL_Release (config);

	//ImphenziaPixPal
	uint pixpal = load_resource ("pixpal.meta");
	uint skyid = load_resource ("eso0932a.meta");

	create_pbr_stuff (skyid);

	IN_SendConnectedDevices ();
	setup_bindings ();

	//Draw_SetScale (1);
	//FIXME need a way to specify font sets (and use them!) because finding
	//nice fonts with all the desired symbols can be quite a chore.
	imui_ctx = IMUI_NewContext ("FreeMono", 22);

	refresh_2d (draw_2d);
	setevents (event_hander, nil);

	int key_devid = IN_FindDeviceId ("core:keyboard");
	int lctrl_key = IN_GetButtonNumber (key_devid, "Control_L");
	int rctrl_key = IN_GetButtonNumber (key_devid, "Control_R");
	int lalt_key = IN_GetButtonNumber (key_devid, "Alt_L");
	int ralt_key = IN_GetButtonNumber (key_devid, "Alt_R");
	int q_key = IN_GetButtonNumber (key_devid, "q");
	int e_key = IN_GetButtonNumber (key_devid, "e");
	int bspace = IN_GetButtonNumber (key_devid, "BackSpace");

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

	main_menu = [[MainMenu menu:imui_ctx] retain];

	arp_end ();
	arp_start ();

	auto main_window = [[MainWindow window:imui_ctx] retain];
	set_sky_id (skyid);

	[main_window setModel:Model_Load ("progs/girl11a.iqm")];

	load_scene (PL_GetPropertyList (scene_plist), [main_window scene]);

	id player = [[Player player:[main_window scene]] retain];
	id playercam = [[PlayerCam inScene:[main_window scene]] retain];
	[player setCamera:playercam];
	[main_window addCamera:playercam];

	int planetary_queue = Scene_Entqueue ([main_window scene], "planetary");
	int pixpal_queue = Scene_Entqueue ([main_window scene], "pixpal");

	auto earth_ent = create_orrery (planetary_queue, [main_window scene]);

	//create_cube ();
	while (true) {
		num_collider_ents = 0;
		arp_end ();
		arp_start ();

		frametime = refresh ([main_window scene]);
		realtime += frametime;
		if (early_exit) {
			if (realtime > early_exit + double (1ul<<32)) {
				break;
			}
		}
		if (0) {
			static bool fish = false;
			if (realtime > (10 + double (1ul<<32)) && !fish) {
				Cvar_SetInteger ("fisheye", true);
				fish = true;
			}
		}

		update_orrery (earth_ent, realtime);

		//update_cube(frametime);
		//draw_cube();

		[player think:frametime];
		[playercam think:frametime];
		[main_window nextClip:frametime];
		[main_window updateCamera];

		//if (0) {
		//	auto xform = Entity_GetTransform (QuadSphere_ent);
		//	mat4 mat = Transform_GetWorldMatrix(xform);
		//	draw_quadsphere_gizmos (key_devid, bspace, SUBDIV, quadsphere,
		//							qsmesh, mat);
		//}

		leafnode ();

		if (quit_editor ||
			check_keys (key_devid, lctrl_key, lalt_key, q_key, e_key)) {
			break;
		}
	}
	[player release];
	[Window shutdown];
	arp_end ();
	return 0;
}
