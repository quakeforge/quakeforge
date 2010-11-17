/*
    options.qc

    Options menu

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

#include "options.h"
#include "cbuf.h"
#include "menu.h"
#include "system.h"
#include "string.h"
#include "draw.h"
#include "cvar.h"
#include "key.h"
#include "controls_o.h"
#include "options_util.h"
#include "qfs.h"

#include "PropertyList.h"

#include "gui/InputLine.h"
#include "gui/Pic.h"
#include "gui/Rect.h"
#include "gui/Slider.h"
#include "gui/Text.h"

PLItem menu_plist;

Group video_options;
Group audio_options;

Group control_options;
CvarToggleView grab_mouse_view;
CvarToggleView invert_mouse_view;
CvarToggleView autorun_view;
CvarToggleView freelook_view;
CvarToggleView lookspring_view;
CvarToggleView lookstrafe_view;
CvarRangeView mouse_amp_view;

Group feature_options;
CvarToggleView fraglog_view;
CvarToggleView autorecord_view;

Group player_options;
InputLine player_config_plname_il;
InputLine player_config_tname_il;
InputLine player_config_iactive;
CvarColorView topcolor_view;
CvarColorView bottomcolor_view;

Group network_options;
InputLine network_config_rate_il;
InputLine network_config_iactive;

/*
	some definitions of border values for different things
*/
#define MIN_GAMMA 0.4
#define MAX_GAMMA 3
#define GAMMA_STEP 0.1

#define MIN_VIEWSIZE 30
#define MAX_VIEWSIZE 120
#define VIEWSIZE_STEP 10

#define MIN_MOUSE_AMP 0
#define MAX_MOUSE_AMP 60
#define MOUSE_AMP_STEP 1

#define MIN_VOLUME 0
#define MAX_VOLUME 1.5
#define VOLUME_STEP 0.1

#define MIN_COLOR 0
#define MAX_COLOR 13
#define COLOR_STEP 1

/****************************
 * VIDEO OPTIONS 
 * Video settings menu code
 ****************************/

void dprint (string str) = #0;

id object_from_plist (PLItem plist);

id
class_from_plist (PLDictionary pldict)
{
	local @param [8]params;
	local @va_list va_list = { 0, params };
	local string classname, selname, paramstr;
	local Class class;
	local id obj;
	local PLArray messages, msg;
	local integer message_count, i, j;
	local SEL sel;
	local PLItem item;

	classname = [(PLString) [pldict getObjectForKey:"Class"] string];
	class = obj_lookup_class (classname);
	if (!class) {
		dprint ("could not find " + classname + "\n");
		return NIL;
	}
	obj = [class alloc];

	messages = (PLArray) [pldict getObjectForKey:"Messages"];
	message_count = [messages count];
	for (i = 0; i < message_count; i++) {
		msg = (PLArray) [messages getObjectAtIndex:i];
		selname = [(PLString) [msg getObjectAtIndex:0] string];
		sel = sel_get_uid (selname);
		va_list.count = [msg count] - 1;
		for (j = 0; j < va_list.count; j++) {
			paramstr = [(PLString) [msg getObjectAtIndex:j + 1] string];
			switch (str_mid (paramstr, 0, 1)) {
				case "\"":
					va_list.list[j].string_val = str_mid (paramstr, 1, -1);
					break;
				case "$":
					item = [pldict getObjectForKey:str_mid (paramstr, 1)];
					va_list.list[j].pointer_val = object_from_plist (item);
					break;
				case "0": case "1": case "2": case "3": case "4":
				case "5": case "6": case "7": case "8": case "9":
					if (str_str (paramstr, "."))
						va_list.list[j].float_val = stof (paramstr);
					else
						va_list.list[j].integer_val = stoi (paramstr);
					break;
			}
		}
		obj_msg_sendv (obj, sel, va_list);
	}
	return obj;
}

id
array_from_plist (PLArray plarray)
{
	local Array array;
	local integer i, count;

	array = [[Array alloc] init];
	count = [plarray count];
	for (i = 0; i < count; i++)
		[array addItem: object_from_plist ([plarray getObjectAtIndex:i])];
	return array;
}

