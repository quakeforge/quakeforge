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

//**** VIDEO OPTIONS

integer (string text, integer key) video_options_f =
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
			val = min_max_cnt(MIN_GAMMA, MAX_GAMMA, GAMMA_STEP, val, (key == QFK_RIGHT) && (key != QFK_LEFT));
			cvar_set("vid_gamma", ftos(val));
			break;
		case "viewsize":
			val = cvar("viewsize");
			val = min_max_cnt(MIN_VIEWSIZE, MAX_VIEWSIZE, VIEWSIZE_STEP, val, (key == QFK_RIGHT) && (key != QFK_LEFT));
			cvar_set("viewsize", ftos(val));
			break;
	}
	return 0;
};

integer () options_video_draw = 
{
	local string tmp = ftos(cvar("crosshair"));
	local integer spacing = 120;
	local integer bar_pad;

	Draw_Pic (16, 4, "gfx/qplaque.lmp");
	Draw_CenterPic (160, 4, "gfx/p_option.lmp");
	Draw_String	(54,  40, "Video");
	Draw_String	(54,  50, "-----");
	draw_val_item   (70, 60, spacing, "Fullscreen", get_cvar_state ("vid_fullscreen"));
	draw_val_item   (70, 70, spacing, "Crosshair", tmp);
	draw_val_item   (70, 80, spacing, "Show fps", get_cvar_state ("show_fps"));
	draw_val_item   (70, 90, spacing, "Show time", get_cvar_state ("show_time"));
	bar_pad = 90;

	Draw_String (70, bar_pad + 10, "Gamma:");
	draw_perc_bar  (118,  bar_pad + 10, 15, to_percentage (MIN_GAMMA, MAX_GAMMA, cvar("vid_gamma")));
	Draw_String (118 + (15 + 4)*8 , bar_pad + 10, ftos(cvar("vid_gamma")));

	Draw_String (70, bar_pad + 20, "Viewsize:");
	draw_perc_bar  (142,  bar_pad + 20, 12, to_percentage (MIN_VIEWSIZE, MAX_VIEWSIZE, cvar("viewsize")));
	Draw_String (142 + (12 + 4)*8 , bar_pad + 20, ftos(cvar("viewsize")));

	opt_cursor  (62, (Menu_GetIndex() * 10) + 60);
	return 1;
};

void () options_video_menu =
{
	local integer bar_pad;

	Menu_Begin (54, 50, "Video");
	Menu_FadeScreen (1);
	Menu_Draw (options_video_draw);
	
	Menu_Item (54, 60, "fullscreen", video_options_f, 0);
	Menu_Item (54, 70, "crosshair", video_options_f, 0);
	Menu_Item (54, 80, "fps", video_options_f, 0);
	Menu_Item (54, 80, "time", video_options_f, 0);
	bar_pad = 90;

	Menu_Item (54, bar_pad + 10, "gamma", video_options_f, 1);
	Menu_Item (54, bar_pad + 20, "viewsize", video_options_f, 1);
	Menu_End ();
};

//**** AUDIO OPTIONS

integer (string text, integer key) audio_volume_f =
{
	local float volume;

	volume = cvar("volume");
	if(key == QFK_RIGHT) {
		volume += VOLUME_STEP;
	} else if(key == QFK_LEFT) {
		volume -= VOLUME_STEP;
	}

	if(volume > MAX_VOLUME) {
		volume = MIN_VOLUME;
	} else if(volume < MIN_VOLUME) {
		volume = MAX_VOLUME;
	}
	cvar_set("volume", ftos(volume));
	return 0;
};

integer () options_audio_draw = 
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
	draw_perc_bar  (126,  bar_pad + 10, 15, to_percentage (MIN_VOLUME, MAX_VOLUME, cvar("volume")));
	Draw_String (126 + (15 + 4)*8 , bar_pad + 10, ftos(cvar("volume")));

	opt_cursor  (62, (Menu_GetIndex() * 10) + 60);
	return 1;
};

