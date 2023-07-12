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

#include "debug.h"
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

#include "plistmenu.h"

#include "gui/Group.h"
#include "gui/InputLine.h"
#include "gui/Pic.h"
#include "gui/Rect.h"
#include "gui/Slider.h"
#include "gui/Text.h"

#include "CvarToggleView.h"
#include "CvarRangeView.h"
#include "CvarColorView.h"

#include "CvarColor.h"
#include "CvarToggle.h"

Group *video_options;
Group *audio_options;
Group *control_options;
Group *feature_options;
Group *network_options;

Group *player_options;
InputLine *player_config_plname_il;
InputLine *player_config_tname_il;
InputLine *player_config_iactive;
CvarColorView *topcolor_view;
CvarColorView *bottomcolor_view;

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
	DRAW_video_options

	Drawing function for the video options menu
*/
int (int x, int y)
DRAW_video_options =
{
	[video_options setBasePos:x y:y];
	[video_options draw];

	return 1;
};

int (int key, int unicode, int down)
KEY_video_options =
{
	return [video_options keyEvent:key unicode:unicode down:down];
}

/*
	MENU_video_options

	Menu function for the video options menu.
*/
void
MENU_video_options (PLItem *plist)
{
	local @param ret;
	Menu_Begin (54, 50, "Video");
	Menu_FadeScreen (1);
	Menu_Draw (DRAW_video_options);
	Menu_KeyEvent (KEY_video_options);

	if (plist) {
		ret = object_from_plist ([(PLDictionary*) plist getObjectForKey:"video_options"]);
		video_options = (Group *) ret.pointer_val;
	}

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
int (int x, int y)
DRAW_audio_options =
{
	[audio_options setBasePos:x y:y];
	[audio_options draw];

	return 1;
};

int (int key, int unicode, int down)
KEY_audio_options =
{
	return [audio_options keyEvent:key unicode:unicode down:down];
}

/*
	MENU_audio_options

	Makes the audio menu
*/
void
MENU_audio_options (PLItem *plist)
{
	local @param ret;
	Menu_Begin (54, 60, "Audio");
	Menu_FadeScreen (1);
	Menu_Draw (DRAW_audio_options);
	Menu_KeyEvent (KEY_audio_options);

	if (plist) {
		ret = object_from_plist ([(PLDictionary*) plist getObjectForKey:"audio_options"]);
		audio_options = (Group *) ret.pointer_val;
	}

	Menu_End ();
};

/************************
 * CONTROL OPTIONS
 * Control setting code
 ************************/

/*
	DRAW_control_options

	Draws the control option menu
*/
int (int x, int y)
DRAW_control_options =
{
	[control_options setBasePos:x y:y];
	[control_options draw];

	return 0;
};

int (int key, int unicode, int down)
KEY_control_options =
{
	return [control_options keyEvent:key unicode:unicode down:down];
}

/*
	MENU_control_options

	Menu make function for control options
*/
void
MENU_control_options (PLItem *plist)
{
	local @param ret;
	Menu_Begin (54, 40, "Controls");
	Menu_FadeScreen (1);
	Menu_Draw (DRAW_control_options);
	Menu_KeyEvent (KEY_control_options);

	if (plist) {
		ret = object_from_plist ([(PLDictionary*) plist getObjectForKey:"control_options"]);
		control_options = (Group *) ret.pointer_val;
	}

	MENU_control_binding (); //FIXME how to hook in the bindings menu?
	Menu_End ();
};

/***********************************************
 * FEATURES OPTIONS
 * Code of settings for special features of QF
 ***********************************************/

/*
	DRAW_feature_options

	Draws the feature option menu
*/
int (int x, int y)
DRAW_feature_options =
{
	local int cursor_pad = 0, spacing = 120;

	[feature_options setBasePos:x y:y];
	[feature_options draw];

	return 1;
};

int (int key, int unicode, int down)
KEY_feature_options =
{
	return [feature_options keyEvent:key unicode:unicode down:down];
}

/*
	MENU_feature_options

	Makes the feature option menu
*/
void
MENU_feature_options (PLItem *plist)
{
	local @param ret;
	Menu_Begin (54, 70, "Features");
	Menu_FadeScreen (1);
	Menu_CenterPic (160, 4, "gfx/p_option.lmp");
	Menu_Draw (DRAW_feature_options);
	Menu_KeyEvent (KEY_feature_options);

	if (plist) {
		ret = object_from_plist ([(PLDictionary*) plist getObjectForKey:"feature_options"]);
		feature_options = (Group *) ret.pointer_val;
	}

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
int player_config_cursor_tbl[NUM_PLAYERCONFIG_CMDS] = {
	PLAYER_CONF_Y_PAD + 8,
	PLAYER_CONF_Y_PAD + 20 + 8,
	PLAYER_CONF_Y_PAD + 45,
	PLAYER_CONF_Y_PAD + 60
};

int player_config_cursor;

// array, which holds commands for this menu
string player_config_vals[NUM_PLAYERCONFIG_CMDS] = {
	"",
	"",
	"topcolor",
	"bottomcolor"
};


int (int key, int unicode, int down)
KEY_player_options =
{
	return [player_options keyEvent:key unicode:unicode down:down];
};

/*
	DRAW_player_options

	Draws the player option menu
*/
int (int x, int y)
DRAW_player_options =
{
	[player_options setBasePos:x y:y];
	[player_options draw];

	return 1;
};

/*
	MENU_player_options

	Makes the player option menu
*/
void
MENU_player_options (PLItem *plist)
{
	local @param ret;
	Menu_Begin (54, 80, "Player");
	Menu_FadeScreen (1);
	Menu_Draw (DRAW_player_options);
	Menu_KeyEvent (KEY_player_options);

	// setup cvar aliases so the player options can refer to the appropriate
	// cvar using just the one name
	if (gametype () == "quakeworld") {
		Cvar_MakeAlias ("menu_name", "name");
	} else {
		Cvar_MakeAlias ("menu_name", "_cl_name");
	}
	// FIXME: is this something else in netquake?
	Cvar_MakeAlias ("menu_team", "team");


	if (plist) {
		ret = object_from_plist ([(PLDictionary*) plist getObjectForKey:"player_options"]);
		player_options = (Group *) ret.pointer_val;
	}

	Menu_End ();
};

/*****************************************************************************
 * NETWORK OPTIONS
 * Options, which have to do with network stuff (rate, noskins, netgraph, ...)
 *****************************************************************************/

int network_config_cursor;

// Y padding for the player config
#define NETWORK_CONF_Y_PAD 60

// table for cursor-positions
#define NUM_NETWORKCONFIG_CMDS 1
int network_config_cursor_tbl[NUM_NETWORKCONFIG_CMDS] = {
	PLAYER_CONF_Y_PAD + 8,
};

int network_config_cursor;

// array, which holds commands for this menu
string network_config_vals[NUM_NETWORKCONFIG_CMDS] = {
	"",
};


int (int key, int unicode, int down)
KEY_network_options =
{
	return [network_options keyEvent:key unicode:unicode down:down];
};

/*
	DRAW_network_options

	Draws the network option menu
*/
int (int x, int y)
DRAW_network_options =
{
	[network_options setBasePos:x y:y];
	[network_options draw];
	return 1;
};

/*
	MENU_network_options

	Makes the network option menu
*/
void
MENU_network_options (PLItem *plist)
{
	local @param ret;
	Menu_Begin (54, 90, "Network");
	Menu_FadeScreen (1);
	Menu_KeyEvent (KEY_network_options);
	Menu_Draw (DRAW_network_options);

	if (plist) {
		ret = object_from_plist ([(PLDictionary*) plist getObjectForKey:"network_options"]);
		network_options = (Group *) ret.pointer_val;
	}

	Menu_End ();
};

int (string text, int key)
op_goto_console =
{
	Menu_SelectMenu ("");
	Cbuf_AddText (nil, "toggleconsole\n");
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
	local int	spacing = 120;
	local PLItem    *plist;

	plist = read_plist ("menu.plist");

	Menu_Begin (54, 72, "");
	Menu_FadeScreen (1);
	Menu_Pic (16, 4, "gfx/qplaque.lmp");
	Menu_CenterPic (160, 4, "gfx/p_option.lmp");

	Menu_Item (54, 32, "Go to Console", op_goto_console, 0);
	MENU_control_options (plist);
	MENU_video_options (plist);
	MENU_audio_options (plist);
	MENU_feature_options (plist);
	MENU_player_options (plist);
	MENU_network_options (plist);

	Menu_End ();
};
