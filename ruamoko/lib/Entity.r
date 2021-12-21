#include <Entity.h>

#include <debug.h>
#include <entities.h>
#include <plist.h>
#include <script.h>
#include <string.h>

typedef void () void_function;

int PR_SetField (entity ent, string field, string value) = #0;
@function PR_FindFunction (string func) = #0;

@static void ParseEntities (string ent_data);

@implementation Entity

- (void) own
{
	own = 1;
}

- (id) init
{
	self = [self initWithEntity: spawn ()];
	[self own];
	return self;
}

- (id) initWithEntity: (entity)e
{
	self = [super init];
	ent = e;
	ent.@this = self;
	return self;
}

- (id) initWithEntity: (entity)e fromPlist: (plitem_t *) dict
{
	self = [self initWithEntity: e];
	return self;
}

- (void) dealloc
{
	if (own && ent)
		remove (ent);
	[super dealloc];
}

- (entity) ent
{
	return ent;
}

+ (void) load
{
	//XXX EntityParseFunction (ParseEntities);
}

+ createFromPlist:(plitem_t *) dict
{
	local string classname;
	local id class;
	local entity ent;
	local int count;
	local string field, value;
	local plitem_t *keys;
	local @function func;
	local Entity *e;

	classname = PL_String (PL_ObjectForKey (dict, "classname"));
	if (classname == "worldspawn")
		ent = nil;
	else
		ent = spawn ();
	if ((class = obj_lookup_class (classname))) {
		e = [[class alloc] initWithEntity:ent fromPlist:dict];
	} else {
		e = [[Entity alloc] initWithEntity:ent];
		keys = PL_D_AllKeys (dict);
		count = PL_A_NumObjects (keys);
		while (count--) {
			field = PL_String (PL_ObjectAtIndex (keys, count));
			value = PL_String (PL_ObjectForKey (dict, field));
			PR_SetField (ent, field, value);
		}
		PL_Free (keys);
		if ((func = PR_FindFunction (classname))) {
			//dprint ("calling " + classname + "\n");
			@self = ent;
			func ();
		} else {
			//dprint ("no spawn function for " + classname + "\n");
		}
	}
	if (ent)
		[e own];
	return e;
}

@end

@static void ParseEntities (string ent_data)
{
	local script_t script;
	local plitem_t *plist, *ent, *key, *value;
	local string token;
	local int anglehack, i, count;

	script = Script_New ();
	token = Script_Start (script, "ent data", ent_data);
	if (Script_GetToken (script, 1)) {
		if (token == "(") {
			plist = PL_GetPropertyList (ent_data);
		} else {
			Script_UngetToken (script);
			plist = PL_NewArray ();
			while (Script_GetToken (script, 1)) {
				if (token != "{")
					return; // abort, bad ent data
				ent = PL_NewDictionary ();
				while (1) {
					if (!Script_GetToken (script, 1))
						return; // abort, bad ent data
					if (token == "}")
						break;
					anglehack = 0;
					if (token == "angle") {
						key = PL_NewString ("angles");
						anglehack = 1;
					} else if (token == "light") {
						key = PL_NewString ("light_lev");
					} else {
						key = PL_NewString (token);
					}
					if (!Script_GetToken (script, 0))
						return; // abort, bad ent data
					if (token == "}")
						return; // abort, bad ent data
					if (anglehack)
						value = PL_NewString ("0 " + token + " 0");
					else
						value = PL_NewString (token);
					PL_D_AddObject (ent, PL_String (key), value);
					PL_Free (key);
				}
				PL_A_AddObject (plist, ent);
			}
		}
		//dprint (PL_WritePropertyList (plist));
		count = PL_A_NumObjects (plist);
		for (i = 0; i < count; i++) {
			ent = PL_ObjectAtIndex (plist, i);
			[Entity createFromPlist:ent];
		}
	}
	Script_Delete (script);
}
