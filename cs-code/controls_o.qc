#define NUM_BINDED_KEYS 3
string [NUM_BINDED_KEYS] key_bindings = 
{
	"+attack",
	"impulse 10",
	"+jump"
};
string [NUM_BINDED_KEYS] key_bindings_keys = 
{
	"none",
	"none",
	"none"
};

integer set_key_flag;

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

integer (string text, integer key) control_bind_f =
{
	local string binding;
	local integer retval = 0;

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
		Key_SetBinding (IMT_0, key, binding);

		set_key_flag = 0;
		retval = 1;
	} else {
		if(key == QFK_RETURN) {
			set_key_flag = 1;

			retval = 1;
		} else if(key == QFK_BACKSPACE) {
			Key_SetBinding (IMT_0, Key_LookupBinding(IMT_0, binding), "");

			retval = 1;
		}
	}

	load_keybindings();
	return retval;
};

integer () control_bind_draw = 
{
	local integer cursor_pad = 0;

	Draw_String		(20,  20, "Attack: " + key_bindings_keys[0]);
	Draw_String		(20,  30, "Next weapon: " + key_bindings_keys[1]);
	Draw_String		(20,  40, "Jump/Swim up: " + key_bindings_keys[2]);
	Draw_String		(20,  70, "Backspace => Del binding");
	Draw_String     (20,  80, "Enter => New binding");

	opt_cursor  (12, (Menu_GetIndex() * 10) + 20 + cursor_pad);

	return 1;
};

void () control_bind_menu =
{
	Menu_Begin (54, 90, "Bindings");
	Menu_FadeScreen (1);
	Menu_Draw (control_bind_draw);
//	Menu_KeyEvent (control_bind_keyevent);
	Menu_Item (20, 20, "set_attack_key", control_bind_f, 1);
	Menu_Item (20, 30, "set_next_weap_key", control_bind_f, 1);
	Menu_Item (20, 40, "set_jump_key", control_bind_f, 1);
	Menu_End ();
};

