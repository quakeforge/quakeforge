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
#include <QF/keys.h>

#include "gui/editwindow.h"
#include "gui/filewindow.h"
#include "gui/listview.h"
#include "armature.h"
#include "camera.h"
#include "gizmo.h"
#include "pga3d.h"
#include "player.h"
#include "playercam.h"

void traceon() = #0;
void traceoff() = #0;

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

void camera_first_person (state_t *camera_state);
void camera_mouse_trackball (state_t *camera_state);
void camera_mouse_first_person (state_t *camera_state);

void printf (string fmt, ...) = #0;

void init_graphics (plitem_t *config) = #0;
float refresh (scene_t scene) = #0;
void refresh_2d (void (func)(void)) = #0;
void setpalette (void *palette, void *colormap) = #0;
void newscene (scene_t scene) = #0;
void setevents (int (func)(struct IE_event_s *, void *), void *data) = #0;
void setctxcbuf (int ctx) = #0;
void addcbuftxt (string txt) = #0;

void Gizmo_AddSphere (vec4 c, float r, vec4 color) = #0;
void Gizmo_AddCapsule (vec4 p1, vec4 p2, float r, vec4 color) = #0;
void Gizmo_AddBrush (vec4 orig, vec4 mins, vec4 maxs,
					 int num_nodes, gizmo_node_t *nodes, vec4 color) = #0;

void Painter_AddLine (vec2 p1, vec2 p2, float r, vec4 color) = #0;
void Painter_AddCircle (vec2 c, float r, vec4 color) = #0;
void Painter_AddBox (vec2 c, vec2 e, float r, vec4 color) = #0;
void Painter_AddBezier (vec2 p0, vec2 p1, vec2 p2, vec2 p3, float r,
						vec4 color) = #0;

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

Array *windows;

@interface MainWindow : Object <FileWindow, ListView>
{
	Array *clips;
	ListView *clipsView;
	Array *bones;
	ListView *bonesView;

	imui_ctx_t IMUI_context;
	imui_window_t *window;

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
	if (!(self = [super init])) {
		return nil;
	}
	IMUI_context = ctx;

	clips = [[Array array] retain];
	clipsView = [[ListView list:"MainWindow:clips" ctx:ctx] retain];
	[clipsView setTarget:self];

	bones = [[Array array] retain];
	bonesView = [[ListView list:"MainWindow:bones" ctx:ctx] retain];

