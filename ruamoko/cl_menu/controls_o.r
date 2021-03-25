/*
    controls_o.qc

    Controls settings menu

    Copyright (C) 2002 Robin Redeker <elmex@x-paste.de>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public
    License along with this program; if not, write to:

        Free Software Foundation, Inc.
        59 Temple Place - Suite 330
        Boston, MA  02111-1307, USA
*/

#include "Array.h"

#include "menu.h"
#include "draw.h"
#include "system.h"
#include "debug.h"
#include "legacy_string.h"
#include "string.h"
#include "key.h"
#include "options_util.h"

int set_key_flag;	// holds flag for the key-setting

// three global hashes for the main binding groups
Array *movement_bindings;
Array *misc_bindings;
Array *weapon_bindings;

struct binding_s {
	string      text;
	string      command;
	string      keys;
};
typedef struct binding_s binding_t;

@interface Binding : Object
{
@public
	string      text;
	string      command;
	string      keys;
}
-initWithBinding: (binding_t) binding;
@end

@implementation Binding
-initWithBinding: (binding_t) binding
{
	self = [self init];
	if (self) {
		text = binding.text;
		command = binding.command;
		keys = binding.keys;
	}
	return self;
}
@end

binding_t movement_binding_list[16] = {
	{"Jump/Swin up", "+jump"},
	{"Walk forward", "+forward"},
	{"Backpedal", "+back"},
	{"Turn left", "+left"},
	{"Turn right", "+right"},
	{"Run", "+speed"},
	{"Step left", "+moveleft"},
	{"Step right", "+moveright"},
	{"Sidestep", "+strafe"},
	{"Look up", "+lookup"},
	{"Look down", "+lookdown"},
	{"Center view", "centerview"},
	{"Mouse look", "+mlook"},
	{"Keyboard look", "+klook"},
	{"Swim up", "+moveup"},
	{"Swim down", "+movedown"},
};

binding_t misc_binding_list [4] = {
	{"Pause game", "pause"},
	{"Tog. m.-grab", "toggle in_grab"},
	{"Messagemode", "messagemode"},
	{"Screenshot", "screenshot"},
};

binding_t weapon_binding_list [11] = {
	{"Attack", "+attack"},
	{"Next weapon", "impulse 10"},
	{"Prev. weapon", "impulse 12"},
	{"Axe", "impulse 1"},
	{"Shotgun", "impulse 2"},
	{"Super Shotgun", "impulse 3"},
	{"Nailgun", "impulse 4"},
	{"Super Nailgun", "impulse 5"},
	{"Grenade L. ", "impulse 6"},
	{"Rocket L. ", "impulse 7"},
	{"Thunderbolt", "impulse 8"},
};

Binding *
new_binding (binding_t binding)
{
	return [[Binding alloc] initWithBinding:binding];
}

/*
	init_binding_hash

	this function initializes the hashes for the binding menus
*/
void ()
init_binding_hash =
{
	local int i;

	movement_bindings = [[Array alloc] init];
	for (i = 0; i < @sizeof (movement_binding_list) / @sizeof (movement_binding_list[0]); i++)
		[movement_bindings addObject: new_binding (movement_binding_list[i])];
	misc_bindings = [[Array alloc] init];
	for (i = 0; i < @sizeof (misc_binding_list) / @sizeof (misc_binding_list[0]); i++)
		[misc_bindings addObject: new_binding (misc_binding_list[i])];
	weapon_bindings = [[Array alloc] init];
	for (i = 0; i < @sizeof (weapon_binding_list) / @sizeof (weapon_binding_list[0]); i++)
		[weapon_bindings addObject: new_binding (weapon_binding_list[i])];

};

/*
	get_keyname

	Gets the string of the key, which is bound to a special binding.
	bindnum is the number of the binding.
	As a command/binding can be bound to many keys,	you can get the second,
	third, etc. key by giving the bindnum.
*/
string (string binding, int bindnum)
get_keyname =
{
	local int keynum;
	local string  keyname;

	keynum = Key_LookupBinding("imt_0", bindnum, binding);
	if(keynum == -1) {
		keyname = "";
	} else {
		keyname = Key_KeynumToString(keynum);
		// cut away the "K_", thats maybe enough as description for now
		//keyname = String_Cut(0, 2, keyname);
	}
	return keyname;
};

