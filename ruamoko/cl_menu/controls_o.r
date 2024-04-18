/*
    controls_o.qc

    Controls settings menu

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

#include "Array.h"

#include "menu.h"
#include "draw.h"
#include "system.h"
#include "debug.h"
#include "legacy_string.h"
#include "string.h"
#include "options_util.h"

/*
	MENU_control_binding

	Main controls menu, for selecting the sub control menus
*/
void ()
MENU_control_binding =
{
	Menu_Begin (54, 60, "Bindings");
	Menu_Pic (16, 4, "gfx/qplaque.lmp");
	Menu_CenterPic (160, 4, "gfx/p_option.lmp");

	Menu_FadeScreen (1);

//	MENU_basic_control_binding ();
//	MENU_misc_control_binding ();
//	MENU_weapon_control_binding();

	Menu_End ();
};
