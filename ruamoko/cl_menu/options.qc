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

#include "gui/InputLine.h"
#include "gui/Pic.h"
#include "gui/Rect.h"
#include "gui/Slider.h"
#include "gui/Text.h"

Group video_options;
CvarToggleView fullscreen_view;
CrosshairView crosshair_view;
CvarToggleView fps_view;
CvarToggleView time_view;
CvarRangeView gamma_view;
CvarRangeView viewsize_view;

Group control_options;			// not done
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

Group audio_options;
CvarRangeView volume_view;
CvarRangeView bgmvolume_view;

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
 
/*
	CB_video_options

	Menu event callback for video options
*/
integer (string text, integer key) 
CB_video_options =
{
	switch (text) {
		case "fullscreen":
			[fullscreen_view toggle];
			break;
		case "crosshair":
			[crosshair_view next];
			break;
		case "fps":
			[fps_view toggle];
			break;
		case "time":
			[time_view toggle];
			break;
		case "gamma":
			if (key == QFK_RIGHT)
				[gamma_view inc];
			else if (key == QFK_LEFT)
				[gamma_view dec];
			break;
		case "viewsize":
			if (key == QFK_RIGHT)
				[viewsize_view inc];
			else if (key == QFK_LEFT)
				[viewsize_view dec];
			break;
	}
	return 0;
};

/*
	DRAW_video_options

	Drawing function for the video options menu
*/
integer (integer x, integer y) 
DRAW_video_options = 
{
	local integer	spacing = 120;

	[video_options setBasePos:x y:y];
	[video_options draw];

	opt_cursor  (x + 62, y + (Menu_GetIndex () * 10) + 60);
	return 1;
};

/*
	MENU_video_options

	Menu function for the video options menu.
*/
void () 
MENU_video_options =
{
	local integer	bar_pad;
	local Rect rect, r;
	local id view;

	Menu_Begin (54, 50, "Video");
	Menu_FadeScreen (1);
	Menu_Draw (DRAW_video_options);

	video_options = [[Group alloc] initWithComponents:0 :0 :320 :200];

	view = [[Pic alloc] initWithComponents:16 :4 :0 :0];
	[view setPic:"gfx/qplaque.lmp"];
	[video_options addView:view];

	view = [[CenterPic alloc] initWithComponents:160 :4 :0 :0];
	[view setPic:"gfx/p_option.lmp"];
	[video_options addView:view];

	view = [[Text alloc] initWithComponents:54 :40 :40 :8];
	[view setText:"Video"];
	[video_options addView:view];

	view = [[Text alloc] initWithComponents:54 :50 :40 :8];
	[view setText:"-----"];
	[video_options addView:view];

	rect = [[Rect alloc] initWithComponents:70 :60 :224 :8];
	fullscreen_view = [[CvarToggleView alloc] initWithBounds:rect title:"Fullscreen" :[[CvarToggle alloc] initWithCvar:"vid_fullscreen"]];
	[video_options addView:fullscreen_view];

	rect.origin.y += 10;
	view = [[Text alloc] initWithBounds:rect];
	[view setText:"Crosshair"];
	[video_options addView:view];
	r = [[Rect alloc] initWithComponents: 0 :rect.origin.y - 3 :14 :14];
	r.origin.x = rect.origin.x + rect.size.width - 24 - r.size.width;
	crosshair_view = [[CrosshairView alloc] initWithBounds:r :[[CrosshairCvar alloc] initWithCvar:"crosshair"]];
	[r dealloc];
	[video_options addView:crosshair_view];

	rect.origin.y += 10;
	fps_view = [[CvarToggleView alloc] initWithBounds:rect title:"Show FPS" :[[CvarToggle alloc] initWithCvar:"hud_fps"]];
	[video_options addView:fps_view];


	rect.origin.y += 10;
	time_view = [[CvarToggleView alloc] initWithBounds:rect title:"Show time" :[[CvarToggle alloc] initWithCvar:"hud_time"]];
	[video_options addView:time_view];


	rect.origin.y += 10;
	gamma_view = [[CvarRangeView alloc] initWithBounds:rect title:"Gamma:" sliderWidth:17 * 8 :[[CvarRange alloc] initWithCvar:"vid_gamma" min:MIN_GAMMA max:MAX_GAMMA step:GAMMA_STEP]];
	[video_options addView:gamma_view];

	rect.origin.y += 10;
	viewsize_view = [[CvarRangeView alloc] initWithBounds:rect title:"Viewsize:" sliderWidth:14 * 8 :[[CvarRange alloc] initWithCvar:"viewsize" min:MIN_VIEWSIZE max:MAX_VIEWSIZE step:VIEWSIZE_STEP]];
	[video_options addView:viewsize_view];

	[rect dealloc];
	
	Menu_Item (54, 60, "fullscreen", CB_video_options, 0);
	Menu_Item (54, 70, "crosshair", CB_video_options, 0);
	Menu_Item (54, 80, "fps", CB_video_options, 0);
	Menu_Item (54, 80, "time", CB_video_options, 0);
	bar_pad = 90;

	Menu_Item (54, bar_pad + 10, "gamma", CB_video_options, 1);
	Menu_Item (54, bar_pad + 20, "viewsize", CB_video_options, 1);
	Menu_End ();
};