/*
	get_hash_keys

	gets the keys for a keybinding-hash
*/
void
get_hash_keys (Array *list)
{
	local int i,hlen;
	local Binding *binding;
	local string desc1 = "", desc2 = "";
	
	hlen = [list count];
	for(i = 0; i < hlen; i++) {
		binding = [list objectAtIndex: i];
		desc1 = get_keyname (binding.command, 1);	// first key bound to
		desc2 = get_keyname (binding.command, 2);	// second key bound to

		if (desc2 != "") {
			desc1 += ", " + desc2;
		}
		if (binding.keys) {
			str_free (binding.keys);
			binding.keys = nil;
		}
		if (desc1) {
			binding.keys = str_new ();
			str_copy (binding.keys, desc1);
		}
	}
};

/*
	load_keybindings
	
	Loads the kername for into the hashes
*/
void ()
load_keybindings =
{
	get_hash_keys (movement_bindings);
	get_hash_keys (misc_bindings);
	get_hash_keys (weapon_bindings);
};

/*******************
 * BINDINGS OPTIONS
 * Binding settings
 *******************/

/*
	DESIGN NOTE (by elmex):

	Every sub-menu for control bindings has its own hash for holding the
	keybindings. Thats why there are three different functions, which shadow
	the CB_MAIN_control_binding() function. They get the binding from the
	correct hash.
*/

/*
	CB_MAIN_control_binding

	The core function of all control_binding function.
	It takes the binding as argument.
	This function is called by the real callbacks.
*/
int
CB_MAIN_control_binding (Binding *binding, int key)
{
	local int	retval = 0, bindcnt = 0;

	if(set_key_flag) {
		bindcnt = Key_CountBinding("imt_0", binding.command);
		/* we are not binding keys for more than one command
		   by the menu (maybe extended later) */
		if(bindcnt < 2) {
			Key_SetBinding ("imt_0", key, binding.command);
		} else {
			// else, remove a binding and assign a new one
			Key_SetBinding ("imt_0", Key_LookupBinding("imt_0", 1, binding.command), "");
			Key_SetBinding ("imt_0", key, binding.command);
		}

		set_key_flag = 0;
		retval = 1;
	} else {
		if(key == QFK_RETURN) {
			set_key_flag = 1;
			retval = 1;
		} else if(key == QFK_BACKSPACE || key == QFK_DELETE) {
			Key_SetBinding ("imt_0", Key_LookupBinding("imt_0", 1, binding.command), "");

			retval = 1;
		}
	}

	return retval;
};

/*
	CB_basic_control_binding

	Callback for the basic control bindings menu
*/
int (string text, int key)
CB_basic_control_binding =
{
	local Binding *binding = [movement_bindings objectAtIndex: stoi (text)];
	local int ret = CB_MAIN_control_binding (binding, key);

	// fetch all keynames (possible to optimize.. but not very neccessary)
	get_hash_keys (movement_bindings);
	return ret;
};

/*
	CB_ME_basic_control_binding

	Loading basic keynames when entering the menu
*/
void ()
CB_ME_basic_control_binding =
{
    get_hash_keys (movement_bindings);
};

/*
	DRAW_basic_control_binding

	Draws the menu for the basic control bindins
*/
int (int x, int y)
DRAW_basic_control_binding =
{
	local int	cursor_pad = 40, bind_desc_pad, hl, i;

	bind_desc_pad = 120;

	Draw_String (x + 20, y + 10, "Backspace/Delete: Del binding");
	Draw_String (x + 20, y + 20, "Enter:            New binding");


	hl = [movement_bindings count];	
	for(i = 0; i < hl; i++) {
		local Binding *binding = [movement_bindings objectAtIndex: i];
		draw_val_item   (x + 20, y + 40 + ( i * 10), bind_desc_pad,
			binding.text, binding.keys);
	}

	opt_cursor  (x + 12, y + (Menu_GetIndex () * 10) + cursor_pad);

	return 0;
};

/*
	MENU_basic_control_binding

	Menu making function for the control bindings
*/
void ()
MENU_basic_control_binding =
{
	local int i,hl;
	
	Menu_Begin (54, 40, "Movement bindings");
	Menu_FadeScreen (1);
	Menu_EnterHook (CB_ME_basic_control_binding);
	Menu_Draw (DRAW_basic_control_binding);

	hl = [movement_bindings count];
	for (i = 0; i < hl; i++) {
		Menu_Item (20, 40 + i * 10, itos (i), CB_basic_control_binding, 1);
	}
	Menu_End ();
};

