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


integer set_key_flag;	// holds flag for the key-setting
 
// three global hashes for the main binding groups
integer basic_binding_hash;
integer misc_binding_hash;
integer weapon_binding_hash;

/*
	init_binding_hash

	this function initializes the hashes for the binding menus
*/
void () 
init_binding_hash =
{
	/* 
	   DESIGN NOTES for the Menu:

	   binding config is loaded into hashes.
	   the key of the hash is the string, which will 
	   be displayed as binding description.
	   The first value of the key is the command, which
	   is bound. 
       The second valu (loaded later) of the hash 
	   will be the keyname. (see get_hash_keys())
	*/

	// Basic keys
	basic_binding_hash = StringHash_Create();
	StringHash_Set(basic_binding_hash, "Attack", "+attack", 0);
	StringHash_Set(basic_binding_hash, "Next weapon", "impulse 10", 0);
	StringHash_Set(basic_binding_hash, "Jump/Swin up", "+jump", 0);
	StringHash_Set(basic_binding_hash, "Walk forward", "+forward", 0);
	StringHash_Set(basic_binding_hash, "Backpedal", "+back", 0);
	StringHash_Set(basic_binding_hash, "Turn left", "+left", 0);
	StringHash_Set(basic_binding_hash, "Turn right", "+right", 0);
	StringHash_Set(basic_binding_hash, "Run", "+speed", 0);
	StringHash_Set(basic_binding_hash, "Step left", "+moveleft", 0);
	StringHash_Set(basic_binding_hash, "Step right", "+moveright", 0);
	StringHash_Set(basic_binding_hash, "Sidestep", "+strafe", 0);
	StringHash_Set(basic_binding_hash, "Look up", "+lookup", 0);
	StringHash_Set(basic_binding_hash, "Look down", "+lookdown", 0);
	StringHash_Set(basic_binding_hash, "Center view", "centerview", 0);
	StringHash_Set(basic_binding_hash, "Mouse look", "+mlook", 0);
	StringHash_Set(basic_binding_hash, "Keyboard look", "+klook", 0);
	StringHash_Set(basic_binding_hash, "Swim up", "+moveup", 0);
	StringHash_Set(basic_binding_hash, "Swim down", "+movedown", 0);

	// Misc keys
	misc_binding_hash = StringHash_Create();
	StringHash_Set(misc_binding_hash, "Pause game", "pause", 0);
	StringHash_Set(misc_binding_hash, "Tog. m.-grab", "toggle in_grab", 0);
	StringHash_Set(misc_binding_hash, "Messagemode", "messagemode", 0);
	StringHash_Set(misc_binding_hash, "Screenshot", "screenshot", 0);

	// Weapon keys
	weapon_binding_hash = StringHash_Create();
	StringHash_Set(weapon_binding_hash, "Axe", "impulse 1", 0);
	StringHash_Set(weapon_binding_hash, "Shotgun", "impulse 2", 0);
	StringHash_Set(weapon_binding_hash, "Super Shotgun", "impulse 3", 0);
	StringHash_Set(weapon_binding_hash, "Nailgun", "impulse 4", 0);
	StringHash_Set(weapon_binding_hash, "Super Nailgun", "impulse 5", 0);
	StringHash_Set(weapon_binding_hash, "Rocket L. ", "impulse 6", 0);
	StringHash_Set(weapon_binding_hash, "Grenade L. ", "impulse 7", 0);
	StringHash_Set(weapon_binding_hash, "Thunderbolt", "impulse 8", 0);

};

/*
	get_keyname

	Gets the string of the key, which is bound 
	to a special binding.
	bindnum is the number of the binding.
	As a command/binding can be bound to many keys,
	you can get the second, third, etc. key by giving 
	the bindnum.
*/
string (string binding, integer bindnum) 
get_keyname =
{
	local integer keynum;
	local string  keyname;

	keynum = Key_LookupBinding(IMT_0, bindnum, binding);
	if(keynum == -1) {
		keyname = "";
	} else {
		keyname = Key_KeynumToString(keynum);
		// cut away the "K_", thats maybe enough as description for now
		keyname = String_Cut(0, 2, keyname);
	}
	return keyname;
};