void () options_audio_menu =
{
	local integer bar_pad;

	Menu_Begin (54, 60, "Audio");
	Menu_FadeScreen (1);
	Menu_Draw (options_audio_draw);
	
	bar_pad = 0;
	Menu_Item (54, bar_pad + 10, "volume", audio_volume_f, 1);
	Menu_End ();
};

//**** CONTROL OPTIONS

integer (string text, integer key) control_options_f =
{
	local float val;

	load_keybindings();

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
		case "m_pitch":
			if(cvar("m_pitch") < 0) {
				Cbuf_AddText ("set m_pitch 0.022\n");
			} else {
				Cbuf_AddText ("set m_pitch -0.022\n");
			}
			break;
	}
	if(!(key == QFK_RIGHT || key == QFK_LEFT)) {
		return 0;
	}
	switch (text) {
		case "mouseamp":
			val = cvar("in_mouse_amp");
			val = min_max_cnt(MIN_MOUSE_AMP, MAX_MOUSE_AMP, MOUSE_AMP_STEP, val, (key == QFK_RIGHT) && (key != QFK_LEFT));
			cvar_set("in_mouse_amp", ftos(val));
			break;
	}
	return 0;
};

integer () options_controls_draw = 
{
	local string tmp;
	local integer cursor_pad = 0, spacing = 120, bar_pad;


	Draw_Pic		(16, 4, "gfx/qplaque.lmp");
	Draw_CenterPic  (160,4, "gfx/p_option.lmp");
	Draw_String		(54,  40, "Controls");
	Draw_String		(54,  50, "--------");
	Draw_String     (70,  60, "Bindings");

	draw_val_item   (70,  70, spacing, "Grab mouse", get_cvar_state ("in_grab"));
	tmp = cvar("cl_forwardspeed") < 400 ? "Off" : "On";
	draw_val_item   (70,  80, spacing, "Auto run", tmp);
	draw_val_item   (70,  90, spacing, "Freelook", get_cvar_state ("freelook"));
	bar_pad = 90;

	Draw_String (70, bar_pad + 10, "Mouse amp:");
	draw_perc_bar  (150,  bar_pad + 10, 12, to_percentage (MIN_MOUSE_AMP, MAX_MOUSE_AMP, cvar("in_mouse_amp")));
	Draw_String (150 + (12 + 4)*8 , bar_pad + 10, ftos(cvar("in_mouse_amp")));
	
	tmp = cvar("m_pitch") < 0 ? "On" : "Off";
	draw_val_item (70, 110, spacing, "Mouse Invert", tmp);

	opt_cursor  (62, (Menu_GetIndex() * 10) + 60 + cursor_pad);
	return 1;
};

void () options_controls_menu =
{
	Menu_Begin (54, 40, "Controls");
	Menu_FadeScreen (1);
	Menu_CenterPic (160, 4, "gfx/p_option.lmp");
	Menu_Draw (options_controls_draw);
	control_bind_menu ();
	Menu_Item (54, 70, "in_grab", control_options_f, 0);
	Menu_Item (54, 80, "autorun", control_options_f, 0);
	Menu_Item (54, 90, "freelook", control_options_f, 0);
	Menu_Item (54, 100, "mouseamp", control_options_f, 1);
	Menu_Item (54, 110, "m_pitch", control_options_f, 0);
	Menu_End ();
};


integer (integer key, integer unicode, integer down) options_keyevent =
{
	// pre-loading of the bindings and set_key_flag == 0
	load_keybindings();	
	set_key_flag = 0;
	return 0;
};

//**** MAIN OPTIONS MENU

void () options_menu =
{
	Menu_Begin (54, 72, "");
	Menu_FadeScreen (1);
	Menu_Pic (16, 4, "gfx/qplaque.lmp");
	Menu_CenterPic (160, 4, "gfx/p_option.lmp");
	Menu_KeyEvent (options_keyevent);
	options_controls_menu ();
	options_video_menu ();
	options_audio_menu ();
	Menu_End ();
};
