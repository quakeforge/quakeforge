
/*
==============================================================================

			SOURCE FOR GLOBALVARS_T C STRUCTURE

==============================================================================
*/

//
// system globals
//
@extern entity          other;
@extern entity          world;
@extern float           time;
@extern float           frametime;

@extern entity          newmis;                         // if this is set, the entity that just
								// run created a new missile that should
								// be simulated immediately


@extern float           force_retouch;          // force all entities to touch triggers
								// next frame.  this is needed because
								// non-moving things don't normally scan
								// for triggers, and when a trigger is
								// created (like a teleport trigger), it
								// needs to catch everything.
								// decremented each frame, so set to 2
								// to guarantee everything is touched
@extern string          mapname;

@extern float           serverflags;            // propagated from level to level, used to
								// keep track of completed episodes

@extern float           total_secrets;
@extern float           total_monsters;

@extern float           found_secrets;          // number of secrets found
@extern float           killed_monsters;        // number of monsters killed


// spawnparms are used to encode information about clients across server
// level changes
@extern float           parm1, parm2, parm3, parm4, parm5, parm6, parm7, parm8, parm9, parm10, parm11, parm12, parm13, parm14, parm15, parm16;

//
// global variables set by built in functions
//
@extern vector          v_forward, v_up, v_right;       // set by makevectors()

// set by traceline / tracebox
@extern float           trace_allsolid;
@extern float           trace_startsolid;
@extern float           trace_fraction;
@extern vector          trace_endpos;
@extern vector          trace_plane_normal;
@extern float           trace_plane_dist;
@extern entity          trace_ent;
@extern float           trace_inopen;
@extern float           trace_inwater;

@extern entity          msg_entity;                             // destination of single entity writes

//
// required prog functions
//
@extern void()          main;                                           // only for testing

@extern void()          StartFrame;

@extern void()          PlayerPreThink;
@extern void()          PlayerPostThink;

@extern void()          ClientKill;
@extern void()          ClientConnect;
@extern void()          PutClientInServer;              // call after setting the parm1... parms
@extern void()          ClientDisconnect;

@extern void()          SetNewParms;                    // called when a client first connects to
									// a server. sets parms so they can be
									// saved off for restarts

@extern void()          SetChangeParms;                 // call to set parms for self so they can
									// be saved for a level transition


//================================================
@extern void            end_sys_globals;                // flag for structure dumping
//================================================

/*
==============================================================================

			SOURCE FOR ENTVARS_T C STRUCTURE

==============================================================================
*/

//
// system fields (*** = do not set in prog code, maintained by C code)
//
@extern .float          modelindex;             // *** model index in the precached list
@extern .vector         absmin, absmax; // *** origin + mins / maxs

@extern .float          ltime;                  // local time for entity
@extern .float          lastruntime;    // *** to allow entities to run out of sequence

@extern .float          movetype;
@extern .float          solid;

@extern .vector         origin;                 // ***
@extern .vector         oldorigin;              // ***
@extern .vector         velocity;
@extern .vector         angles;
@extern .vector         avelocity;

@extern .string         classname;              // spawn function
@extern .string         model;
@extern .float          frame;
@extern .float          skin;
@extern .float          effects;

@extern .vector         mins, maxs;             // bounding box extents reletive to origin
@extern .vector         size;                   // maxs - mins

@extern .void()         touch;
@extern .void()         use;
@extern .void()         think;
@extern .void()         blocked;                // for doors or plats, called when can't push other

@extern .float          nextthink;
@extern .entity         groundentity;



// stats
@extern .float          health;
@extern .float          frags;
@extern .float          weapon;                 // one of the IT_SHOTGUN, etc flags
@extern .string         weaponmodel;
@extern .float          weaponframe;
@extern .float          currentammo;
@extern .float          ammo_shells, ammo_nails, ammo_rockets, ammo_cells;

@extern .float          items;                  // bit flags

@extern .float          takedamage;
@extern .entity         chain;
@extern .float          deadflag;

@extern .vector         view_ofs;                       // add to origin to get eye point


@extern .float          button0;                // fire
@extern .float          button1;                // use
@extern .float          button2;                // jump

@extern .float          impulse;                // weapon changes

@extern .float          fixangle;
@extern .vector         v_angle;                // view / targeting angle for players

@extern .string         netname;

