#define NUM_BINDED_KEYS 19
string [NUM_BINDED_KEYS] key_bindings = 
{
	"+attack",
	"impulse 10",
	"+jump",
	"+forward",
	"+back",
	"+left",
	"+right",
	"+speed",
	"+moveleft",
	"+moveright",
	"+strafe",
	"+loookup",
	"+lookdown",
	"centerview",
	"+mlook",
	"+klook",
	"+moveup",
	"+movedown",
	"toggle in_grab"
};
string [NUM_BINDED_KEYS] key_bindings_desc;

integer set_key_flag;

string (integer key, integer bindnum) make_key_desc =
{
	local integer keynum;
	local string keyname;

	keynum = Key_LookupBinding(IMT_0, bindnum, key_bindings[key]);
	if(keynum == -1) {
		keyname = "";
	} else {
		keyname = Key_KeynumToString(keynum);
		// cut away the "K_"
		keyname = String_Cut(0, 2, keyname);
	}
	return keyname;
};

void () load_keybindings =
{
	local integer i;
	local string nddesc = "";
	
	for(i = 0;i < NUM_BINDED_KEYS; i++) {
		key_bindings_desc[i] = make_key_desc(i, 1);
		nddesc = make_key_desc(i, 2);
		if(nddesc != "") {
			key_bindings_desc[i] += ", " + nddesc;
		}
	}
};

integer (string text, integer key) control_bind_f =
{
	local string binding;
	local integer retval = 0, bindcnt = 0;

	binding = key_bindings[stoi(text)];

	if(set_key_flag) {
		bindcnt = Key_CountBinding(IMT_0, binding);
		if(bindcnt < 2) {
			Key_SetBinding (IMT_0, key, binding);
		}
		// else: not bind

		set_key_flag = 0;
		retval = 1;
	} else {
		if(key == QFK_RETURN) {
			if(Key_CountBinding(IMT_0, binding) < 2)
				set_key_flag = 1;
			retval = 1;
		} else if(key == QFK_BACKSPACE || key == QFK_DELETE) {
			Key_SetBinding (IMT_0, Key_LookupBinding(IMT_0, 1, binding), "");

			retval = 1;
		}
	}

	load_keybindings();
	return retval;
};

integer () basic_control_bind_draw = 
{
	local integer cursor_pad = 40, bind_desc_pad;

	bind_desc_pad = 120;

	Draw_String		(20, 10, "Backspace/Delete: Del binding");
	Draw_String     (20, 20, "Enter:            New binding");

	draw_val_item   (20, 40, bind_desc_pad, "Attack", key_bindings_desc[0]);
	draw_val_item   (20, 50, bind_desc_pad, "Next weapon", key_bindings_desc[1]);
	draw_val_item   (20, 60, bind_desc_pad, "Jump/Swin up", key_bindings_desc[2]);
	draw_val_item   (20, 70, bind_desc_pad, "Walf forward", key_bindings_desc[3]);
	draw_val_item   (20, 80, bind_desc_pad, "Backpedal", key_bindings_desc[4]);
	draw_val_item   (20, 90, bind_desc_pad, "Turn left", key_bindings_desc[5]);
	draw_val_item   (20, 100, bind_desc_pad, "Turn right", key_bindings_desc[6]);
	draw_val_item   (20, 110, bind_desc_pad, "Run", key_bindings_desc[7]);
	draw_val_item   (20, 120, bind_desc_pad, "Step left", key_bindings_desc[8]);
	draw_val_item   (20, 130, bind_desc_pad, "Step right", key_bindings_desc[9]);
	draw_val_item   (20, 140, bind_desc_pad, "Sidestep", key_bindings_desc[10]);
	draw_val_item   (20, 150, bind_desc_pad, "Look up", key_bindings_desc[11]);
	draw_val_item   (20, 160, bind_desc_pad, "Look down", key_bindings_desc[12]);
	draw_val_item   (20, 170, bind_desc_pad, "Center view", key_bindings_desc[13]);
	draw_val_item   (20, 180, bind_desc_pad, "Mouse look", key_bindings_desc[14]);
	draw_val_item   (20, 190, bind_desc_pad, "Keyboard look", key_bindings_desc[15]);
	draw_val_item   (20, 200, bind_desc_pad, "Swim up", key_bindings_desc[16]);
	draw_val_item   (20, 210, bind_desc_pad, "Swim down", key_bindings_desc[17]);

	opt_cursor  (12, (Menu_GetIndex() * 10) + cursor_pad);

	return 1;
};

integer () misc_control_bind_draw = 
{
	local integer cursor_pad = 40, bind_desc_pad;

	bind_desc_pad = 120;

	Draw_String		(20, 10, "Backspace/Delete: Del binding");
	Draw_String     (20, 20, "Enter:            New binding");

	draw_val_item   (20, 40, bind_desc_pad, "Tog. m.-grab", key_bindings_desc[18]);

	opt_cursor  (12, (Menu_GetIndex() * 10) + cursor_pad);

	return 1;
};

void () basic_control_bind_menu =
{
	local integer i;
	
	Menu_Begin (54, 40, "Basic bindings");
	Menu_FadeScreen (1);
	Menu_Draw (basic_control_bind_draw);

	for (i = 0; i < 18; i++) {
		Menu_Item (20, 40 + i*10, itos(i), control_bind_f, 1);
	}
	Menu_End ();
};

void () misc_control_bind_menu =
{
	local integer i;

	Menu_Begin (54, 50, "Misc bindings");
	Menu_FadeScreen (1);
	Menu_Draw (misc_control_bind_draw);

	for (i = 18; i < 19; i++) {
		Menu_Item (20, 40 + i*10, itos(i), control_bind_f, 1);
	}
	Menu_End ();
};


void () control_bind_menu =
{
	Menu_Begin (54, 90, "Bindings");
	Menu_Pic (16, 4, "gfx/qplaque.lmp");
	Menu_CenterPic (160, 4, "gfx/p_option.lmp");
	Menu_FadeScreen (1);
	basic_control_bind_menu ();
	misc_control_bind_menu ();
	Menu_End ();
};

