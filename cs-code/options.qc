#define MIN_GAMMA 0.4
#define MAX_GAMMA 3
#define GAMMA_STEP 0.1


//**** VIDEO OPTIONS

integer (string text, integer key) video_gamma_f =
{
	local float gamma;

	switch (text) {
		case "gamma":
			gamma = cvar("vid_gamma");
			if(key == QFK_RIGHT) {
				gamma += GAMMA_STEP;
			} else if(key == QFK_LEFT) {
				gamma -= GAMMA_STEP;
			}

			if(gamma > MAX_GAMMA) {
				gamma = MIN_GAMMA;
			} else if(gamma < MIN_GAMMA) {
				gamma = MAX_GAMMA;
			}
			cvar_set("vid_gamma", ftos(gamma));
			break;
	}
	return 0;
};

integer (string text, integer key) video_options_f =
{
	local integer selected_crosshair;

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
	}
	return 0;
};

integer () gamma_to_percentage =
{
	local float gamma = cvar("vid_gamma");
	local integer gam_perc;

	local float max_gamma = (MAX_GAMMA-MIN_GAMMA) * 100;
	gamma -= MIN_GAMMA;	

	if(gamma > (MAX_GAMMA-MIN_GAMMA)) {
		gamma = (MAX_GAMMA-MIN_GAMMA);
	}
	if(gamma < 0) {
		gamma = 0;
	}
	gamma *= 100;

	gam_perc = ftoi((gamma/max_gamma) * 100);

	return gam_perc;
};

integer () options_video_draw = 
{
	local string tmp = ftos(cvar("crosshair"));
	Draw_Pic (16, 4, "gfx/qplaque.lmp");
	Draw_CenterPic (160, 4, "gfx/p_option.lmp");
	Draw_String	(54,  40, "Video");
	Draw_String	(54,  50, "-----");
	Draw_String	(70, 60, "Fullscreen: " + get_cvar_state("vid_fullscreen"));
	Draw_String	(70, 70, "Crosshair: " + tmp);

	Draw_String (70, 80, "Gamma:");
	draw_perc_bar  (118,  80, 15, gamma_to_percentage());
	Draw_String (118 + (15 + 4)*8 , 80, ftos(cvar("vid_gamma")));
	opt_cursor  (62, (Menu_GetIndex() * 10) + 60);
	return 1;
};

void () options_video_menu =
{
	Menu_Begin (54, 50, "Video");
	Menu_FadeScreen (1);
	Menu_Draw (options_video_draw);
	
	Menu_Item (54, 60, "fullscreen", video_options_f, 0);
	Menu_Item (54, 70, "crosshair", video_options_f, 0);
	Menu_Item (54, 80, "gamma", video_gamma_f, 1);
	Menu_End ();
};


//**** CONTROL OPTIONS

integer (string text, integer key) control_options_f =
{
	switch (text) {
	    case "in_grab":	    
			Cbuf_AddText ("toggle in_grab\n");
			return 0;
		case "autorun":
			if(cvar("cl_forwardspeed") < 400) {
				Cbuf_AddText ("set cl_forwardspeed 400\n");
				Cbuf_AddText ("set cl_backspeed 400\n");
			} else {
				Cbuf_AddText ("set cl_forwardspeed 200\n");
				Cbuf_AddText ("set cl_backspeed 200\n");
			}
			return 0;
	}
};

integer (integer key, integer unicode, integer down) options_controls_keyevent =
{
	load_keybindings();
	return 0;
};


integer () options_controls_draw = 
{
	local string tmp;
	local integer cursor_pad = 0;


	Draw_Pic		(16, 4, "gfx/qplaque.lmp");
	Draw_CenterPic (160, 4, "gfx/p_option.lmp");
	Draw_String		(54,  40, "Controls");
	Draw_String		(54,  50, "--------");
	Draw_String     (70,  60, "Grab mouse: " + get_cvar_state ("in_grab"));
	tmp = "On";
	if(cvar("cl_forwardspeed") < 400) {
		tmp = "Off";
	}
	Draw_String     (70,  70, "Auto run: " + tmp);
	Draw_String     (70,  80, "Bindings");
	opt_cursor  (62, (Menu_GetIndex() * 10) + 60 + cursor_pad);

	return 1;
};

void () options_controls_menu =
{
	Menu_Begin (54, 40, "Controls");
	Menu_FadeScreen (1);
	Menu_CenterPic (160, 4, "gfx/p_option.lmp");
	Menu_Draw (options_controls_draw);
	Menu_KeyEvent (options_controls_keyevent);
	Menu_Item (54, 60, "in_grab", control_options_f, 0);
	Menu_Item (54, 70, "autorun", control_options_f, 0);
	control_bind_menu ();
	Menu_End ();
};


integer (integer key, integer unicode, integer down) options_keyevent =
{
	// pre-loading of the bindings and set_key_flag == 0
	load_keybindings();	
	set_key_flag = 0;
	return 0;
};

void () options_menu =
{
	Menu_Begin (54, 72, "");
	Menu_FadeScreen (1);
	Menu_Pic (16, 4, "gfx/qplaque.lmp");
	Menu_CenterPic (160, 4, "gfx/p_option.lmp");
	Menu_KeyEvent (options_keyevent);
	options_video_menu ();
	options_controls_menu ();
	Menu_End ();
};