/*
	get_hash_keys

	gets the keys for a keybinding-hash
*/
void (integer hash_id) 
get_hash_keys =
{
	local integer i,hlen;
	local string binding, desc1 = "", desc2 = "";
	
	hlen = StringHash_Length(hash_id);
	for(i = 0;i < hlen; i++) {
		binding = StringHash_GetIdx(hash_id, i, 0);
		desc1 = get_keyname(binding, 1);	// first key bound to
		desc2 = get_keyname(binding, 2);	// second key bound to

		if(desc2 != "") {
			desc1 += ", " + desc2;
		}
		StringHash_SetIdx(hash_id, i, desc1, 1);
	}
};

/*
	load_keybindings
	
	Loads the kername for into the hashes
*/
void () 
load_keybindings =
{
	get_hash_keys(basic_binding_hash);
	get_hash_keys(misc_binding_hash);
	get_hash_keys(weapon_binding_hash);
};


/*******************
 * BINDINGS OPTIONS
 * Binding settings
 *******************/

/*
	DESIGN NOTE (by elmex): 

	Every sub-menu for control bindings
	has its own hash for holding the keybindings.
	Thats why there are three different functions, which
	shadow the CB_MAIN_control_binding() function.
	They get the binding from the correct hash.
*/

/*
	CB_MAIN_control_binding

	The core function of all control_binding function.
	Its taking the binding as argument.
	This function is called by the real callbacks.
*/
integer (string binding, integer key)
CB_MAIN_control_binding =
{
	local integer retval = 0, bindcnt = 0;

	if(set_key_flag) {
		bindcnt = Key_CountBinding(IMT_0, binding);
		/* we are not binding keys for more than one command
		   by the menu (maybe extended later) */
		if(bindcnt < 2) {
			Key_SetBinding (IMT_0, key, binding);
		} else {
			// else, remove a binding and assign a new one
			dprint ("FOO\n");
			Key_SetBinding (IMT_0, Key_LookupBinding(IMT_0, 1, binding), "");
			Key_SetBinding (IMT_0, key, binding);
		}

		set_key_flag = 0;
		retval = 1;
	} else {
		if(key == QFK_RETURN) {
			set_key_flag = 1;
			retval = 1;
		} else if(key == QFK_BACKSPACE || key == QFK_DELETE) {
			Key_SetBinding (IMT_0, Key_LookupBinding(IMT_0, 1, binding), "");

			retval = 1;
		}
	}

	return retval;
};

/*
	CB_basic_control_binding

	Callback for the basic control bindings menu
*/
integer (string text, integer key) 
CB_basic_control_binding = 
{
	local string binding = StringHash_GetIdx(basic_binding_hash, stoi(text), 0);
	local integer ret = CB_MAIN_control_binding(binding, key);

	// fetch all keynames (possible to optimize.. but not very neccessary)
	get_hash_keys(basic_binding_hash);
	return ret;
};


/*
	CB_ME_basic_control_binding

	Loading basic keynames when entering the
	menu
*/
integer ()
CB_ME_basic_control_binding =
{
    get_hash_keys(basic_binding_hash);
};

/*
	DRAW_basic_control_binding

	Draws the menu for the basic control bindins
*/
integer () 
DRAW_basic_control_binding = 
{
	local integer cursor_pad = 40, bind_desc_pad;
	local integer i, hl;

	bind_desc_pad = 120;

	Draw_String		(20, 10, "Backspace/Delete: Del binding");
	Draw_String     (20, 20, "Enter:            New binding");


	hl = StringHash_Length(basic_binding_hash);	
	for(i=0;i < hl; i++) {
		draw_val_item   (20, 40+(i*10), bind_desc_pad, 
			StringHash_GetIdx(basic_binding_hash, i, -1),
			StringHash_GetIdx(basic_binding_hash, i, 1));
	}

	opt_cursor  (12, (Menu_GetIndex() * 10) + cursor_pad);

	return 1;
};

/*
	MENU_basic_control_binding

	Menu making function for the control bindings
*/
void () 
MENU_basic_control_binding =
{
	local integer i,hl;
	
	Menu_Begin (54, 40, "Basic bindings");
	Menu_FadeScreen (1);
	Menu_EnterHook(CB_ME_basic_control_binding);
	Menu_Draw (DRAW_basic_control_binding);

	hl = StringHash_Length(basic_binding_hash);
	for (i = 0; i < hl; i++) {
		Menu_Item (20, 40 + i*10, itos(i), CB_basic_control_binding, 1);
	}
	Menu_End ();
};


