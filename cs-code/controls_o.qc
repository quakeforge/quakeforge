#define NUM_BINDED_KEYS 3
string [NUM_BINDED_KEYS] key_bindings = 
{
	"+attack",
	"impulse 10",
	"+jump"
};
string [NUM_BINDED_KEYS] key_bindings_desc = 
{
	"none",
	"none",
	"none"
};

integer set_key_flag;

string (integer key, integer bindnum) make_key_desc =
{
	local integer keynum;
	local string keyname;

	keynum = Key_LookupBinding(IMT_0, bindnum, key_bindings[key]);
	if(keynum == -1) {
		keyname = "none";
	} else {
		keyname = Key_KeynumToString(keynum);
		// cut away the "K_"
		keyname = String_Cut(0, 2, keyname);
	}
	return keyname;
};

void () load_keybindings =
{
	local integer i, keynum;
	
	for(i = 0;i < NUM_BINDED_KEYS; i++) {
		key_bindings_desc[i] = make_key_desc(i, 1) + "," + make_key_desc(i, 2);
	}
};

integer (string text, integer key) control_bind_f =
{
	local string binding;
	local integer retval = 0, bindcnt = 0;

	switch (text) {
		case "set_attack_key":
			binding = key_bindings[0];
			break;
		case "set_next_weap_key":
			binding = key_bindings[1];
			break;
		case "set_jump_key":
			binding = key_bindings[2];
			break;
	}

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

integer () control_bind_draw = 
{
	local integer cursor_pad = 50, bind_desc_pad;

	bind_desc_pad = 120;

	Draw_String		(20, 20, "Backspace/Delete: Del binding");
	Draw_String     (20, 30, "Enter:            New binding");

	draw_val_item   (20, 50, bind_desc_pad, "Attack", key_bindings_desc[0]);
	draw_val_item   (20, 60, bind_desc_pad, "Next weapon", key_bindings_desc[1]);
	draw_val_item   (20, 70, bind_desc_pad, "Jump/Swin up", key_bindings_desc[2]);

	opt_cursor  (12, (Menu_GetIndex() * 10) + cursor_pad);

	return 1;
};

void () control_bind_menu =
{
	Menu_Begin (54, 90, "Bindings");
	Menu_FadeScreen (1);
	Menu_Draw (control_bind_draw);

	Menu_Item (20, 50, "set_attack_key", control_bind_f, 1);
	Menu_Item (20, 60, "set_next_weap_key", control_bind_f, 1);
	Menu_Item (20, 70, "set_jump_key", control_bind_f, 1);
	Menu_End ();
};