@extern .entity         enemy;

@extern .float          flags;

@extern .float          colormap;
@extern .float          team;

@extern .float          max_health;             // players maximum health is stored here

@extern .float          teleport_time;  // don't back up

@extern .float          armortype;              // save this fraction of incoming damage
@extern .float          armorvalue;

@extern .float          waterlevel;             // 0 = not in, 1 = feet, 2 = wast, 3 = eyes
@extern .float          watertype;              // a contents value

@extern .float          ideal_yaw;
@extern .float          yaw_speed;

@extern .entity         aiment;

@extern .entity         goalentity;             // a movetarget or an enemy

@extern .float          spawnflags;

@extern .string         target;
@extern .string         targetname;

// damage is accumulated through a frame. and sent as one single
// message, so the super shotgun doesn't generate huge messages
@extern .float          dmg_take;
@extern .float          dmg_save;
@extern .entity         dmg_inflictor;

@extern .entity         owner;          // who launched a missile
@extern .vector         movedir;        // mostly for doors, but also used for waterjump

@extern .string         message;                // trigger messages

@extern .float          sounds;         // either a cd track number or sound number

@extern .string         noise, noise1, noise2, noise3;  // contains names of wavs to play

//================================================
@extern void            end_sys_fields;                 // flag for structure dumping
//================================================

/*
==============================================================================

				VARS NOT REFERENCED BY C CODE

==============================================================================
*/


//
// constants
//

// edict.flags
@extern float   FL_FLY;
@extern float   FL_SWIM;
@extern float   FL_CLIENT;    // set for all client edicts
@extern float   FL_INWATER;   // for enter / leave water splash
@extern float   FL_MONSTER;
@extern float   FL_GODMODE;   // player cheat
@extern float   FL_NOTARGET;  // player cheat
@extern float   FL_ITEM;  // extra wide size for bonus items
@extern float   FL_ONGROUND;  // standing on something
@extern float   FL_PARTIALGROUND; // not all corners are valid
@extern float   FL_WATERJUMP; // player jumping out of water
@extern float   FL_JUMPRELEASED; // for jump debouncing

// edict.movetype values
#define MOVETYPE_NONE			0    // never moves
//#define MOVETYPE_ANGLENOCLIP	1
//#define MOVETYPE_ANGLECLIP	2
#define MOVETYPE_WALK			3    // players only
#define MOVETYPE_STEP			4    // discrete, not real time unless fall
#define MOVETYPE_FLY			5
#define MOVETYPE_TOSS			6    // gravity
#define MOVETYPE_PUSH			7    // no clip to world, push and crush
#define MOVETYPE_NOCLIP			8
#define MOVETYPE_FLYMISSILE		9    // fly with extra size against monsters
#define MOVETYPE_BOUNCE			10
#define MOVETYPE_BOUNCEMISSILE	11   // bounce with extra size

// edict.solid values
@extern float   SOLID_NOT;    // no interaction with other objects
@extern float   SOLID_TRIGGER;    // touch on edge, but not blocking
@extern float   SOLID_BBOX;    // touch on edge, block
@extern float   SOLID_SLIDEBOX;    // touch on edge, but not an onground
@extern float   SOLID_BSP;    // bsp clip, touch on edge, block

// range values
@extern float   RANGE_MELEE;
@extern float   RANGE_NEAR;
@extern float   RANGE_MID;
@extern float   RANGE_FAR;

// deadflag values

@extern float   DEAD_NO;
@extern float   DEAD_DYING;
@extern float   DEAD_DEAD;
@extern float   DEAD_RESPAWNABLE;

// takedamage values

@extern float   DAMAGE_NO;
@extern float   DAMAGE_YES;
@extern float   DAMAGE_AIM;

// items
#define IT_AXE				0x001000
#define IT_SHOTGUN			0x000001
#define IT_SUPER_SHOTGUN	0x000002
#define IT_NAILGUN			0x000004
#define IT_SUPER_NAILGUN	0x000008
#define IT_GRENADE_LAUNCHER	0x000010
#define IT_ROCKET_LAUNCHER	0x000020
#define IT_LIGHTNING		0x000040
#define IT_EXTRA_WEAPON		0x000080

#define IT_SHELLS			0x000100
#define IT_NAILS			0x000200
#define IT_ROCKETS			0x000400
#define IT_CELLS			0x000800

