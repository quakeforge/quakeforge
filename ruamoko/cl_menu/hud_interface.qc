#include "gib.h"
#include "draw.h"
#include "Array.h"
#include "HUD.h"
#include "string.h"
#include "debug.h"

@static Array HUDObjects;

void (integer argc, string [] argv) gib_hud_new_text_f =
{
	local HUDObject newText;

	if (argc != 4)
		return;

	newText = [[HUDText alloc] initWithComponents
		:stoi (argv[1])
		:stoi (argv[2])
		:argv[3]
	];

	[HUDObjects addItem :newText];

	GIB_Return (itos ([newText handle]));
};

void (integer argc, string [] argv) gib_hud_new_graphic_f =
{
	local HUDGraphic newGraphic;

	if (argc != 4)
		return;

	newGraphic = [[HUDGraphic alloc] initWithComponents
		:stoi (argv[1])
		:stoi (argv[2])
		:argv[3]
	];

	[HUDObjects addItem :newGraphic];

	GIB_Return (itos ([newGraphic handle]));
};

void (integer argc, string [] argv) gib_hud_new_anim_f =
{
	local HUDAnimation newAnim;
	local Frame frame;
	local integer i;

	if (argc < 5)
		return;

	newAnim = [[HUDAnimation alloc] initWithComponents
		:stoi (argv[1])
		:stoi (argv[2])
	];

	for (i = 3; i < argc; i += 2) {
		frame = [[Frame alloc] initWithFile: argv[i] duration: stof (argv[i+1])];
		[newAnim addFrame :frame];
	}
	[HUDObjects addItem :newAnim];

	GIB_Return (itos ([newAnim handle]));
};

void (integer argc, string [] argv) gib_hud_start_anim_f =
{
	local integer i;
	local HUDAnimation anim;

	if (argc < 2)
		return;

	for (i = 1; i < argc; i++) {
		anim = GIB_Handle_Get (stoi (argv[i]), HUDHandleClass);
		if (anim && [anim isKindOf :[HUDAnimation class]])
			[anim start];
	}
};

void (integer argc, string [] argv) gib_hud_stop_anim_f =
{
	local integer i;
	local HUDAnimation anim;

	if (argc < 2)
		return;

	for (i = 1; i < argc; i++) {
		anim = GIB_Handle_Get (stoi (argv[i]), HUDHandleClass);
		if (anim && [anim isKindOf :[HUDAnimation class]])
			[anim stop];
	}
};

void (integer argc, string [] argv) gib_hud_set_looping_f =
{
	local HUDAnimation anim;

	if (argc < 3)
		return;

	anim = GIB_Handle_Get (stoi (argv[1]), HUDHandleClass);
	if (anim && [anim isKindOf :[HUDAnimation class]]) {
		switch (argv[2]) {
			case "yes":
			case "true":
				[anim setLooping :YES];
				break;
			case "no":
			case "false":
				[anim setLooping :NO];
				break;
			default:
				break;
		}
	}
};

void (integer argc, string [] argv) gib_hud_delete_f =
{
	local integer i;
	local HUDObject trashObject;

	if (argc < 2)
		return;

	for (i = 1; i < argc; i++) {
		trashObject = GIB_Handle_Get (stoi (argv[i]), HUDHandleClass);
		if (trashObject) {
			[HUDObjects removeItem :trashObject];
			[trashObject free];
		} else
			dprint (sprintf ("Warning: no HUD object associated with handle %i\n", stoi (argv[i])));
	}
};

void (integer argc, string [] argv) gib_hud_get_rect_f =
{
	local HUDObject myObject;
	local Point myPoint;

	if (argc != 2)
		return;

	myObject = GIB_Handle_Get (stoi (argv[1]), HUDHandleClass);
	if (!myObject)
		return;

	myPoint = [myObject origin];
	GIB_Return (itos ([myPoint x]));
	GIB_Return (itos ([myPoint y]));
	myPoint = [myObject size];
	GIB_Return (itos ([myPoint x]));
	GIB_Return (itos ([myPoint y]));
	[myPoint free];
};


