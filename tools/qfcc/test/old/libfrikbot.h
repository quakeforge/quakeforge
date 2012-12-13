#include "Entity.h"

@class Bot;
@class Waypoint;

struct bot_data_t = {
	string name;
	float pants, shirt;
};
typedef struct bot_data_t bot_data_t;

@interface Target: Entity
{
}
-(vector)realorigin;
-(integer)canSee:(Target)targ ignoring:(entity)ignore;
-(void)setOrigin:(vector) org;
@end

@interface Waypoint: Target
{
@public
	Waypoint [4] targets;
	integer flags;
	vector origin;

	integer b_pants, b_skill, b_shirt, b_frags, b_sound;
	integer keys;
	float items;
	Waypoint enemy;
	float search_time;
}
+(void)clearAll;
+(Waypoint)waypointForNum:(integer)num;
+(void)fixWaypoints;

+(void)clearRouteTable;
+(void)clearMyRoute:(Bot) bot;

-(void)fix;
-(id)init;
-(id)initAt:(vector)org;
-(id)initFromEntity:(entity)ent;

-(integer)isLinkedTo:(Waypoint)way;
-(integer)linkWay:(Waypoint)way;
-(integer)teleLinkWay:(Waypoint)way;
-(void)unlinkWay:(Waypoint)way;

-(void)followLink:(Waypoint)e2 :(integer)b_bit;
-(void)waypointThink;

-(void)clearRoute;
-(void)clearRouteForBot:(Bot)bot;

-(id)queueForThink;
@end
@class Array;
@extern Array waypoint_array;

@interface Bot: Target
{
@public
	//model modelindex
	//frame angles colormap effects
	//owner

	//movetype solid touch <size> watertype flags think nextthink

	integer keys, oldkeys;
	integer buttons, impulse;
	vector v_angle, b_angle;
	vector mouse_emu;

	integer wallhug;
	integer ishuman;
	float b_frags;
	integer b_clientno, b_clientflag;
	float b_shirt, b_pants;
	float ai_time;
	float b_sound;
	float missile_speed;
	float portal_time;
	integer b_skill;
	float switch_wallhug;
	integer b_aiflags;
	integer b_num;
	float b_chattime;
	float b_entertime;
	float b_menu, b_menu_time, b_menu_value;
	integer route_failed;
	integer dyn_flags, dyn_plat;
	float dyn_time;
	Waypoint temp_way, last_way, current_way;
	entity [4] targets;
	entity avoid;
	vector obs_dir;
	vector b_dir;
	vector dyn_dest;
	vector punchangle;

	float teleport_time, portal_time;
}
- (id) init;
- (id) initWithEntity: (entity) e named:(bot_data_t [])name skill:(integer)skill;
- (id) initFromPlayer: (entity) e;
- (void) preThink;
- (void) postThink;
- (void) frame;
- (void) disconnect;

- (void) updateClient;
@end

@interface Bot (Misc)
+(bot_data_t [])name:(integer)r;
+(bot_data_t [])randomName;
-(integer)fov:(entity)targ;

+(void)kick;
@end

@interface Bot (Physics)
- (void)sendMove;
@end

@interface Bot (Move)
- (void)jump;
- (integer)can_rj;
- (integer)recognize_plat: (integer) flag;
- (integer)keysForDir: (vector) sdir;
- (void)obstructed: (vector) whichway : (integer) danger;
- (void)obstacles;
- (void)dodge_obstruction;
- (void)movetogoal;
- (integer)walkmove: (vector) weird;
- (void)roam;
@end

@interface Bot (AI)
-(integer)target_onstack:(entity)scot;
-(void)target_add:(entity)e;
-(void)target_drop:(entity)e;
-(void)lost:(Waypoint)targ :(integer)success;
-(void)check_lost:(Waypoint)targ;
-(void)handle_ai;
-(void)path;
-(float)priority_for_thing:(entity)thing;
-(void)look_for_crap:(integer)scope;
-(void)angle_set;
-(void)AI;
@end

@interface Bot (Fight)
-(float)size_player:(entity)e;
-(void)dodge_stuff;
-(void)weapon_switch:(float)brange;
-(void)shoot;
-(void)fight_style;
@end

@interface Bot (Way)
-(Waypoint)findWaypoint:(Waypoint)start;
-(void)deleteWaypoint:(Waypoint)what;
-(entity)findThing:(string)s;
-(Waypoint)findRoute:(Waypoint)lastone;
-(void)mark_path:(entity)this;
-(void)get_path:(Waypoint)this :(integer)direct;
-(integer)begin_route;
-(void)spawnTempWaypoint:(vector)org;
-(void)dynamicWaypoint;

-(integer)canSee:(Target)targ;
@end

@interface Bot (Chat)
-(void)start_topic:(integer)topic;
-(void)say:(string)msg;
-(void)say2:(string)msg;
-(void)sayTeam:(string)msg;
-(void)sayInit;
-(void)chat;
@end

#define FALSE 0
#define TRUE 1

/* punchangle
 *	bot	fake kick?
 */
@extern .vector	punchangle; // HACK - Don't want to screw with bot_phys