Rect
rect_from_plist (PLString plstring)
{
	local string str = [plstring string];
	local string tmp;
	local integer xp, yp, xl, yl;
	local PLItem item;

	if (str_mid (str, 0, 1) == "[") {
		tmp = "(" + str_mid (str, 1, -1) + ")";
		item = [PLItem newFromString:tmp];
		xp = stoi ([(PLString) [item getObjectAtIndex:0] string]);
		yp = stoi ([(PLString) [item getObjectAtIndex:1] string]);
		xl = stoi ([(PLString) [item getObjectAtIndex:2] string]);
		yl = stoi ([(PLString) [item getObjectAtIndex:3] string]);
		return makeRect (xp, yp, xl, yl);
	}
	return makeRect (0, 0, 0, 0);
}

id
string_from_plist (PLString plstring)
{
	return NIL;
}

id
object_from_plist (PLItem plist)
{
	switch ([plist type]) {
		case QFDictionary:
			return class_from_plist ((PLDictionary) plist);
		case QFArray:
			return array_from_plist ((PLArray) plist);
		case QFBinary:
			return NIL;
		case QFString:
			return string_from_plist ((PLString) plist);
	}
}

PLItem read_plist (void)
{
	local string plist_data;
	local QFile file;
	local PLItem plist;

	file = QFS_OpenFile ("menu.plist");
	if (!file) {
		dprint ("could not load menu.plist\n");
		return NIL;
	}
	plist_data = Qreadstring (file, Qfilesize (file));
	Qclose (file);
	plist = [PLItem newFromString:plist_data];
	str_free (plist_data);
	return plist;
}

/*
	DRAW_video_options

	Drawing function for the video options menu
*/
integer (integer x, integer y) 
DRAW_video_options = 
{
	[video_options setBasePos:x y:y];
	[video_options draw];

	return 1;
};

integer (integer key, integer unicode, integer down)
KEY_video_options =
{
	return [video_options keyEvent:key unicode:unicode down:down];
}

/*
	MENU_video_options

	Menu function for the video options menu.
*/
void () 
MENU_video_options =
{
	Menu_Begin (54, 50, "Video");
	Menu_FadeScreen (1);
	Menu_Draw (DRAW_video_options);
	Menu_KeyEvent (KEY_video_options);

	if (menu_plist)
		video_options = object_from_plist ([menu_plist getObjectForKey:"video_options"]);

	Menu_End ();
};

/*************************************
 * AUDIO OPTIONS
 * Code for the audio settings menu
 *************************************/

/*
	DRAW_audio_options

	Draws the audio options menu
*/
integer (integer x, integer y) 
DRAW_audio_options = 
{
	[audio_options setBasePos:x y:y];
	[audio_options draw];

	return 1;
};

integer (integer key, integer unicode, integer down)
KEY_audio_options =
{
	return [audio_options keyEvent:key unicode:unicode down:down];
}

/*
	MENU_audio_options

	Makes the audio menu
*/
void () 
MENU_audio_options =
{
	Menu_Begin (54, 60, "Audio");
	Menu_FadeScreen (1);
	Menu_Draw (DRAW_audio_options);
	Menu_KeyEvent (KEY_audio_options);

	if (menu_plist)
		audio_options = object_from_plist ([menu_plist getObjectForKey:"audio_options"]);

	Menu_End ();
};

/************************
 * CONTROL OPTIONS
 * Control setting code
 ************************/

/*
	CB_control_options

	Callback for control options
*/
integer (string text, integer key) 
CB_control_options =
{
	switch (text) {
	    case "in_grab":	    
			[grab_mouse_view toggle];
			break;
		case "autorun":
			[autorun_view toggle];
			break;
		case "freelook":
			[freelook_view toggle];
			break;
		case "lookspring":
			[lookspring_view toggle];
			break;
		case "lookstrafe":
			[lookstrafe_view toggle];
			break;
		case "m_pitch":
			[invert_mouse_view toggle];
			break;
		case "mouseamp":
			if (key == QFK_RIGHT)
				[mouse_amp_view inc];
			else if (key == QFK_LEFT)
				[mouse_amp_view dec];
			break;
	}
	return 0;
};

/*
	DRAW_control_options

	Draws the control option menu
*/
integer (integer x, integer y) 
DRAW_control_options = 
{
	[control_options setBasePos:x y:y];
	[control_options draw];

	opt_cursor (x + 62, y + (Menu_GetIndex () * 10) + 60);

	return 1;
};