/*************************************
 * AUDIO OPTIONS
 * Code for the audio settings menu
 *************************************/

/*
	CB_audio_options

	Callback for the audio settings.
*/
integer (string text, integer key) 
CB_audio_options =
{
	local CvarRangeView range;

	if(!(key == QFK_RIGHT || key == QFK_LEFT )) {
		return 0;
	}
	if (text == "volume")
		range = volume_view;
	else
		range = bgmvolume_view;

	if ((key == QFK_RIGHT) && (key != QFK_LEFT))
		[range inc];
	else
		[range dec];

	return 0;
};

/*
	DRAW_audio_options

	Draws the audio options menu
*/
integer (integer x, integer y) 
DRAW_audio_options = 
{
	[audio_options setBasePos:x y:y];
	[audio_options draw];

	opt_cursor  (x + 62, y + (Menu_GetIndex() * 10) + 60);
	return 1;
};

/*
	MENU_audio_options

	Makes the audio menu
*/
void () 
MENU_audio_options =
{
	local integer bar_pad;
	local Rect rect;
	local id view;

	Menu_Begin (54, 60, "Audio");
	Menu_FadeScreen (1);
	Menu_Draw (DRAW_audio_options);

	audio_options = [[Group alloc] initWithComponents:0 :0 :320 :200];

	view = [[Pic alloc] initWithComponents:16 :4 :0 :0];
	[view setPic:"gfx/qplaque.lmp"];
	[audio_options addView:view];

	view = [[CenterPic alloc] initWithComponents:160 :4 :0 :0];
	[view setPic:"gfx/p_option.lmp"];
	[audio_options addView:view];

	view = [[Text alloc] initWithComponents:54 :40 :40 :8];
	[view setText:"Audio"];
	[audio_options addView:view];

	view = [[Text alloc] initWithComponents:54 :50 :40 :8];
	[view setText:"-----"];
	[audio_options addView:view];

	rect = [[Rect alloc] initWithComponents:70 :60 :17 * 8 :8];
	volume_view = [[CvarRangeView alloc] initWithBounds:rect title:"Volume:" sliderWidth:14 * 8 :[[CvarRange alloc] initWithCvar:"volume" min:MIN_VOLUME max:MAX_VOLUME step:VOLUME_STEP]];
	[audio_options addView:volume_view];

	rect.origin.y += 8;
	bgmvolume_view = [[CvarRangeView alloc] initWithBounds:rect title:"Music :" sliderWidth:14 * 8 :[[CvarRange alloc] initWithCvar:"bgmvolume" min:MIN_VOLUME max:MAX_VOLUME step:VOLUME_STEP]];
	[audio_options addView:bgmvolume_view];

	[rect dealloc];
	
	bar_pad = 0;
	Menu_Item (54, bar_pad + 10, "volume", CB_audio_options, 1);
	Menu_Item (54, bar_pad + 10, "bgmvolume", CB_audio_options, 1);
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
	Draw_Pic (x + 16, y + 4,  Draw_CachePic ("gfx/qplaque.lmp", 1));
	Draw_CenterPic (x + 160, y + 4,  Draw_CachePic ("gfx/p_option.lmp", 1));
	Draw_String	(x + 54, y + 40, "Controls");
	Draw_String (x + 54, y + 50, "--------");
	Draw_String (x + 70, y + 60, "Bindings");

	[grab_mouse_view setBasePos:x y:y];
	[mouse_amp_view setBasePos:x y:y];
	[invert_mouse_view setBasePos:x y:y];
	[autorun_view setBasePos:x y:y];
	[freelook_view setBasePos:x y:y];
	[lookspring_view setBasePos:x y:y];
	[lookstrafe_view setBasePos:x y:y];

	[grab_mouse_view draw];
	[mouse_amp_view draw];
	[invert_mouse_view draw];
	[autorun_view draw];
	[freelook_view draw];
	[lookspring_view draw];
	[lookstrafe_view draw];

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

	Menu_Begin (54, 40, "Controls");
	Menu_FadeScreen (1);
	Menu_CenterPic (160, 4, "gfx/p_option.lmp");

	Menu_Draw (DRAW_control_options);

	rect = [[Rect alloc] initWithComponents:70 :70 :224 :8];
	grab_mouse_view = [[CvarToggleView alloc] initWithBounds:rect title:"Grab mouse" :[[CvarToggle alloc] initWithCvar:"in_grab"]];

	rect.origin.y += 10;
	autorun_view = [[CvarToggleView alloc] initWithBounds:rect title:"Auto run" :[[RunToggle alloc] init]];

	rect.origin.y += 10;
	invert_mouse_view = [[CvarToggleView alloc] initWithBounds:rect title:"Mouse Invert" :[[MouseToggle alloc] init]];

	rect.origin.y += 10;
	rect.size.width += 8;
	mouse_amp_view = [[CvarRangeView alloc] initWithBounds:rect title:"Mouse amp:" sliderWidth:14 * 8 :[[CvarRange alloc] initWithCvar:"in_mouse_amp" min:MIN_MOUSE_AMP max:MAX_MOUSE_AMP step:MOUSE_AMP_STEP]];
	rect.size.width -= 8;

	rect.origin.y += 10;
	freelook_view = [[CvarToggleView alloc] initWithBounds:rect title:"Freelook" :[[CvarToggle alloc] initWithCvar:"freelook"]];

	rect.origin.y += 10;
	lookspring_view = [[CvarToggleView alloc] initWithBounds:rect title:"Lookspring" :[[CvarToggle alloc] initWithCvar:"lookspring"]];

	rect.origin.y += 10;
	lookstrafe_view = [[CvarToggleView alloc] initWithBounds:rect title:"Lookstrafe" :[[CvarToggle alloc] initWithCvar:"lookstrafe"]];

	[rect dealloc];

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

	rect = [[Rect alloc] initWithComponents:70 :60 :224 :8];
	autorecord_view = [[CvarToggleView alloc] initWithBounds:rect title:"Autorecord" :[[CvarToggle alloc] initWithCvar:"cl_autorecord"]];
	[feature_options addView:autorecord_view];

	rect.origin.y += 10;
	fraglog_view = [[CvarToggleView alloc] initWithBounds:rect title:"Frag Logging" :[[CvarToggle alloc] initWithCvar:"cl_fraglog"]];
	[feature_options addView:fraglog_view];

	[rect dealloc];

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
integer () 
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

	player_config_plname_il = [[InputLineBox alloc] initWithBounds:[[Rect alloc] initWithComponents:120 :PLAYER_CONF_Y_PAD :18 :4] promptCharacter:' '];
	[player_config_plname_il setWidth:18];
	[player_options addView:player_config_plname_il];

	player_config_tname_il = [[InputLineBox alloc] initWithBounds:[[Rect alloc] initWithComponents:120 :PLAYER_CONF_Y_PAD + 20 :7 :4] promptCharacter:' '];
	[player_config_tname_il setWidth:7];
	[player_options addView:player_config_tname_il];

	player_config_iactive = NIL;

	rect = [[Rect alloc] initWithComponents:192 :PLAYER_CONF_Y_PAD + 37 :24 :24];
	topcolor_view = [[CvarColorView alloc] initWithBounds:rect :[[CvarColor alloc] initWithCvar:"topcolor"]];
	[player_options addView:topcolor_view];

	rect.origin.y = PLAYER_CONF_Y_PAD + 52;
	bottomcolor_view = [[CvarColorView alloc] initWithBounds:rect :[[CvarColor alloc] initWithCvar:"bottomcolor"]];
	[player_options addView:bottomcolor_view];

	[rect dealloc];

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
integer () 
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

	network_config_rate_il = [[InputLineBox alloc] initWithBounds:[[Rect alloc] initWithComponents: 120 :NETWORK_CONF_Y_PAD :9 :4] promptCharacter:' '];
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