// --------defines-----
#define SVC_UPDATENAME	13
#define SVC_UPDATEFRAGS	14
#define SVC_UPDATECOLORS	17

// used for the physics & movement AI
#define KEY_MOVEUP 		0x001
#define KEY_MOVEDOWN 	0x002
#define KEY_MOVELEFT 	0x004
#define KEY_MOVERIGHT 	0x008
#define KEY_MOVEFORWARD	0x010
#define KEY_MOVEBACK	0x020
#define KEY_LOOKUP		0x040
#define KEY_LOOKDOWN	0x080
#define KEY_LOOKLEFT	0x100
#define KEY_LOOKRIGHT	0x200

#define KEY_LOOK		(KEY_LOOKRIGHT|KEY_LOOKLEFT|KEY_LOOKDOWN|KEY_LOOKUP)
#define KEY_MOVE		(KEY_MOVEBACK|KEY_MOVEFORWARD|KEY_MOVERIGHT\
						 |KEY_MOVELEFT|KEY_MOVEDOWN|KEY_MOVEUP)

// these are aiflags for waypoints
// some overlap to the bot
#define AI_TELELINK_1	0x00001	// link type
#define AI_TELELINK_2	0x00002	// link type
#define AI_TELELINK_3	0x00004	// link type
#define AI_TELELINK_4	0x00008	// link type
#define AI_DOORFLAG		0x00010	// read ahead
#define AI_PRECISION	0x00020	// read ahead + point
#define AI_SURFACE		0x00040	// point
#define AI_BLIND		0x00080	// read ahead + point
#define AI_JUMP			0x00100	// point + ignore
#define AI_DIRECTIONAL	0x00200	// read ahead + ignore
#define AI_PLAT_BOTTOM	0x00400	// read ahead
#define AI_RIDE_TRAIN	0x00800	// read ahead
#define AI_SUPER_JUMP	0x01000	// point + ignore + route test
#define AI_SNIPER		0x02000	// point type
#define AI_AMBUSH		0x04000	// point type
#define AI_DOOR_NO_OPEN	0x08000	// read ahead
#define AI_DIFFICULT	0x10000	// route test
#define AI_TRACE_TEST	0x20000	// route test

// addition masks
#define AI_POINT_TYPES 		(AI_AMBUSH|AI_SNIPER|AI_SUPER_JUMP|AI_JUMP\
							 |AI_BLIND|AI_SURFACE|AI_PRECISION)
#define AI_READAHEAD_TYPES	(AI_DOOR_NO_OPEN|AI_RIDE_TRAIN|AI_PLAT_BOTTOM\
							 |AI_DIRECTIONAL)
#define AI_IGNORE_TYPES		(AI_SUPER_JUMP|AI_DIRECTIONAL|AI_JUMP)

// these are flags for bots/players (dynamic/editor flags)
#define AI_OBSTRUCTED	1
#define AI_HOLD_SELECT	2
#define AI_ROUTE_FAILED	2
#define AI_WAIT			4
#define AI_DANGER		8

#define WM_UNINIT		0
#define WM_DYNAMIC		1
#define WM_LOADING		2
#define WM_LOADED		3
// editor modes aren't available in QW, but we retain support of them
// since the editor is still built into the AI in places
#define WM_EDITOR		4
#define WM_EDITOR_DYNAMIC	5
#define WM_EDITOR_DYNLINK	6

#define OPT_NOCHAT	2

// -------globals-----
@extern Bot [32] players;
@extern float		real_frametime;
@extern float		bot_count, b_options;
@extern float		lasttime;
@extern float		waypoint_mode;
@extern float		dump_mode;
@extern float		direct_route;
@extern float		sv_friction, sv_gravity;
@extern float		sv_accelerate, sv_maxspeed, sv_stopspeed;
@extern Bot			route_table;
@extern integer		busy_waypoints;

@extern float coop;

// -------ProtoTypes------
// external, in main code
@extern void()				ClientConnect;
@extern void()				ClientDisconnect;
@extern void()				SetNewParms;

// rankings
@extern integer (entity e) ClientNumber;

@extern void(vector org, vector bit1, integer bit4, integer flargs) make_way;

@extern void () map_dm1;
@extern void () map_dm2;
@extern void () map_dm3;
@extern void () map_dm4;
@extern void () map_dm5;
@extern void () map_dm6;

// physics & movement
@extern void()				SV_Physics_Client;
@extern void()				SV_ClientThink;
@extern void() 				CL_KeyMove;

// ai & misc
@extern float(float y1, float y2)	angcomp;
@extern float(entity ent, entity targ)		sisible;
@extern vector(entity ent)		realorigin;
@extern float(float v)			frik_anglemod;

@extern void(Waypoint e1, Waypoint e2, integer flag) DeveloperLightning;

/*
	angles is pitch yaw roll
	move is forward right up
*/
@extern void (entity cl, float sec, vector angles, vector move, integer buttons, integer impulse) SV_UserCmd;

@extern integer bot_way_linker;
@extern integer bot_move_linker;
@extern integer bot_phys_linker;
@extern integer bot_chat_linker;

#include "defs.h"