#define IT_ARMOR1			0x002000
#define IT_ARMOR2			0x004000
#define IT_ARMOR3			0x008000
#define IT_SUPERHEALTH		0x010000

#define IT_KEY1				0x020000
#define IT_KEY2				0x040000

#define IT_INVISIBILITY		0x080000
#define IT_INVULNERABILITY	0x100000
#define IT_SUIT				0x200000
#define IT_QUAD				0x400000

// point content values

@extern float   CONTENT_EMPTY;
@extern float   CONTENT_SOLID;
@extern float   CONTENT_WATER;
@extern float   CONTENT_SLIME;
@extern float   CONTENT_LAVA;
@extern float   CONTENT_SKY;

@extern float   STATE_TOP;
@extern float   STATE_BOTTOM;
@extern float   STATE_UP;
@extern float   STATE_DOWN;

@extern vector  VEC_ORIGIN;
@extern vector  VEC_HULL_MIN;
@extern vector  VEC_HULL_MAX;

@extern vector  VEC_HULL2_MIN;
@extern vector  VEC_HULL2_MAX;

// protocol bytes
@extern float   SVC_TEMPENTITY;
@extern float   SVC_KILLEDMONSTER;
@extern float   SVC_FOUNDSECRET;
@extern float   SVC_INTERMISSION;
@extern float   SVC_FINALE;
@extern float   SVC_CDTRACK;
@extern float   SVC_SELLSCREEN;
@extern float   SVC_SMALLKICK;
@extern float   SVC_BIGKICK;
@extern float   SVC_MUZZLEFLASH;


@extern float   TE_SPIKE;
@extern float   TE_SUPERSPIKE;
@extern float   TE_GUNSHOT;
@extern float   TE_EXPLOSION;
@extern float   TE_TAREXPLOSION;
@extern float   TE_LIGHTNING1;
@extern float   TE_LIGHTNING2;
@extern float   TE_WIZSPIKE;
@extern float   TE_KNIGHTSPIKE;
@extern float   TE_LIGHTNING3;
@extern float   TE_LAVASPLASH;
@extern float   TE_TELEPORT;
@extern float   TE_BLOOD;
@extern float   TE_LIGHTNINGBLOOD;

// sound channels
// channel 0 never willingly overrides
// other channels (1-7) allways override a playing sound on that channel
@extern float   CHAN_AUTO;
@extern float   CHAN_WEAPON;
@extern float   CHAN_VOICE;
@extern float   CHAN_ITEM;
@extern float   CHAN_BODY;
@extern float   CHAN_NO_PHS_ADD;    // ie: CHAN_BODY+CHAN_NO_PHS_ADD

@extern float   ATTN_NONE;
@extern float   ATTN_NORM;
@extern float   ATTN_IDLE;
@extern float   ATTN_STATIC;

// update types

@extern float   UPDATE_GENERAL;
@extern float   UPDATE_STATIC;
@extern float   UPDATE_BINARY;
@extern float   UPDATE_TEMP;

// entity effects

//float EF_BRIGHTFIELD;
//float EF_MUZZLEFLASH;
@extern float   EF_BRIGHTLIGHT;
@extern float   EF_DIMLIGHT;
@extern float   EF_FLAG1;
@extern float   EF_FLAG2;
// GLQuakeWorld Stuff
@extern float 	EF_BLUE;	// Blue Globe effect for Quad
@extern float	EF_RED;	// Red Globe effect for Pentagram
// messages
@extern float   MSG_BROADCAST;            // unreliable to all
@extern float   MSG_ONE;            // reliable to one (msg_entity)
@extern float   MSG_ALL;            // reliable to all
@extern float   MSG_INIT;            // write to the init string
@extern float   MSG_MULTICAST;            // for multicast() call

// message levels
@extern float   PRINT_LOW;            // pickup messages
@extern float   PRINT_MEDIUM;            // death messages
@extern float   PRINT_HIGH;            // critical messages
@extern float   PRINT_CHAT;            // also goes to chat console

// multicast sets
@extern float   MULTICAST_ALL;            // every client
@extern float   MULTICAST_PHS;            // within hearing
@extern float   MULTICAST_PVS;            // within sight
@extern float   MULTICAST_ALL_R;            // every client, reliable
@extern float   MULTICAST_PHS_R;            // within hearing, reliable
@extern float   MULTICAST_PVS_R;            // within sight, reliable




