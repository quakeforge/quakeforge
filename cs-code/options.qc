float time;
#define MIN_GAMMA 0.4
#define MAX_GAMMA 3
#define GAMMA_STEP 0.1

integer set_key_flag;

void (integer x, integer y) opt_cursor =
{
	Draw_Character (x, y, 12 + (integer (time * 4) & 1));
};

void (string text, integer key) video_options_f =
{
	local integer selected_crosshair;
	local float gamma;
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
		case "gamma":
			// FIX KEYS
			gamma = cvar("vid_gamma");
			gamma += GAMMA_STEP;
/*			if(key == QFK_RIGHT) {
				gamma += GAMMA_STEP;
			} else if(key == QFK_LEFT) {
				gamma -= GAMMA_STEP;
			}
*/
			if(gamma > MAX_GAMMA) {
				gamma = MIN_GAMMA;
//			} else if(gamma < MIN_GAMMA) {
//				gamma = MIN_GAMMA;
			}
			cvar_set("vid_gamma", ftos(gamma));
			break;
	}
};
void (string text, integer key) control_options_f =
{
	switch (text) {
	    case "in_grab":	    
			Cbuf_AddText ("toggle in_grab\n");
			return;
		case "autorun":
			if(cvar("cl_forwardspeed") < 400) {
				Cbuf_AddText ("set cl_forwardspeed 400\n");
				Cbuf_AddText ("set cl_backspeed 400\n");
			} else {
				Cbuf_AddText ("set cl_forwardspeed 200\n");
				Cbuf_AddText ("set cl_backspeed 200\n");
			}
			return;
		case "set_key":
			if(set_key_flag) {
				set_key_flag = 0;
			} else {
				set_key_flag = 1;
			}
			break;
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


void (integer x, integer y, integer perc_val) draw_perc_bar =
{
	local integer perc;
	if(perc_val > 100) {
		perc_val = 100;
	} else if(perc_val < 0) {
		perc_val = 0;
	}
	perc = perc_val / 5;
	Draw_String (x, y, "[---------------------]");
    Draw_Character (x + ((perc + 1) * 8), y, '*');
};

#define NUM_BINDED_KEYS 3
string [NUM_BINDED_KEYS] key_bindings = {
	"+attack",
	"impulse 10",
	"+jump"
};
string [NUM_BINDED_KEYS] key_bindings_keys = {
	"none",
	"none",
	"none"
};

void () load_keybindings =
{
	local integer i, keynum;
	
	for(i = 0;i < NUM_BINDED_KEYS; i++) {
		keynum = Key_LookupBinding(IMT_0, key_bindings[i]);
		if(keynum == -1) {
			key_bindings_keys[i] = "<none>";
		} else {
			key_bindings_keys[i] = Key_KeynumToString(keynum);
		}
	}
};

integer (integer key, integer unicode, integer down) options_controls_keyevent =
{
	local integer tmp;

	if(set_key_flag) {
		tmp = Menu_GetIndex () - 2;				
		Key_SetBinding (IMT_0, key, key_bindings[tmp]);
		set_key_flag = 0;

		load_keybindings();
		return 1;
	}
	if((key == QFK_BACKSPACE) && (Menu_GetIndex () > 1)) {
		Key_SetBinding (IMT_0, Key_LookupBinding(IMT_0, key_bindings[Menu_GetIndex () - 2]), "");
	}
		
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
	Draw_String		(54,  80, "Bindings");
	Draw_String		(70,  90, "Attack: " + key_bindings_keys[0]);
	Draw_String		(70,  100, "Next weapon: " + key_bindings_keys[1]);
	Draw_String		(70,  110, "Jump/Swim up: " + key_bindings_keys[2]);
	Draw_String		(70,  130, "Backspace => Del binding");
	Draw_String     (70,  140, "Enter => New binding");

	if(Menu_GetIndex() > 1) {
		cursor_pad = 10;
	}
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
	Menu_Item (54, 60, "in_grab", control_options_f);
	Menu_Item (54, 70, "autorun", control_options_f);
	Menu_Item (54, 90, "set_key", control_options_f);
	Menu_Item (54, 100, "set_key", control_options_f);
	Menu_Item (54, 110, "set_key", control_options_f);
	Menu_End ();
};


#define NUM_VIDEOCONFIG_CMDS 3
integer [NUM_VIDEOCONFIG_CMDS] videoConfig_cursor_table = { 60, 70, 80 };

integer (integer key, integer unicode, integer down) options_video_keyevent =
{
	if (key == QFK_RETURN) {
	} else if(key == QFK_RIGHT) {
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

	Draw_String (70, 80, "Gamma:");
	draw_perc_bar  (118,  80, gamma_to_percentage());
	opt_cursor  (62, (Menu_GetIndex() * 10) + 60);
	return 1;
};

void () options_video_menu =
{
	Menu_Begin (54, 50, "Video");
	Menu_FadeScreen (1);
	Menu_Draw (options_video_draw);
	Menu_KeyEvent (options_video_keyevent);

	Menu_Item (54, 60, "fullscreen", video_options_f);
	Menu_Item (54, 70, "crosshair", video_options_f);
	Menu_Item (54, 80, "gamma", video_options_f);
	Menu_End ();
};

integer (integer key, integer unicode, integer down) options_keyevent =
{
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
