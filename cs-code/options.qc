float time;

void (integer x, integer y) opt_cursor =
{
	Draw_Character (x, y, 12 + (integer (time * 4) & 1));
};

void (string text, integer key) video_options_f =
{
	local integer selected_crosshair;

	switch (text) {
		case "fullscreen":
			Cbuf_AddText ("toggle vid_fullscreen\n");
				return;
		case "crosshair":
			selected_crosshair = ftoi(cvar("crosshair"));
			selected_crosshair++;
			if(selected_crosshair >= 3) {
			    selected_crosshair = 0;
			}
			cvar_set("crosshair", itos(selected_crosshair));
	}
};
void (string text, integer key) control_options_f =
{
	switch (text) {
	    case "in_grab":	    
		Cbuf_AddText ("toggle in_grab\n");
		return;
	}
};


/* FIXME - how to catch a key ?
void (string text, integer key) set_key_f = 
{
	Cbuf_AddText ("in_bind IMT_0 ");
	Cbuf_AddText (itos(key));
	Cbuf_AddText (" \"+jump\"\n");
};
*/

// ********* OPTIONS

string (string cvarstr) get_cvar_state =
{
	if(cvar(cvarstr)) {
		return("On");
	} else {
		return("Off");
	}
};

integer (integer key, integer unicode, integer down) options_controls_keyevent =
{
	if (key == QFK_RETURN) {
	} else {
	}
	return 0;
};

integer () options_controls_draw = 
{
	//local string tmp;

	Draw_Pic (16, 4, "gfx/qplaque.lmp");
	Draw_CenterPic (160, 4, "gfx/p_option.lmp");
	Draw_String	(54,  40, "Controls");
	Draw_String	(54,  50, "--------");
	Draw_String     (70,  60, "Grab mouse: " + get_cvar_state ("in_grab"));
	opt_cursor  (62, (Menu_GetIndex() * 10) + 60);
	return 1;
};

void () options_controls_menu =
{
	Menu_Begin (54, 40, "Controls");
	Menu_CenterPic (160, 4, "gfx/p_option.lmp");
	Menu_Draw (options_controls_draw);
	Menu_KeyEvent (options_controls_keyevent);
	Menu_Item (54, 60, "in_grab", control_options_f);
	Menu_End ();
};

integer (integer key, integer unicode, integer down) options_video_keyevent =
{
	if (key == QFK_RETURN) {
	} else {
	}
	// i have a question ;)
	return 0;
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
	opt_cursor  (62, (Menu_GetIndex() * 10) + 60);
	return 1;
};

void () options_video_menu =
{
	Menu_Begin (54, 50, "Video");
	Menu_Draw (options_video_draw);
	Menu_KeyEvent (options_video_keyevent);
	Menu_Item (54, 60, "fullscreen", video_options_f);
	Menu_Item (54, 70, "crosshair", video_options_f);
	Menu_End ();
};

void () options_menu =
{
	Menu_Begin (54, 72, "");
	Menu_Pic (16, 4, "gfx/qplaque.lmp");
	Menu_CenterPic (160, 4, "gfx/p_option.lmp");
	options_video_menu ();
	options_controls_menu ();
	Menu_End ();
};