//================================================

//
// globals
//
@extern float   movedist;

@extern string  string_null;    // null string, nothing should be held here
@extern float   empty_float;

@extern entity  activator;              // the entity that activated a trigger or brush

@extern entity  damage_attacker;        // set by T_Damage
@extern entity  damage_inflictor;
@extern float   framecount;

//
// cvars checked each frame
//
@extern float           teamplay;
@extern float           timelimit;
@extern float           fraglimit;
@extern float           deathmatch;
@extern float           rj;

//================================================

//
// world fields
//
@extern .string         wad;
@extern .string         map;
@extern .float          worldtype;      // 0=medieval 1=metal 2=base

//================================================

@extern .string         killtarget;

//
// quakeed fields
//
@extern .float          light_lev;              // not used by game, but parsed by light util
@extern .float          style;


//
// monster ai
//
@extern .void()         th_stand;
@extern .void()         th_walk;
@extern .void()         th_run;
@extern .void()         th_missile;
@extern .void()         th_melee;
@extern .void(entity attacker, float damage)            th_pain;
@extern .void()         th_die;

@extern .entity         oldenemy;               // mad at this player before taking damage

@extern .float          speed;

@extern .float  lefty;

@extern .float  search_time;
@extern .float  attack_state;

@extern float   AS_STRAIGHT;
@extern float   AS_SLIDING;
@extern float   AS_MELEE;
@extern float   AS_MISSILE;

//
// player only fields
//
@extern .float          voided;
@extern .float          walkframe;

// Zoid Additions
@extern .float		maxspeed;		// Used to set Maxspeed on a player
@extern .float		gravity;		// Gravity Multiplier (0 to 1.0)



@extern .float          attack_finished;
@extern .float          pain_finished;

@extern .float          invincible_finished;
@extern .float          invisible_finished;
@extern .float          super_damage_finished;
@extern .float          radsuit_finished;

@extern .float          invincible_time, invincible_sound;
@extern .float          invisible_time, invisible_sound;
@extern .float          super_time, super_sound;
@extern .float          rad_time;
@extern .float          fly_sound;

@extern .float          axhitme;

@extern .float          show_hostile;   // set to time+0.2 whenever a client fires a
							// weapon or takes damage.  Used to alert
							// monsters that otherwise would let the player go
@extern .float          jump_flag;              // player jump flag
@extern .float          swim_flag;              // player swimming sound flag
@extern .float          air_finished;   // when time > air_finished, start drowning
@extern .float          bubble_count;   // keeps track of the number of bubbles
@extern .string         deathtype;              // keeps track of how the player died

//
// object stuff
//
@extern .string         mdl;
@extern .vector         mangle;                 // angle at start

@extern .vector         oldorigin;              // used by only secret doors

@extern .float          t_length, t_width;


//
// doors, etc
//
@extern .vector         dest, dest1, dest2;
@extern .float          wait;                   // time from firing to restarting
@extern .float          delay;                  // time from activation to firing
@extern .entity         trigger_field;  // door's trigger entity
@extern .string         noise4;

//
// monsters
//
@extern .float          pausetime;
@extern .entity         movetarget;


//
// doors
//
@extern .float          aflag;
@extern .float          dmg;                    // damage done by door when hit

//
// misc
//
@extern .float          cnt;                    // misc flag

//
// subs
//
@extern .void()         think1;
@extern .vector         finaldest, finalangle;

//
// triggers
//
@extern .float          count;                  // for counting triggers


//
// plats / doors / buttons
//
@extern .float          lip;
@extern .float          state;
@extern .vector         pos1, pos2;             // top and bottom positions
@extern .float          height;

//
// sounds
//
@extern .float          waitmin, waitmax;
@extern .float          distance;
@extern .float          volume;




//===========================================================================


//
// builtin functions
//

@extern void(vector ang)        makevectors;           // sets v_forward, etc globals
@extern void(entity e, vector o) setorigin;
@extern void(entity e, string m) setmodel;           // set movetype and solid first
@extern void(entity e, vector min, vector max) setsize;
// #5 was removed
//void() break;
@extern float() random;           // returns 0 - 1
@extern void(entity e, float chan, string samp, float vol, float atten) sound;
@extern vector(vector v) normalize;
@extern void(string e) error;
@extern void(string e) objerror;
@extern float(vector v) vlen;
@extern float(vector v) vectoyaw;
@extern entity() spawn;
@extern void(entity e) remove;

