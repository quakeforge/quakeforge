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

/* 
	some definitions of border values for
	different things
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
	local integer selected_crosshair;
	local float val;

	switch (text) {
		case "fullscreen":
			Cbuf_AddText ("toggle vid_fullscreen\n");
			break;
		case "crosshair":
			selected_crosshair = ftoi(cvar("crosshair"));
			selected_crosshair++;
			if(selected_crosshair >= 3) {
			    selected_crosshair = 0;
			}
			cvar_set("crosshair", itos(selected_crosshair));
			break;
		case "fps":
			Cbuf_AddText ("toggle show_fps\n");
			break;
		case "time":
			Cbuf_AddText ("toggle show_time\n");
			break;
	}

	if(!(key == QFK_RIGHT || key == QFK_LEFT )) {
		return 0;
	}
	switch (text) {
		case "gamma":
			val = cvar("vid_gamma");
			val = min_max_cnt(MIN_GAMMA, MAX_GAMMA, GAMMA_STEP, val, 
				(key == QFK_RIGHT) && (key != QFK_LEFT));
			cvar_set("vid_gamma", ftos(val));
			break;
		case "viewsize":
			val = cvar("viewsize");
			val = min_max_cnt(MIN_VIEWSIZE, MAX_VIEWSIZE, VIEWSIZE_STEP, val, 
				(key == QFK_RIGHT) && (key != QFK_LEFT));
			cvar_set("viewsize", ftos(val));
			break;
	}
	return 0;
};

/*
	DRAW_video_options

	Drawing function for the video options menu
*/
integer () 
DRAW_video_options = 
{
	local integer spacing = 120;
	local integer bar_pad;

	Draw_Pic (16, 4, "gfx/qplaque.lmp");
	Draw_CenterPic (160, 4, "gfx/p_option.lmp");
	Draw_String	(54,  40, "Video");
	Draw_String	(54,  50, "-----");
	draw_val_item   (70, 60, spacing, "Fullscreen", 
		cvar("vid_fullscreen") ? "On" : "Off");
	draw_val_item   (70, 70, spacing, "Crosshair", ftos(cvar("crosshair")));
	draw_val_item   (70, 80, spacing, "Show fps", 
		cvar("show_fps") ? "On" : "Off");
	draw_val_item   (70, 90, spacing, "Show time", 
		cvar("show_time") ? "On" : "Off");
	bar_pad = 90;

	Draw_String (70, bar_pad + 10, "Gamma:");
	draw_perc_bar  (118,  bar_pad + 10, 15, 
		to_percentage (MIN_GAMMA, MAX_GAMMA, cvar("vid_gamma")));
	Draw_String (118 + (15 + 4)*8 , bar_pad + 10, ftos(cvar("vid_gamma")));

	Draw_String (70, bar_pad + 20, "Viewsize:");
	draw_perc_bar  (142,  bar_pad + 20, 12, 
		to_percentage (MIN_VIEWSIZE, MAX_VIEWSIZE, cvar("viewsize")));
	Draw_String (142 + (12 + 4)*8 , bar_pad + 20, ftos(cvar("viewsize")));

	opt_cursor  (62, (Menu_GetIndex() * 10) + 60);
	return 1;
};

/*
	MENU_video_options

	Menu function for the video options menu.
*/
void () 
MENU_video_options =
{
	local integer bar_pad;

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
	local float volume;

	volume = cvar("volume");
	volume = min_max_cnt(MIN_VOLUME, MAX_VOLUME, VOLUME_STEP, volume, 
		(key == QFK_RIGHT) && (key != QFK_LEFT));
	cvar_set("volume", ftos(volume));

	return 0;
};