/*
	MENU_control_options

	Menu make function for control options
*/
void () 
MENU_control_options =
{
	local Rect rect;
	local id view;

	Menu_Begin (54, 40, "Controls");
	Menu_FadeScreen (1);
	Menu_CenterPic (160, 4, "gfx/p_option.lmp");

	Menu_Draw (DRAW_control_options);

	control_options = [[Group alloc] initWithComponents:0 :0 :320 :200];

	view = [[Pic alloc] initWithComponents:16 :4 :0 :0];
	[view setPic:"gfx/qplaque.lmp"];
	[control_options addView:view];

	view = [[CenterPic alloc] initWithComponents:160 :4 :0 :0];
	[view setPic:"gfx/p_option.lmp"];
	[control_options addView:view];

	view = [[Text alloc] initWithComponents:54 :40 :64 :8];
	[view setText:"Controls"];
	[control_options addView:view];

	view = [[Text alloc] initWithComponents:54 :50 :64 :8];
	[view setText:"--------"];
	[control_options addView:view];

	view = [[Text alloc] initWithComponents:70 :60 :64 :8];
	[view setText:"Bindings"];
	[control_options addView:view];

	rect = makeRect (70, 70, 224, 8);
	grab_mouse_view = [[CvarToggleView alloc] initWithBounds:rect title:"Grab mouse" :[[CvarToggle alloc] initWithCvar:"in_grab"]];
	[control_options addView:grab_mouse_view];

	rect.origin.y += 10;
	autorun_view = [[CvarToggleView alloc] initWithBounds:rect title:"Auto run" :[[RunToggle alloc] init]];
	[control_options addView:autorun_view];

	rect.origin.y += 10;
	invert_mouse_view = [[CvarToggleView alloc] initWithBounds:rect title:"Mouse Invert" :[[MouseToggle alloc] init]];
	[control_options addView:invert_mouse_view];

	rect.origin.y += 10;
	rect.size.width += 8;
	mouse_amp_view = [[CvarRangeView alloc] initWithBounds:rect title:"Mouse amp:" sliderWidth:14 * 8 :[[CvarRange alloc] initWithCvar:"in_mouse_amp" min:MIN_MOUSE_AMP max:MAX_MOUSE_AMP step:MOUSE_AMP_STEP]];
	rect.size.width -= 8;
	[control_options addView:mouse_amp_view];

	rect.origin.y += 10;
	freelook_view = [[CvarToggleView alloc] initWithBounds:rect title:"Freelook" :[[CvarToggle alloc] initWithCvar:"freelook"]];
	[control_options addView:freelook_view];

	rect.origin.y += 10;
	lookspring_view = [[CvarToggleView alloc] initWithBounds:rect title:"Lookspring" :[[CvarToggle alloc] initWithCvar:"lookspring"]];
	[control_options addView:lookspring_view];

	rect.origin.y += 10;
	lookstrafe_view = [[CvarToggleView alloc] initWithBounds:rect title:"Lookstrafe" :[[CvarToggle alloc] initWithCvar:"lookstrafe"]];
	[control_options addView:lookstrafe_view];

	MENU_control_binding ();

	Menu_Item (54, 70, "in_grab", CB_control_options, 0);
	Menu_Item (54, 80, "autorun", CB_control_options, 0);
	Menu_Item (54, 90, "m_pitch", CB_control_options, 0);
	Menu_Item (54, 100, "mouseamp", CB_control_options, 1);
	Menu_Item (54, 110, "freelook", CB_control_options, 0);
	Menu_Item (54, 120, "lookspring", CB_control_options, 0);
	Menu_Item (54, 130, "lookstrafe", CB_control_options, 0);

	Menu_End ();
};

/***********************************************
 * FEATURES OPTIONS
 * Code of settings for special features of QF
 ***********************************************/

/*
	CB_feature_options

	Callback for feature settings
*/
integer (string text, integer key) 
CB_feature_options =
{
	switch (text) {
		case "cl_autorecord":
			[autorecord_view toggle];
			break;
		case "cl_fraglog":
			[fraglog_view toggle];
			break;
	}
	return 0;
};

/*
	DRAW_feature_options

	Draws the feature option menu
*/
integer (integer x, integer y) 
DRAW_feature_options = 
{
	local integer cursor_pad = 0, spacing = 120;

	[feature_options setBasePos:x y:y];
	[feature_options draw];

	opt_cursor (x + 62, y + (Menu_GetIndex () * 10) + 60 + cursor_pad);
	return 1;
};