// sets trace_* globals
// nomonsters can be:
// An entity will also be ignored for testing if forent == test,
// forent->owner == test, or test->owner == forent
// a forent of world is ignored
@extern void(vector v1, vector v2, float nomonsters, entity forent) traceline;

@extern entity() checkclient;  // returns a client to look for
@extern entity(entity start, .string fld, string match) find;
@extern string(string s) precache_sound;
@extern string(string s) precache_model;
@extern void(entity client, string s)stuffcmd;
@extern entity(vector org, float rad) findradius;
@extern void(float level, string s) bprint;
@extern void(entity client, float level, string s) sprint;
@extern void(string s) dprint;
@extern string(float f) ftos;
@extern string(vector v) vtos;
@extern void() coredump;          // prints all edicts
@extern void() traceon;          // turns statment trace on
@extern void() traceoff;
@extern void(entity e) eprint;          // prints an entire edict
@extern float(float yaw, float dist) walkmove;  // returns TRUE or FALSE
// #33 was removed
@extern float() droptofloor;  // TRUE if landed on floor
@extern void(float style, string value) lightstyle;
@extern float(float v) rint;          // round to nearest int
@extern float(float v) floor;          // largest integer <= v
@extern float(float v) ceil;          // smallest integer >= v
// #39 was removed
@extern float(entity e) checkbottom;          // true if self is on ground
@extern float(vector v) pointcontents;          // returns a CONTENT_*
// #42 was removed
@extern float(float f) fabs;
@extern vector(entity e, float speed) aim;                // returns the shooting vector
@extern float(string s) cvar;                                             // return cvar.value
@extern void(string s) localcmd;                                  // put string into local que
@extern entity(entity e) nextent;                                 // for looping through all ents
// #48 was removed
@extern void() ChangeYaw;                                         // turn towards self.ideal_yaw
											// at self.yaw_speed
// #50 was removed
@extern vector(vector v) vectoangles;

//
// direct client message generation
//
@extern void(float to, float f) WriteByte;
@extern void(float to, float f) WriteChar;
@extern void(float to, float f) WriteShort;
@extern void(float to, float f) WriteLong;
@extern void(float to, float f) WriteCoord;
@extern void(float to, float f) WriteAngle;
@extern void(float to, string s) WriteString;
@extern void(float to, entity s) WriteEntity;

// several removed

@extern void(float step) movetogoal;

@extern string(string s) precache_file;  // no effect except for -copy
@extern void(entity e) makestatic;
@extern void(string s) changelevel;

//#71 was removed

@extern void(string var, string val) cvar_set;    // sets cvar.value

@extern void(entity client, string s) centerprint;        // sprint, but in middle

@extern void(vector pos, string samp, float vol, float atten) ambientsound;

@extern string(string s) precache_model2;          // registered version only
@extern string(string s) precache_sound2;          // registered version only
@extern string(string s) precache_file2;          // registered version only

@extern void(entity e) setspawnparms;          // set parm1... to the
												// values at level start
												// for coop respawn
@extern void(entity killer, entity killee) logfrag;       // add to stats

@extern string(entity e, string key) infokey;  // get a key value (world = serverinfo)
@extern float(string s) stof;          // convert string to float
@extern void(vector where, float set) multicast;  // sends the temp message to a set
												// of clients, possibly in PVS or PHS

//============================================================================

//
// subs.qc
//
@extern void(vector tdest, float tspeed, void() func) SUB_CalcMove;
@extern void(entity ent, vector tdest, float tspeed, void() func) SUB_CalcMoveEnt;
@extern void(vector destangle, float tspeed, void() func) SUB_CalcAngleMove;
@extern void()  SUB_CalcMoveDone;
@extern void() SUB_CalcAngleMoveDone;
@extern void() SUB_Null;
@extern void() SUB_UseTargets;
@extern void() SUB_Remove;

//
//      combat.qc
//
@extern void(entity targ, entity inflictor, entity attacker, float damage) T_Damage;


@extern float (entity e, float healamount, float ignore) T_Heal; // health function

@extern float(entity targ, entity inflictor) CanDamage;
