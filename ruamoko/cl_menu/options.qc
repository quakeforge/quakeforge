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
#include "math.h"
#include "draw.h"
#include "cvar.h"
#include "key.h"
#include "InputLine.h"
#include "Rect.h"
#include "controls_o.h"
#include "options_util.h"

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
#define MOUSE_AMP_STEP 2

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
	local float		val;
	local integer	selected_crosshair;

	switch (text) {
		case "fullscreen":
			Cbuf_AddText ("toggle vid_fullscreen\n");
			break;
		case "crosshair":
			selected_crosshair = ftoi (cvar ("crosshair"));
			selected_crosshair++;
			if (selected_crosshair >= 3) {
			    selected_crosshair = 0;
			}
			cvar_set ("crosshair", itos (selected_crosshair));
			break;
		case "fps":
			Cbuf_AddText ("toggle hud_fps\n");
			break;
		case "time":
			Cbuf_AddText ("toggle hud_time\n");
			break;
	}

	if(!(key == QFK_RIGHT || key == QFK_LEFT )) {
		return 0;
	}
	switch (text) {
		case "gamma":
			val = cvar ("vid_gamma");
			val = min_max_cnt (MIN_GAMMA, MAX_GAMMA, GAMMA_STEP, val, 
							   (key == QFK_RIGHT) && (key != QFK_LEFT));
			cvar_set("vid_gamma", ftos (val));
			break;
		case "viewsize":
			val = cvar ("viewsize");
			val = min_max_cnt (MIN_VIEWSIZE, MAX_VIEWSIZE, VIEWSIZE_STEP, val, 
							   (key == QFK_RIGHT) && (key != QFK_LEFT));
			cvar_set ("viewsize", ftos (val));
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
	local integer	bar_pad, spacing = 120;

	Draw_Pic (x + 16, y + 4, Draw_CachePic ("gfx/qplaque.lmp", 1));
	Draw_CenterPic (x + 160, y + 4, Draw_CachePic ("gfx/p_option.lmp", 1));
	Draw_String	(x + 54, y + 40, "Video");
	Draw_String	(x + 54, y + 50, "-----");
	draw_val_item (x + 70, y + 60, spacing, "Fullscreen", 
				   cvar ("vid_fullscreen") ? "On" : "Off");
	draw_val_item (x + 70, y + 70, spacing, "Crosshair",
				   ftos (cvar ("crosshair")));
	draw_val_item (x + 70, y + 80, spacing, "Show fps", 
				   cvar ("hud_fps") ? "On" : "Off");
	draw_val_item (x + 70, y + 90, spacing, "Show time", 
				   cvar ("hud_time") ? "On" : "Off");
	bar_pad = y + 90;

	Draw_String (x + 70, bar_pad + 10, "Gamma:");
	draw_perc_bar (x + 118,  bar_pad + 10, 15, 
				   to_percentage (MIN_GAMMA, MAX_GAMMA,
				   				  cvar ("vid_gamma")));
	Draw_String (x + 118 + (15 + 4)*8 , bar_pad + 10,
				 ftos (cvar ("vid_gamma")));

	Draw_String (x + 70, bar_pad + 20, "Viewsize:");
	draw_perc_bar  (x + 142,  bar_pad + 20, 12,
					to_percentage (MIN_VIEWSIZE, MAX_VIEWSIZE,
								   cvar ("viewsize")));
	Draw_String (x + 142 + (12 + 4) * 8 , bar_pad + 20,
				 ftos (cvar ("viewsize")));

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

	Menu_Begin (54, 50, "Video");
	Menu_FadeScreen (1);
	Menu_Draw (DRAW_video_options);
	
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
	local float		volume;

	if(!(key == QFK_RIGHT || key == QFK_LEFT )) {
		return 0;
	}
	volume = cvar (text);
	volume = min_max_cnt (MIN_VOLUME, MAX_VOLUME, VOLUME_STEP, volume, 
						  (key == QFK_RIGHT) && (key != QFK_LEFT));
	cvar_set (text, ftos (volume));

	return 0;
};