/*
	MENU_feature_options

	Makes the feature option menu
*/
void () 
MENU_feature_options =
{
	local Rect rect;
	local id view;

	Menu_Begin (54, 70, "Features");
	Menu_FadeScreen (1);
	Menu_CenterPic (160, 4, "gfx/p_option.lmp");
	Menu_Draw (DRAW_feature_options);

	feature_options = [[Group alloc] initWithComponents:0 :0 :320 :200];

	view = [[Pic alloc] initWithComponents:16 :4 :0 :0];
	[view setPic:"gfx/qplaque.lmp"];
	[feature_options addView:view];

	view = [[CenterPic alloc] initWithComponents:160 :4 :0 :0];
	[view setPic:"gfx/p_option.lmp"];
	[feature_options addView:view];

	view = [[Text alloc] initWithComponents:54 :40 :64 :8];
	[view setText:"Features"];
	[feature_options addView:view];

	view = [[Text alloc] initWithComponents:54 :50 :64 :8];
	[view setText:"--------"];
	[feature_options addView:view];

	rect = makeRect (70, 60, 224, 8);
	autorecord_view = [[CvarToggleView alloc] initWithBounds:rect title:"Autorecord" :[[CvarToggle alloc] initWithCvar:"cl_autorecord"]];
	[feature_options addView:autorecord_view];

	rect.origin.y += 10;
	fraglog_view = [[CvarToggleView alloc] initWithBounds:rect title:"Frag Logging" :[[CvarToggle alloc] initWithCvar:"cl_fraglog"]];
	[feature_options addView:fraglog_view];

	Menu_Item (54, 70, "cl_autorecord", CB_feature_options, 0);
	Menu_Item (54, 80, "cl_fraglog", CB_feature_options, 0);
	Menu_End ();
};


/***************************************************
 * PLAYER OPTIONS
 * Player settings, generally name, team, and color
 ***************************************************/

string playername_cvar; // name of the cvar holding playername (gametype dependend)
string teamname_cvar; // name of the cvar holding teamname (MAY ? gametype dependend)


// Y padding for the player config
#define PLAYER_CONF_Y_PAD 60

// table for cursor-positions
#define NUM_PLAYERCONFIG_CMDS 4
integer [NUM_PLAYERCONFIG_CMDS] player_config_cursor_tbl = { 
	PLAYER_CONF_Y_PAD + 8,
	PLAYER_CONF_Y_PAD + 20 + 8,
	PLAYER_CONF_Y_PAD + 45,
	PLAYER_CONF_Y_PAD + 60
};

integer player_config_cursor;

// array, which holds commands for this menu
string [NUM_PLAYERCONFIG_CMDS] player_config_vals = {
	"",
	"",
	"topcolor",
	"bottomcolor"
};


integer (integer key, integer unicode, integer down) 
KEYEV_player_options =
{
	switch (key) {
		case QFK_DOWN:
		case QFM_WHEEL_DOWN:
			if (!player_config_iactive) {
				player_config_cursor ++;
				player_config_cursor %= NUM_PLAYERCONFIG_CMDS;
			}
			break;
		case QFK_UP:
		case QFM_WHEEL_UP:
			if (!player_config_iactive) {
				player_config_cursor += NUM_PLAYERCONFIG_CMDS - 1;
				player_config_cursor %= NUM_PLAYERCONFIG_CMDS;
			}
			break;
		case QFK_RETURN:
			if (player_config_iactive) {
				if (player_config_iactive == player_config_plname_il) {
					Cvar_SetString (playername_cvar,
									[player_config_plname_il text]);
				} else if (player_config_iactive == player_config_tname_il) {
					Cvar_SetString (teamname_cvar,
									[player_config_tname_il text]);
				}
				player_config_iactive = NIL;
			} else {
				if (player_config_cursor == 0) {
					player_config_iactive = player_config_plname_il;
				} else if (player_config_cursor == 1) {
					player_config_iactive = player_config_tname_il;
				}
			}
			break;
	}
	if(key != QFK_RETURN && player_config_iactive) {
		[player_config_iactive  processInput:(key >= 256 ? key : unicode)];
	}

	if(!(key == QFK_RIGHT || key == QFK_LEFT )) {
		return 1;
	}

	switch (player_config_vals[player_config_cursor]) {
		case "topcolor":
			if (key == QFK_RIGHT)
				[topcolor_view next];
			else if (key == QFK_LEFT)
				[topcolor_view prev];
			break;
		case "bottomcolor":
			if (key == QFK_RIGHT)
				[bottomcolor_view next];
			else if (key == QFK_LEFT)
				[bottomcolor_view prev];
			break;
	}

	return 1;
};