/*
	CB_misc_control_binding

	Callback for misc control bindings.
*/
integer (string text, integer key) 
CB_misc_control_binding = 
{
	local string binding = StringHash_GetIdx(misc_binding_hash, stoi(text), 0);
	local integer ret = CB_MAIN_control_binding(binding, key);

	// fetch all keynames (possible to optimize.. but not very neccessary)
	get_hash_keys(misc_binding_hash);
	return ret;
};

/*
	CB_ME_misc_control_binding

	Loading misc keynames when entering the
	menu
*/
integer ()
CB_ME_misc_control_binding =
{
    get_hash_keys(misc_binding_hash);
};

/*
	DRAW_misc_control_binding

	Draw the bindings for the misc controls
*/
integer () 
DRAW_misc_control_binding =
{
	local integer cursor_pad = 40, bind_desc_pad;
	local integer i, hl;

	bind_desc_pad = 120;

	Draw_String		(20, 10, "Backspace/Delete: Del binding");
	Draw_String     (20, 20, "Enter:            New binding");

	hl = StringHash_Length(misc_binding_hash);	
	for(i=0;i < hl; i++) {
		draw_val_item   (20, 40+(i*10), bind_desc_pad, 
			StringHash_GetIdx(misc_binding_hash, i, -1),
			StringHash_GetIdx(misc_binding_hash, i, 1));
	}

	opt_cursor  (12, (Menu_GetIndex() * 10) + cursor_pad);
	return 1;
};



/*
	MENU_misc_control_binding

	Menu maker function for the misc control binding
*/
void () 
MENU_misc_control_binding =
{
	local integer i,hl;

	Menu_Begin (54, 50, "Misc bindings");
	Menu_FadeScreen (1);
	Menu_EnterHook(CB_ME_misc_control_binding);
	Menu_Draw (DRAW_misc_control_binding);

	hl = StringHash_Length(basic_binding_hash);
	for (i = 0; i < hl; i++) {
		Menu_Item (20, 40 + i*10, itos(i), CB_misc_control_binding, 1);
	}
	Menu_End ();
};

/*
	CB_weapon_control_binding

	Callback function for the weapons control bindings
*/
integer (string text, integer key) 
CB_weapon_control_binding = 
{
	local string binding = StringHash_GetIdx(weapon_binding_hash, stoi(text),0);
	local integer ret = CB_MAIN_control_binding(binding, key);

	// fetch all keynames (possible to optimize.. but not very neccessary)
	get_hash_keys(weapon_binding_hash);
	return ret;
};

/*
	CB_ME_weapon_control_binding

	Loading weapon keynames when entering the
	menu
*/
integer ()
CB_ME_weapon_control_binding =
{
    get_hash_keys(weapon_binding_hash);
};

/*
	DRAW_weapon_control_binding

	Draw the weapon binding menu
*/
integer () 
DRAW_weapon_control_binding =
{
	local integer cursor_pad = 40, bind_desc_pad;
	local integer i,hl;

	bind_desc_pad = 120;

	Draw_String		(20, 10, "Backspace/Delete: Del binding");
	Draw_String     (20, 20, "Enter:            New binding");

	hl = StringHash_Length(weapon_binding_hash);	
	for(i=0;i < hl; i++) {
		draw_val_item   (20, 40+(i*10), bind_desc_pad, 
			StringHash_GetIdx(weapon_binding_hash, i, -1),
			StringHash_GetIdx(weapon_binding_hash, i, 1));
	}

	opt_cursor  (12, (Menu_GetIndex() * 10) + cursor_pad);

	return 1;
};


/*
	MENU_weapon_control_binding

	Menu maker for the weapons menu
*/
void () 
MENU_weapon_control_binding =
{
	local integer i,hl;

	Menu_Begin (54, 60, "Weapon bindings");
	Menu_FadeScreen (1);
	Menu_EnterHook(CB_ME_weapon_control_binding);
	Menu_Draw (DRAW_weapon_control_binding);

	hl = StringHash_Length(basic_binding_hash);
	for (i = 0; i < hl; i++) {
		Menu_Item (20, 40 + i*10, itos(i), CB_weapon_control_binding, 1);
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
	MENU_misc_control_binding  ();
	MENU_weapon_control_binding();

	Menu_End ();
};