/*
	DRAW_audio_options

	Draws the audio options menu
*/
integer (integer x, integer y) 
DRAW_audio_options = 
{
	local string tmp = ftos (cvar ("crosshair"));
	local integer bar_pad, spacing = 120;

	Draw_Pic (x + 16, y + 4, Draw_CachePic ("gfx/qplaque.lmp", 1));
	Draw_CenterPic (x + 160, y + 4, Draw_CachePic ("gfx/p_option.lmp", 1));
	Draw_String	(x + 54, y + 40, "Audio");
	Draw_String	(x + 54, y + 50, "-----");
	bar_pad = y + 50;

	Draw_String (x + 70, bar_pad + 10, "Volume:");
	draw_perc_bar  (x + 126,  bar_pad + 10, 15, 
		to_percentage (MIN_VOLUME, MAX_VOLUME, cvar("volume")) );
	Draw_String (x + 126 + (15 + 4)*8 , bar_pad + 10, ftos(cvar("volume")));

	Draw_String (x + 70, bar_pad + 18, "Music :");
	draw_perc_bar  (x + 126,  bar_pad + 18, 15, 
		to_percentage (MIN_VOLUME, MAX_VOLUME, cvar("bgmvolume")) );
	Draw_String (x + 126 + (15 + 4)*8 , bar_pad + 18, ftos(cvar("bgmvolume")));

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

	Menu_Begin (54, 60, "Audio");
	Menu_FadeScreen (1);
	Menu_Draw (DRAW_audio_options);
	
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
	local float val;

	switch (text) {
	    case "in_grab":	    
			Cbuf_AddText ("toggle in_grab\n");
			break;
		case "autorun":
			if(cvar("cl_forwardspeed") < 400) {
				Cbuf_AddText ("set cl_forwardspeed 400\n");
				Cbuf_AddText ("set cl_backspeed 400\n");
			} else {
				Cbuf_AddText ("set cl_forwardspeed 200\n");
				Cbuf_AddText ("set cl_backspeed 200\n");
			}
			break;
		case "freelook":
			Cbuf_AddText ("toggle freelook\n");
			break;
		case "lookspring":
			Cbuf_AddText ("toggle lookspring\n");
			break;
		case "lookstrafe":
			Cbuf_AddText ("toggle lookstrafe\n");
			break;
		case "m_pitch":
			if(cvar("m_pitch") < 0) {
				Cbuf_AddText ("set m_pitch 0.022\n");
			} else {
				Cbuf_AddText ("set m_pitch -0.022\n");
			}
			break;
		case "cl_autorecord":
			Cbuf_AddText ("toggle cl_autorecord\n");
			break;
		case "cl_fraglog":
			Cbuf_AddText ("toggle cl_fraglog\n");
			break;
	}
	if(!(key == QFK_RIGHT || key == QFK_LEFT)) {
		return 0;
	}
	switch (text) {
		case "mouseamp":
			val = cvar("in_mouse_amp");
			val = min_max_cnt(MIN_MOUSE_AMP, MAX_MOUSE_AMP, MOUSE_AMP_STEP, val,
				(key == QFK_RIGHT) && (key != QFK_LEFT));
			cvar_set("in_mouse_amp", ftos(val));
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
	local integer	cursor_pad = 0, spacing = 120, bar_pad;

	Draw_Pic (x + 16, y + 4,  Draw_CachePic ("gfx/qplaque.lmp", 1));
	Draw_CenterPic (x + 160, y + 4,  Draw_CachePic ("gfx/p_option.lmp", 1));
	Draw_String	(x + 54, y + 40, "Controls");
	Draw_String (x + 54, y + 50, "--------");
	Draw_String (x + 70, y + 60, "Bindings");

	draw_val_item (x + 70, y + 70, spacing, "Grab mouse",
				   cvar ("in_grab") ? "On" : "Off");
	draw_val_item (x + 70, y + 80, spacing, "Auto run",
				   cvar ("cl_forwardspeed") < 400 ? "Off" : "On");
	draw_val_item (x + 70, y + 90, spacing, "Mouse Invert",
				   cvar ("m_pitch") < 0 ?  "On" : "Off");
	bar_pad = y + 90;

	Draw_String (x + 70, bar_pad + 10, "Mouse amp:");
	draw_perc_bar (x + 150, bar_pad + 10, 12,
				   to_percentage (MIN_MOUSE_AMP, MAX_MOUSE_AMP,
								  cvar("in_mouse_amp")));
	Draw_String (x + 150 + (12 + 4) * 8 , bar_pad + 10,
				 ftos (cvar ("in_mouse_amp")));

	draw_val_item (x + 70, y + 110, spacing, "Freelook",
				   cvar("freelook") ? "On" : "Off");
	draw_val_item (x + 70, y + 120, spacing, "Lookspring",
				   cvar ("lookspring") ? "On" : "Off");
	draw_val_item (x + 70, y + 130, spacing, "Lookstrafe",
				   cvar ("lookstrafe") ? "On" : "Off");

	opt_cursor (x + 62, y + (Menu_GetIndex () * 10) + 60 + cursor_pad);

	return 1;
};

/*
	MENU_control_options

	Menu make function for control options
*/
void () 
MENU_control_options =
{
	Menu_Begin (54, 40, "Controls");
	Menu_FadeScreen (1);
	Menu_CenterPic (160, 4, "gfx/p_option.lmp");

	Menu_Draw (DRAW_control_options);

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
			Cbuf_AddText ("toggle cl_autorecord\n");
			break;
		case "cl_fraglog":
			Cbuf_AddText ("toggle cl_fraglog\n");
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

	Draw_Pic (x + 16, y + 4, Draw_CachePic ("gfx/qplaque.lmp", 1));
	Draw_CenterPic (x + 160, y + 4, Draw_CachePic ("gfx/p_option.lmp", 1));
	Draw_String (x + 54,  y + 40, "Features");
	Draw_String (x + 54,  y + 50, "--------");

	draw_val_item (x + 70, y + 60, spacing, "Auto Record", 
				   cvar ("cl_autorecord") != 0 ? "On" : "Off");
	draw_val_item (x + 70, y + 70, spacing, "Fraglogging", 
				   cvar ("cl_fraglog") != 0 ? "On" : "Off");

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
	Menu_Begin (54, 70, "Features");
	Menu_FadeScreen (1);
	Menu_CenterPic (160, 4, "gfx/p_option.lmp");
	Menu_Draw (DRAW_feature_options);
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

// input for playername and teamname
InputLine player_config_plname_il;
InputLine player_config_tname_il;

// this holds active inputline pointer
InputLine player_config_iactive;

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
	local float		colortmp;

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
					cvar_set (playername_cvar, [player_config_plname_il text]);
				} else if (player_config_iactive == player_config_tname_il) {
					cvar_set (teamname_cvar, [player_config_tname_il text]);
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
			colortmp = cvar ("topcolor");
			colortmp = min_max_cnt (MIN_COLOR, MAX_COLOR, COLOR_STEP, colortmp,
									(key == QFK_RIGHT) && (key != QFK_LEFT));
			cvar_set ("topcolor", ftos (colortmp));
			break;
		case "bottomcolor":
			colortmp = cvar ("bottomcolor");
			colortmp = min_max_cnt (MIN_COLOR, MAX_COLOR, COLOR_STEP, colortmp,
									(key == QFK_RIGHT) && (key != QFK_LEFT));
			cvar_set ("bottomcolor", ftos (colortmp));
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

	Draw_Pic (x + 16, y + 4, Draw_CachePic ("gfx/qplaque.lmp", 1));
	Draw_CenterPic (x + 160, y + 4, Draw_CachePic ("gfx/p_option.lmp", 1));
	Draw_String (x + 54, y + 40, "Player");
	Draw_String (x + 54, y + 50, "--------");


	Draw_String (x + 70, y + PLAYER_CONF_Y_PAD + 8, "Name..:");
	text_box (x + 120, y + PLAYER_CONF_Y_PAD, 17, 1);
	[player_config_plname_il setBasePos:x y:y];
	[player_config_plname_il draw: player_config_iactive == player_config_plname_il];

	Draw_String (x + 70, y + PLAYER_CONF_Y_PAD + 20 + 8, "Team..:");
	text_box (x + 120, y + PLAYER_CONF_Y_PAD + 20, 5, 1);
	[player_config_tname_il setBasePos:x y:y];
	[player_config_tname_il draw:player_config_iactive == player_config_tname_il];
	

	draw_val_item (x + 70, y + PLAYER_CONF_Y_PAD + 45, spacing, "Top color", 
				   "   " + ftos (cvar ("topcolor"))); 
	draw_val_item (x + 70, y + PLAYER_CONF_Y_PAD + 60, spacing,
				   "Bottom color", "   " + ftos (cvar ("bottomcolor"))); 

	// Draw nice color boxes
	text_box (x + 192, y + PLAYER_CONF_Y_PAD + 45 - 8, 1, 1);
	Draw_Fill (x + 200, y + PLAYER_CONF_Y_PAD + 45, 16, 8,
			   ftoi (cvar ("topcolor")) * 16 + 8);

	text_box (x + 192, y + PLAYER_CONF_Y_PAD + 60 - 8, 1, 1);
	Draw_Fill (x + 200, y + PLAYER_CONF_Y_PAD + 60, 16, 8,
			   ftoi (cvar ("bottomcolor")) * 16 + 8);

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

	[player_config_plname_il setText:Cvar_GetCvarString (playername_cvar)]; 
	[player_config_tname_il setText:Cvar_GetCvarString (teamname_cvar)]; 
};

/*
	MENU_player_options

	Makes the player option menu
*/
void () 
MENU_player_options =
{
	player_config_plname_il = [[InputLine alloc] initWithBounds:[[Rect alloc] initWithComponents:120 :PLAYER_CONF_Y_PAD + 8 :18 :4] promptCharacter:' '];
	[player_config_plname_il setWidth:18];
	player_config_tname_il = [[InputLine alloc] initWithBounds:[[Rect alloc] initWithComponents:120 :PLAYER_CONF_Y_PAD + 8 + 20 :7 :4] promptCharacter:' '];
	[player_config_tname_il setWidth:7];
	player_config_iactive = NIL;

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

// input for playername
InputLine network_config_rate_il;
// this holds active inputline pointer
InputLine network_config_iactive;

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
					cvar_set("rate", [network_config_rate_il text]);
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

	Draw_Pic (x + 16, y + 4, Draw_CachePic ("gfx/qplaque.lmp", 1));
	Draw_CenterPic (x + 160, y + 4, Draw_CachePic ("gfx/p_option.lmp", 1));
	Draw_String (x + 54, y + 40, "Network");
	Draw_String (x + 54, y + 50, "--------");


	Draw_String (x + 70, y + NETWORK_CONF_Y_PAD + 8, "Rate..:");
	text_box (x + 120, y + NETWORK_CONF_Y_PAD, 9, 1);
	[network_config_rate_il setBasePos:x y:y];
	[network_config_rate_il draw: network_config_iactive == network_config_rate_il];

	opt_cursor  (x + 62, y + player_config_cursor_tbl[player_config_cursor]);
	return 1;
};

/*
	CB_ME_network_options

	Entercallback for the networkmenu.
*/
integer () 
CB_ME_network_options =
{
	[network_config_rate_il setText:Cvar_GetCvarString("rate")]; 
};

/*
	MENU_network_options

	Makes the network option menu
*/
void () 
MENU_network_options =
{
	network_config_rate_il = [[InputLine alloc] initWithBounds:[[Rect alloc] initWithComponents: 120 :NETWORK_CONF_Y_PAD + 8 :9 :4] promptCharacter:' '];
	[network_config_rate_il setWidth:9];

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