void (integer argc, string [] argv) gib_hud_set_translate_f =
{
	local integer i;
	local Point p;

	if (argc < 4)
		return;

	p = [[Point alloc] initWithComponents :stoi (argv[argc-2]) :stoi (argv[argc-1])];

	if (argv[0] == "HUD::translate")
		for (i = 1; i < argc - 2; i++)
			[(HUDObject) GIB_Handle_Get (stoi (argv[i]), HUDHandleClass) translate :p];
	else
		for (i = 1; i < argc - 2; i++)
			[(HUDObject) GIB_Handle_Get (stoi (argv[i]), HUDHandleClass) setOrigin :p];
	[p free];
};

void (integer argc, string [] argv) gib_hud_set_text_f =
{
	local HUDText myObject;

	if (argc != 3)
		return;

	myObject = GIB_Handle_Get (stoi (argv[1]), HUDHandleClass);
	if (!myObject || ![myObject isKindOf :[HUDText class]])
		return;
	[myObject setText :argv[2]];
};

void (integer argc, string [] argv) gib_hud_set_file_f =
{
	local HUDGraphic myObject;

	if (argc != 3)
		return;

	myObject = GIB_Handle_Get (stoi (argv[1]), HUDHandleClass);
	if (!myObject || ![myObject isKindOf :[HUDGraphic class]])
		return;
	[myObject setFile :argv[2]];
};

void (integer argc, string [] argv) gib_hud_show_f =
{
	local integer i;
	local HUDObject object;

	if (argc < 2)
		return;

	for (i = 1; i < argc; i++) {
		object = GIB_Handle_Get (stoi (argv[i]), HUDHandleClass);
		if (object)
			[object setVisible :YES];
		else
			dprint (sprintf ("Warning: no HUD object associated with handle %i\n", stoi (argv[i])));
	}
};

void (integer argc, string [] argv) gib_hud_hide_f =
{
	local integer i;
	local HUDObject object;

	if (argc < 2)
		return;

	for (i = 1; i < argc; i++) {
		object = GIB_Handle_Get (stoi (argv[i]), HUDHandleClass);
		if (object)
			[object setVisible :NO];
		else
			dprint (sprintf ("Warning: no HUD object associated with handle %i\n", stoi (argv[i])));
	}
};

void () HUD_Init =
{
	GIB_Builtin_Add ("HUD::newText", gib_hud_new_text_f);
	GIB_Builtin_Add ("HUD::newGraphic", gib_hud_new_graphic_f);
	GIB_Builtin_Add ("HUD::newAnim", gib_hud_new_anim_f);
	GIB_Builtin_Add ("HUD::delete", gib_hud_delete_f);
	GIB_Builtin_Add ("HUD::show", gib_hud_show_f);
	GIB_Builtin_Add ("HUD::hide", gib_hud_hide_f);
	GIB_Builtin_Add ("HUD::getRect", gib_hud_get_rect_f);
	GIB_Builtin_Add ("HUD::setOrigin", gib_hud_set_translate_f);
	GIB_Builtin_Add ("HUD::translate", gib_hud_set_translate_f);
	GIB_Builtin_Add ("HUD::setText", gib_hud_set_text_f);
	GIB_Builtin_Add ("HUD::setFile", gib_hud_set_file_f);
	GIB_Builtin_Add ("HUD::startAnim", gib_hud_start_anim_f);
	GIB_Builtin_Add ("HUD::stopAnim", gib_hud_stop_anim_f);
	GIB_Builtin_Add ("HUD::setLooping", gib_hud_set_looping_f);

	// Initialize HUDObject class
	[HUDObject initClass];

	// Create array of HUD objects
	HUDObjects = [[Array alloc] init];
};

void () menu_draw_hud =
{
	local integer i;
	local HUDObject myHUDObject;
	
	for (i = 0; i < [HUDObjects count]; i++) {
		myHUDObject = [HUDObjects getItemAt :i];
		[myHUDObject display];
	}
};
