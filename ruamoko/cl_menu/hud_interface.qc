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
		:(integer) stof (argv[1])
		:(integer) stof (argv[2])
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
		:(integer) stof (argv[1])
		:(integer) stof (argv[2])
		:argv[3]
	];

	[HUDObjects addItem :newGraphic];

	GIB_Return (itos ([newGraphic handle]));
};


void (integer argc, string [] argv) gib_hud_delete_f =
{
	local integer i;
	local HUDObject trashObject;

	if (argc < 2)
		return;

	for (i = 1; i < argc; i++) {
		trashObject = GIB_Handle_Get ((integer) stof (argv[i]), HUDHandleClass);
		if (trashObject) {
			[HUDObjects removeItem :trashObject];
			[trashObject free];
		} else
			dprint (sprintf ("Warning: no HUD object associated with handle %i\n", (integer) stof (argv[i])));
	}
};

void (integer argc, string [] argv) gib_hud_get_origin_f =
{
	local HUDObject myObject;
	local Point myOrigin;

	if (argc != 2)
		return;

	myObject = GIB_Handle_Get ((integer) stof (argv[1]), HUDHandleClass);
	if (!myObject)
		return;

	myOrigin = [myObject origin];
	GIB_Return (itos ([myOrigin x]));
	GIB_Return (itos ([myOrigin y]));
};

void (integer argc, string [] argv) gib_hud_set_translate_f =
{
	local integer i;
	local Point p;

	if (argc < 4)
		return;

	p = [[Point alloc] initWithComponents :(integer) stof (argv[argc-2]) :(integer) stof (argv[argc-1])];

	if (argv[0] == "HUD::translate")
		for (i = 1; i < argc - 2; i++)
			[(HUDObject) GIB_Handle_Get ((integer) stof (argv[i]), HUDHandleClass) translate :p];
	else
		for (i = 1; i < argc - 2; i++)
			[(HUDObject) GIB_Handle_Get ((integer) stof (argv[i]), HUDHandleClass) setOrigin :p];
	[p free];
};

void (integer argc, string [] argv) gib_hud_set_text_f =
{
	local HUDText myObject;

	if (argc != 3)
		return;

	myObject = GIB_Handle_Get ((integer) stof (argv[1]), HUDHandleClass);
	if (!myObject || ![myObject isKindOf :[HUDText class]])
		return;
	[myObject setText :argv[2]];
};

void (integer argc, string [] argv) gib_hud_set_file_f =
{
	local HUDGraphic myObject;

	if (argc != 3)
		return;

	myObject = GIB_Handle_Get ((integer) stof (argv[1]), HUDHandleClass);
	if (!myObject || ![myObject isKindOf :[HUDGraphic class]])
		return;
	[myObject setFile :argv[2]];
};

void () HUD_Init =
{
	GIB_Builtin_Add ("HUD::newText", gib_hud_new_text_f);
	GIB_Builtin_Add ("HUD::newGraphic", gib_hud_new_graphic_f);
	GIB_Builtin_Add ("HUD::delete", gib_hud_delete_f);
	GIB_Builtin_Add ("HUD::getOrigin", gib_hud_get_origin_f);
	GIB_Builtin_Add ("HUD::setOrigin", gib_hud_set_translate_f);
	GIB_Builtin_Add ("HUD::translate", gib_hud_set_translate_f);
	GIB_Builtin_Add ("HUD::setText", gib_hud_set_text_f);
	GIB_Builtin_Add ("HUD::setFile", gib_hud_set_file_f);

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