	window = IMUI_NewWindow ("MainWindow");
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
		{ 1, 1, 1, 50 },
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

-(void)openFile:(string)path forSave:(bool)forSave
{
	if (forSave) {
	} else {
		[windows addObject:[EditWindow openFile:path ctx:imui_ctx]];
	}
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

	[cameras makeObjectsPerformSelector: @selector(draw)];
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
	if (ent && anim && arm && show_armature) {
		qfa_update_anim (anim, frametime);
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
		qfa_find_clip ("girl14:" + clipinfo.name),
	};
	int num_clips = countof (clips);
	armhandle_t arma = qfa_find_armature ("girl14");
	animstate_t anim = qfa_create_animation (clips, num_clips, arma, nil);
	for (int i = 0; i < num_clips; i++) {
		qfa_set_clip_loop (anim, i, true);
	}
	qfa_reset_anim (anim);
	[self setAnim:anim];

	cliphandle_t root_clips[] = {
		qfa_find_clip ("rm|girl14:" + clipinfo.name),
	};
	armhandle_t root_arma = qfa_find_armature ("rm|girl14");
	animstate_t root_anim = qfa_create_animation (root_clips, num_clips,
												  root_arma, nil);
	for (int i = 0; i < num_clips; i++) {
		qfa_set_clip_loop (root_anim, i, true);
	}
	qfa_reset_anim (root_anim);
	[self setRootAnim:root_anim];

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

@interface CamTest : Object
{
	entity_t ent;
	transform_t xform;
}
+camtest:(scene_t) scene;
-think:(float)frametime state:(state_t)state;
@end

@implementation CamTest
-initInScene:(scene_t) scene
{
	if (!(self = [super init])) {
		return nil;
	}

	ent = Scene_CreateEntity (scene);
	auto main = Scene_CreateEntity (scene);
	auto fwd = Scene_CreateEntity (scene);
	auto up = Scene_CreateEntity (scene);
	printf ("%08lx %08lx %08lx %08lx\n", ent, main, fwd, up);

	Entity_SetModel (main, Model_Load ("progs/Octahedron.mdl"));
	Entity_SetModel (fwd, Model_Load ("progs/Cube.mdl"));
	Entity_SetModel (up, Model_Load ("progs/Capsule.mdl"));

	xform = Entity_GetTransform (ent);
	auto main_xform = Entity_GetTransform (main);
	auto fwd_xform = Entity_GetTransform (fwd);
	auto up_xform = Entity_GetTransform (up);
	Transform_SetParent (main_xform, xform);
	Transform_SetParent (fwd_xform, main_xform);
	Transform_SetParent (up_xform, main_xform);
	Transform_SetLocalPosition(xform, {0, 0, 0, 1});
	Transform_SetLocalPosition(main_xform, {0, 0, 0, 1});
	Transform_SetLocalPosition(fwd_xform, {0.5, 0, 0, 1});
	Transform_SetLocalScale(fwd_xform, {0.125, 0.125, 0.125, 1});
	Transform_SetLocalPosition(up_xform, {0, 0, 0.5, 1});
	Transform_SetLocalScale(up_xform, {0.125, 0.125, 0.125, 1});

	return self;
}

-(void)dealloc
{
	Scene_DestroyEntity (ent);
	[super dealloc];
}

+camtest:(scene_t) scene
{
	return [[[self alloc] initInScene:scene] autorelease];
}

-think:(float)frametime state:(state_t)state
{
	set_transform (state.M, xform);
	return self;
}
@end

void
camera_first_person (state_t *state)
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
camera_mouse_first_person (state_t *state)
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
camera_mouse_trackball (state_t *state)
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
		state.B = (drot • p) * p;
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

msgbuf_t create_ico();
msgbuf_t create_block();
msgbuf_t create_quadsphere();
body_t calc_inertia_tensor (msgbuf_t model_buf);
void leafnode();

static void
draw_principle_axes (motor_t M, bivector_t I)
{
	@algebra(PGA) {
		auto o = M * e123 * ~M;
		auto p1 = M * (e123 + e032 * I.bvecp[0]) * ~M;
		auto p2 = M * (e123 + e013 * I.bvecp[1]) * ~M;
		auto p3 = M * (e123 + e021 * I.bvecp[2]) * ~M;
		Gizmo_AddCapsule ((vec4) o, (vec4) p1, 0.25, {1, 0, 0, 0.5});
		Gizmo_AddCapsule ((vec4) o, (vec4) p2, 0.25, {0, 1, 0, 0.5});
		Gizmo_AddCapsule ((vec4) o, (vec4) p3, 0.25, {0, 0, 1, 0.5});
	}
}

int
main (int argc, string *argv)
{
	bool early_exit = argc > 1 && strtof (argv[1], nil);

	if (early_exit) {
		realtime = -1;
	}

	arp_start ();

	plitem_t *config = PL_GetPropertyList (render_graph_cfg);

	IN_LoadConfig (config);
	init_graphics (config);
	PL_Release (config);

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
	int q_key = IN_GetButtonNumber (key_devid, "q");
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

	windows = [[Array array] retain];

	auto main_window = [MainWindow window:imui_ctx];
	auto file_window = [FileWindow openFile:"*.r" at:"." ctx:imui_ctx];
	[file_window setTarget:main_window];
	[windows addObject:main_window];
	[windows addObject:file_window];

	[main_window setModel:Model_Load ("progs/girl14.iqm")];

	entity_t nh_ent = Scene_CreateEntity ([main_window scene]);
	entity_t Capsule_ent = Scene_CreateEntity ([main_window scene]);
	entity_t Cube_ent = Scene_CreateEntity ([main_window scene]);
	entity_t Octahedron_ent = Scene_CreateEntity ([main_window scene]);
	entity_t Plane_ent = Scene_CreateEntity ([main_window scene]);
	entity_t Tetrahedron_ent = Scene_CreateEntity ([main_window scene]);
	entity_t Icosahedron_ent = Scene_CreateEntity ([main_window scene]);
	entity_t block_ent = Scene_CreateEntity ([main_window scene]);

	add_target (nh_ent);
	add_target (Capsule_ent);
	add_target (Cube_ent);
	add_target (Octahedron_ent);
	add_target (Tetrahedron_ent);
	add_target (Icosahedron_ent);
	add_target (block_ent);

	printf ("Night Heron: %lx\n", nh_ent);
	printf ("Capsule: %lx\n", Capsule_ent);
	printf ("Cube: %lx\n", Cube_ent);
	printf ("Octahedron: %lx\n", Octahedron_ent);
	printf ("Plane: %lx\n", Plane_ent);
	printf ("Tetrahedron: %lx\n", Tetrahedron_ent);

	Entity_SetModel (nh_ent, Model_Load ("night_heron.iqm"));
	Entity_SetModel (Capsule_ent, Model_Load ("progs/Capsule.mdl"));
	Entity_SetModel (Cube_ent, Model_Load ("progs/Cube.mdl"));
	Entity_SetModel (Octahedron_ent, Model_Load ("progs/Octahedron.mdl"));
	Entity_SetModel (Plane_ent, Model_Load ("progs/Plane.mdl"));
	Entity_SetModel (Tetrahedron_ent, Model_Load ("progs/Tetrahedron.mdl"));
	auto ico_mesh = create_ico ();
	auto block_mesh = create_block ();
	auto block_body = calc_inertia_tensor (block_mesh);
	printf ("R:[%g %v %v %g] I:[%v %v] Ii:[%v %v]\n",
			block_body.R.scalar, block_body.R.bvect,
			block_body.R.bvecp,  block_body.R.qvec,
			block_body.I.bvect,  block_body.I.bvecp,
			block_body.Ii.bvect, block_body.Ii.bvecp);
	Entity_SetModel (Icosahedron_ent, Model_LoadMesh ("ico", ico_mesh));
	Entity_SetModel (block_ent, Model_LoadMesh ("block", block_mesh));

	Transform_SetLocalPosition(Entity_GetTransform (nh_ent), {15, 0, 10, 1});
	Transform_SetLocalRotation(Entity_GetTransform (nh_ent), {0.5, -0.5, 0.5, 0.5});
	Transform_SetLocalPosition(Entity_GetTransform (Capsule_ent), {5, 2, 1, 1});
	Transform_SetLocalPosition(Entity_GetTransform (Cube_ent), {5, -2, 0.5, 1});
	Transform_SetLocalPosition(Entity_GetTransform (Octahedron_ent), {0, -2, 0.5, 1});
	Transform_SetLocalPosition(Entity_GetTransform (Tetrahedron_ent), {0, 2, 0.5, 1});
	Transform_SetLocalPosition(Entity_GetTransform (Icosahedron_ent), {-2, 2, 0.5, 1});
	Transform_SetLocalScale (Entity_GetTransform (Plane_ent), {25, 25, 25, 1});

	id player = [[Player player:[main_window scene]] retain];
	id playercam = [[PlayerCam inScene:[main_window scene]] retain];
	[player setCamera:playercam];
	[main_window addCamera:playercam];

	//id camtest = [[CamTest camtest:[main_window scene]] retain];

	#define SUBDIV 1
	auto quadsphere = create_quadsphere();
	entity_t QuadSphere_ent = Scene_CreateEntity ([main_window scene]);
	add_target (QuadSphere_ent);
	Entity_SetModel (QuadSphere_ent, Model_LoadMesh ("quadsphere", quadsphere));
	Entity_SetSubmeshMask (QuadSphere_ent, ~(1<<8));
	Transform_SetLocalPosition(Entity_GetTransform (QuadSphere_ent), { 6470e3, -20, 20, 1});
	Transform_SetLocalScale(Entity_GetTransform (QuadSphere_ent), { 6370e3, 6370e3, 6370e3, 1});
	qf_mesh_t qsmesh;
	vec4 stuff = {};
	{
		qf_model_t model;
		vec4 stuff = {};
		MsgBuf_ReadBytes (quadsphere, &model, sizeof (model));
		int offset = model.meshes.offset + sizeof(qsmesh) * SUBDIV;
		MsgBuf_ReadSeek (quadsphere, offset, msg_set);
		MsgBuf_ReadBytes (quadsphere, &qsmesh, sizeof (qsmesh));
		qsmesh.adjacency.offset += offset;
		qsmesh.vertices.offset += offset;
	}
	//create_cube ();
	state_t block_state = {
		.M = make_motor ({-20, 20, 5, 1}, {0, 0, 0, 1}),
		.B.bvect = (PGA.bvect)'0 0.0005 1',
	};
	printf ("block_state M:[%g %v %v %g] B:[%v %v]\n",
			block_state.M.scalar, block_state.M.bvect,
			block_state.M.bvecp,  block_state.M.qvec,
			block_state.B.bvect,  block_state.B.bvecp);
	auto block_xform = Entity_GetTransform (block_ent);
	while (true) {
		arp_end ();
		arp_start ();

		frametime = refresh ([main_window scene]);
		if (early_exit) {
			if (realtime < 0) {
				realtime = 0;
			} else {
				realtime += frametime;
			}
			if (realtime > 1) {
				break;
			}
		} else {
		    realtime += frametime;
		}

		//update_cube(frametime);
		//draw_cube();
		auto bs = block_state;
		bs.M = block_body.R * bs.M;
		bs.B = block_body.R * bs.B * ~block_body.R;

		float h = frametime / 100;
		bivector_t f = {
			.bvect = '0 0 0.1',
			.bvecp = '0 0 1',
		};
		for (int i = 0; i < 100; i++) {
			auto ds = dState (bs, f, &block_body);
			bs.M += h * ds.M;
			bs.B += h * ds.B;
			bs.M = normalize (bs.M);
		}
		bs.M = bs.M * ~block_body.R;
		bs.B = ~block_body.R * bs.B * block_body.R;
		block_state = bs;
		set_transform (block_state.M, block_xform);
		draw_principle_axes (block_state.M, block_body.I);

		[player think:frametime];
		[playercam think:frametime];
		//[camtest think:frametime state:[playercam state]];
		[main_window nextClip:frametime];

		[main_window updateCamera];
		{
			static uint base = 0;
			static double inc_time = -1;
			static double key_time = -1;
			in_buttoninfo_t info = {};
			IN_GetButtonInfo (key_devid, bspace, &info);
			if (realtime > key_time && info.state) {
				key_time = realtime + 0.2;
				base += 4 * (1 << (2 * SUBDIV));
			}
			if (0&&realtime > inc_time) {
				inc_time = realtime + 3;
				base += 4 * (1 << (2 * SUBDIV));
			}
			if (base >= qsmesh.adjacency.count) {
				base = 0;
			}
			//for (uint i = 0; i < 4u * (1 << (2 * SUBDIV)); i++) {
			for (uint i = 0; i < qsmesh.adjacency.count; i++) {
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
				vec4 v[2] = {};
				vec4 n = {};
				offset = verts + h[0].vert * sizeof (vec3) * 2;
				MsgBuf_ReadSeek (quadsphere, offset, msg_set);
				MsgBuf_ReadBytes (quadsphere, &v[0], sizeof (vec3));
				MsgBuf_ReadBytes (quadsphere, &n, sizeof (vec3));
				offset = verts + h[1].vert * sizeof (vec3) * 2;
				MsgBuf_ReadSeek (quadsphere, offset, msg_set);
				MsgBuf_ReadBytes (quadsphere, &v[1], sizeof (vec3));
				v[0] += '-20 20 0 0';
				v[1] += '-20 20 0 0';
				Gizmo_AddCapsule (v[0], v[1], 0.005,// { 1, 0, 0, 0});
								  vec4 (i & 1, (i>>1)&1, (i>>2)&1, -1)*0.5+0.5);
			}
		}
		leafnode ();
		//set_transform ([playercam state].M, camera);
		//{
		//	auto p = (vec4)[player pos];
		//	auto c = Transform_GetWorldPosition (camera);
		//	auto n = (vec4)[playercam getNest];
		//	auto d = c - p;
		//	printf ("n:%9q\n", n);
		//	printf ("c:%9q d:%g x+y:%g\n", c, sqrt(d•d), c.x+c.y);
		//}

		in_buttoninfo_t info[2] = {};
		IN_GetButtonInfo (key_devid, lctrl_key, &info[0]);
		IN_GetButtonInfo (key_devid, q_key, &info[1]);
		if (info[0].state && info[1].state) {
			break;
		}
	}
	[player release];
	[windows release];
	arp_end ();
	return 0;
}