/*
	CB_misc_control_binding

	Callback for misc control bindings.
*/
int (string text, int key)
CB_misc_control_binding =
{
	local Binding *binding = [misc_bindings objectAtIndex: stoi (text)];
	local int ret = CB_MAIN_control_binding (binding, key);

	// fetch all keynames (possible to optimize.. but not very neccessary)
	get_hash_keys (misc_bindings);
	return ret;
};

/*
	CB_ME_misc_control_binding

	Loading misc keynames when entering the menu
*/
void ()
CB_ME_misc_control_binding =
{
    get_hash_keys(misc_bindings);
};

/*
	DRAW_misc_control_binding

	Draw the bindings for the misc controls
*/
int (int x, int y)
DRAW_misc_control_binding =
{
	local int cursor_pad = 40, bind_desc_pad;
	local int i, hl;

	bind_desc_pad = 120;

	Draw_String (x + 20, y + 10, "Backspace/Delete: Del binding");
	Draw_String (x + 20, y + 20, "Enter:            New binding");

	hl = [misc_bindings count];	
	for(i=0;i < hl; i++) {
		local Binding *binding = [misc_bindings objectAtIndex: i];
		draw_val_item   (x + 20, y + 40+(i*10), bind_desc_pad,
			binding.text, binding.keys);
	}

	opt_cursor  (x + 12, y + (Menu_GetIndex() * 10) + cursor_pad);
	return 0;
};

/*
	MENU_misc_control_binding

	Menu maker function for the misc control binding
*/
void ()
MENU_misc_control_binding =
{
	local int	hl, i;

	Menu_Begin (54, 50, "Misc bindings");
	Menu_FadeScreen (1);
	Menu_EnterHook (CB_ME_misc_control_binding);
	Menu_Draw (DRAW_misc_control_binding);

	hl = [misc_bindings count];
	for (i = 0; i < hl; i++) {
		Menu_Item (20, 40 + i * 10, itos (i), CB_misc_control_binding, 1);
	}
	Menu_End ();
};

/*
	CB_weapon_control_binding

	Callback function for the weapons control bindings
*/
int (string text, int key)
CB_weapon_control_binding =
{
	local Binding *binding = [weapon_bindings objectAtIndex: stoi (text)];
	local int ret = CB_MAIN_control_binding (binding, key);

	// fetch all keynames (possible to optimize.. but not very neccessary)
	get_hash_keys (weapon_bindings);
	return ret;
};

/*
	CB_ME_weapon_control_binding

	Loading weapon keynames when entering the
	menu
*/
void ()
CB_ME_weapon_control_binding =
{
    get_hash_keys(weapon_bindings);
};

/*
	DRAW_weapon_control_binding

	Draw the weapon binding menu
*/
int (int x, int y)
DRAW_weapon_control_binding =
{
	local int	cursor_pad = 40, bind_desc_pad, hl, i;

	bind_desc_pad = 120;

	Draw_String	(x + 20, y + 10, "Backspace/Delete: Del binding");
	Draw_String (x + 20, y + 20, "Enter:            New binding");

	hl = [weapon_bindings count];	
	for(i = 0; i < hl; i++) {
		local Binding *binding = [weapon_bindings objectAtIndex: i];
		draw_val_item (x + 20, y + 40 + (i * 10), bind_desc_pad,
			binding.text, binding.keys);
	}

	opt_cursor  (x + 12, y + (Menu_GetIndex () * 10) + cursor_pad);

	return 0;
};

/*
	MENU_weapon_control_binding

	Menu maker for the weapons menu
*/
void ()
MENU_weapon_control_binding =
{
	local int	hl, i;

	Menu_Begin (54, 60, "Weapon bindings");
	Menu_FadeScreen (1);
	Menu_EnterHook (CB_ME_weapon_control_binding);
	Menu_Draw (DRAW_weapon_control_binding);

	hl = [weapon_bindings count];
	for (i = 0; i < hl; i++) {
		Menu_Item (20, 40 + i * 10, itos (i), CB_weapon_control_binding, 1);
	}
	Menu_End ();
};

/*
	MENU_control_binding

	Main controls menu, for selecting the sub control menus
*/
void ()
MENU_control_binding =
{
	init_binding_hash (); // init the keybinding hashes

	Menu_Begin (54, 60, "Bindings");
	Menu_Pic (16, 4, "gfx/qplaque.lmp");
	Menu_CenterPic (160, 4, "gfx/p_option.lmp");

	Menu_FadeScreen (1);

	MENU_basic_control_binding ();
	MENU_misc_control_binding ();
	MENU_weapon_control_binding();

	Menu_End ();
};