/*
	DRAW_player_options

	Draws the player option menu
*/
integer (integer x, integer y) 
DRAW_player_options = 
{
	local integer	cursor_pad = 0, spacing = 120;

	[player_config_plname_il cursor: player_config_iactive == player_config_plname_il];
	[player_config_tname_il cursor: player_config_iactive == player_config_tname_il];

	[player_options setBasePos:x y:y];
	[player_options draw];

	opt_cursor  (x + 62, y + player_config_cursor_tbl[player_config_cursor]);
	return 1;
};

/*
	CB_ME_player_options

	Entercallback for the playermenu.
	For initalising the playername and teamname.
*/
void () 
CB_ME_player_options =
{
	if (gametype () == "quakeworld") {
		playername_cvar = "name";	
	} else {
		playername_cvar = "_cl_name";
	}

	teamname_cvar = "team"; // FIXME: is this something else in netquake?

	[player_config_plname_il setText:Cvar_GetString (playername_cvar)]; 
	[player_config_tname_il setText:Cvar_GetString (teamname_cvar)]; 
};

/*
	MENU_player_options

	Makes the player option menu
*/
void () 
MENU_player_options =
{
	local Rect rect;
	local id view;

	player_options = [[Group alloc] initWithComponents:0 :0 :320 :200];

	view = [[Pic alloc] initWithComponents:16 :4 :0 :0];
	[view setPic:"gfx/qplaque.lmp"];
	[player_options addView:view];

	view = [[CenterPic alloc] initWithComponents:160 :4 :0 :0];
	[view setPic:"gfx/p_option.lmp"];
	[player_options addView:view];

	view = [[Text alloc] initWithComponents:54 :40 :48 :8];
	[view setText:"Player"];
	[player_options addView:view];

	view = [[Text alloc] initWithComponents:54 :50 :48 :8];
	[view setText:"------"];
	[player_options addView:view];

	view = [[Text alloc] initWithComponents:70 :PLAYER_CONF_Y_PAD + 8 :32 :8];
	[view setText:"Name"];
	[player_options addView:view];

	view = [[Text alloc] initWithComponents:70 :PLAYER_CONF_Y_PAD + 20 :32 :8];
	[view setText:"Team"];
	[player_options addView:view];

	view = [[Text alloc] initWithComponents:70 :PLAYER_CONF_Y_PAD + 45 :88 :8];
	[view setText:"Shirt color"];
	[player_options addView:view];

	view = [[Text alloc] initWithComponents:70 :PLAYER_CONF_Y_PAD + 60 :88 :8];
	[view setText:"Pants color"];
	[player_options addView:view];

	player_config_plname_il = [[InputLineBox alloc] initWithBounds:makeRect (120, PLAYER_CONF_Y_PAD, 18, 4) promptCharacter:' '];
	[player_config_plname_il setWidth:18];
	[player_options addView:player_config_plname_il];

	player_config_tname_il = [[InputLineBox alloc] initWithBounds:makeRect (120, PLAYER_CONF_Y_PAD + 20, 7, 4) promptCharacter:' '];
	[player_config_tname_il setWidth:7];
	[player_options addView:player_config_tname_il];

	player_config_iactive = NIL;

	rect = makeRect (192, PLAYER_CONF_Y_PAD + 37, 24, 24);
	topcolor_view = [[CvarColorView alloc] initWithBounds:rect :[[CvarColor alloc] initWithCvar:"topcolor"]];
	[player_options addView:topcolor_view];

	rect.origin.y = PLAYER_CONF_Y_PAD + 52;
	bottomcolor_view = [[CvarColorView alloc] initWithBounds:rect :[[CvarColor alloc] initWithCvar:"bottomcolor"]];
	[player_options addView:bottomcolor_view];

	Menu_Begin (54, 80, "Player");
	Menu_FadeScreen (1);
	Menu_KeyEvent (KEYEV_player_options);
	Menu_EnterHook (CB_ME_player_options);
	Menu_Draw (DRAW_player_options);
	Menu_End ();
};

/*****************************************************************************
 * NETWORK OPTIONS
 * Options, which have to do with network stuff (rate, noskins, netgraph, ...)
 *****************************************************************************/

integer network_config_cursor;

// Y padding for the player config
#define NETWORK_CONF_Y_PAD 60

// table for cursor-positions
#define NUM_NETWORKCONFIG_CMDS 1
integer [NUM_NETWORKCONFIG_CMDS] network_config_cursor_tbl = { 
	PLAYER_CONF_Y_PAD + 8, 
};

integer network_config_cursor;