/*
	DRAW_audio_options

	Draws the audio options menu
*/
integer () 
DRAW_audio_options = 
{
	local string tmp = ftos(cvar("crosshair"));
	local integer spacing = 120;
	local integer bar_pad;

	Draw_Pic (16, 4, "gfx/qplaque.lmp");
	Draw_CenterPic (160, 4, "gfx/p_option.lmp");
	Draw_String	(54,  40, "Audio");
	Draw_String	(54,  50, "-----");
	bar_pad = 50;

	Draw_String (70, bar_pad + 10, "Volume:");
	draw_perc_bar  (126,  bar_pad + 10, 15, 
		to_percentage (MIN_VOLUME, MAX_VOLUME, cvar("volume")) );
	Draw_String (126 + (15 + 4)*8 , bar_pad + 10, ftos(cvar("volume")));

	opt_cursor  (62, (Menu_GetIndex() * 10) + 60);
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
integer () 
DRAW_control_options = 
{
	local integer cursor_pad = 0, spacing = 120, bar_pad;

	Draw_Pic		(16,  4,  "gfx/qplaque.lmp");
	Draw_CenterPic  (160, 4,  "gfx/p_option.lmp");
	Draw_String		(54,  40, "Controls");
	Draw_String		(54,  50, "--------");
	Draw_String     (70,  60, "Bindings");

	draw_val_item   (70,  70, spacing, "Grab mouse",   cvar("in_grab") ? "On" : "Off");
	draw_val_item   (70,  80, spacing, "Auto run",     cvar("cl_forwardspeed") < 400 ? "Off" : "On");
	draw_val_item   (70,  90, spacing, "Mouse Invert", cvar("m_pitch") < 0 ? "On" : "Off");
	bar_pad = 90;

	Draw_String     (70,  bar_pad + 10, "Mouse amp:");
	draw_perc_bar   (150, bar_pad + 10, 12, to_percentage (MIN_MOUSE_AMP, MAX_MOUSE_AMP, cvar("in_mouse_amp")));
	Draw_String     (150 + (12 + 4)*8 , bar_pad + 10, ftos(cvar("in_mouse_amp")));

	draw_val_item   (70,  110, spacing, "Freelook",   cvar("freelook") ? "On" : "Off");
	draw_val_item   (70,  120, spacing, "Lookspring", cvar("lookspring") ? "On" : "Off");
	draw_val_item   (70,  130, spacing, "Lookstrafe", cvar("lookstrafe") ? "On" : "Off");

	opt_cursor  (62, (Menu_GetIndex() * 10) + 60 + cursor_pad);

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
integer () 
DRAW_feature_options = 
{
	local integer cursor_pad = 0, spacing = 120;

	Draw_Pic		(16, 4, "gfx/qplaque.lmp");
	Draw_CenterPic  (160,4, "gfx/p_option.lmp");
	Draw_String		(54,  40, "Features");
	Draw_String		(54,  50, "--------");

	draw_val_item (70, 60, spacing, "Auto Record", 
		cvar("cl_autorecord") != 0 ? "On" : "Off");
	draw_val_item (70, 70, spacing, "Fraglogging", 
		cvar("cl_fraglog") != 0 ? "On" : "Off");

	opt_cursor  (62, (Menu_GetIndex() * 10) + 60 + cursor_pad);
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

/*************************
 * MAIN OPTIONS 
 * Main options menu code
 *************************/

/*
	CB_options

	Callback for main options menu
*/
integer (integer key, integer unicode, integer down) 
CB_options =
{
	// pre-loading of the bindings and set_key_flag == 0
	set_key_flag = 0;

	return 0;
};


/*
	MENU_options

	Makes the main options menu
*/
void () 
MENU_options =
{
	local integer spacing = 120;

	Menu_Begin (54, 72, "");
	Menu_FadeScreen (1);
	Menu_Pic (16, 4, "gfx/qplaque.lmp");
	Menu_CenterPic (160, 4, "gfx/p_option.lmp");
	Menu_KeyEvent (CB_options);

	MENU_control_options ();
	MENU_video_options ();
	MENU_audio_options ();
	MENU_feature_options ();

	Menu_End ();
};