// array, which holds commands for this menu
string [NUM_NETWORKCONFIG_CMDS] network_config_vals = {
	"",
};


integer (integer key, integer unicode, integer down) 
KEYEV_network_options =
{
	switch (key) {
		case QFK_DOWN:
		case QFM_WHEEL_DOWN:
			if (!network_config_iactive) {
				network_config_cursor ++;
				network_config_cursor %= NUM_PLAYERCONFIG_CMDS;
			}
			break;
		case QFK_UP:
		case QFM_WHEEL_UP:
			if (!network_config_iactive) {
				network_config_cursor += NUM_PLAYERCONFIG_CMDS - 1;
				network_config_cursor %= NUM_PLAYERCONFIG_CMDS;
			}
			break;
		case QFK_RETURN:
			if (network_config_iactive) {
				if(network_config_iactive == network_config_rate_il) {
					Cvar_SetString ("rate",
									[network_config_rate_il text]);
				}
				network_config_iactive = NIL;
			} else {
				if (network_config_cursor == 0) {
					network_config_iactive = network_config_rate_il;
				}
			}
			break;
	}
	if (key != QFK_RETURN && network_config_iactive) {
		[network_config_iactive processInput:(key >= 256 ? key : unicode)];
	}

	if (!(key == QFK_RIGHT || key == QFK_LEFT )) {
		return 1;
	}

//	switch (network_config_vals[network_config_cursor]) {
//	} // none yet

	return 1;
};

/*
	DRAW_network_options

	Draws the network option menu
*/
integer (integer x, integer y) 
DRAW_network_options = 
{
	local integer	cursor_pad = 0, spacing = 120;

	[network_config_rate_il cursor:network_config_iactive == network_config_rate_il];
	[network_options setBasePos:x y:y];
	[network_options draw];

	opt_cursor  (x + 62, y + network_config_cursor_tbl[network_config_cursor]);
	return 1;
};

/*
	CB_ME_network_options

	Entercallback for the networkmenu.
*/
void () 
CB_ME_network_options =
{
	[network_config_rate_il setText:Cvar_GetString ("rate")]; 
};

/*
	MENU_network_options

	Makes the network option menu
*/
void () 
MENU_network_options =
{
	local id view;

	network_options = [[Group alloc] initWithComponents:0 :0 :320 :200];

	view = [[Pic alloc] initWithComponents:16 :4 :0 :0];
	[view setPic:"gfx/qplaque.lmp"];
	[network_options addView:view];

	view = [[CenterPic alloc] initWithComponents:160 :4 :0 :0];
	[view setPic:"gfx/p_option.lmp"];
	[network_options addView:view];

	view = [[Text alloc] initWithComponents:54 :40 :56 :8];
	[view setText:"Network"];
	[network_options addView:view];

	view = [[Text alloc] initWithComponents:54 :50 :56 :8];
	[view setText:"-------"];
	[network_options addView:view];

	view = [[Text alloc] initWithComponents:70 :NETWORK_CONF_Y_PAD + 8 :56 :8];
	[view setText:"Rate..:"];
	[network_options addView:view];

	network_config_rate_il = [[InputLineBox alloc] initWithBounds:makeRect (120, NETWORK_CONF_Y_PAD, 9, 4) promptCharacter:' '];
	[network_config_rate_il setWidth:9];
	[network_options addView:network_config_rate_il];

	Menu_Begin (54, 90, "Network");
	Menu_FadeScreen (1);
	Menu_KeyEvent (KEYEV_network_options);
	Menu_EnterHook (CB_ME_network_options);
	Menu_Draw (DRAW_network_options);
	Menu_End ();
};

integer (string text, integer key)
op_goto_console =
{
	Menu_SelectMenu ("");
	Cbuf_AddText ("toggleconsole\n");
	return 0;
};

/*************************
 * MAIN OPTIONS 
 * Main options menu code
 *************************/

/*
	MENU_options

	Makes the main options menu
*/
void () 
MENU_options =
{
	local integer	spacing = 120;

	menu_plist = read_plist ();

	Menu_Begin (54, 72, "");
	Menu_FadeScreen (1);
	Menu_Pic (16, 4, "gfx/qplaque.lmp");
	Menu_CenterPic (160, 4, "gfx/p_option.lmp");

	Menu_Item (54, 32, "Go to Console", op_goto_console, 0);
	MENU_control_options ();
	MENU_video_options ();
	MENU_audio_options ();
	MENU_feature_options ();
	MENU_player_options ();
	MENU_network_options ();

	Menu_End ();
};
