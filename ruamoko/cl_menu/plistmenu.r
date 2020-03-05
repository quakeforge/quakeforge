/*
    plistmenu.r

    PropertyList menu parser.

    Copyright (C) 2010 Bill Currie <bill@taniwha.org>

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

#include "debug.h"
#include "string.h"
#include "qfs.h"

#include "Array.h"
#include "gui/Rect.h"
#include "plistmenu.h"

@reference Pic;
@reference CenterPic;

@static @param
class_from_plist (PLDictionary *pldict)
{
	local @param params[8];
	local @param ret;
	local @va_list va_list = { 0, params };
	local string classname, selname, paramstr;
	local id class;
	local id obj;
	local PLArray *messages, *msg;
	local int message_count, i, j;
	local SEL sel;
	local PLItem *item;

	ret = nil;
	classname = [(PLString*) [pldict getObjectForKey:"Class"] string];
	class = obj_lookup_class (classname);
	if (!class) {
		dprint ("could not find " + classname + "\n");
		ret.pointer_val = nil;
		return ret;
	}
	obj = [class alloc];

	messages = (PLArray*) [pldict getObjectForKey:"Messages"];
	message_count = [messages count];
	for (i = 0; i < message_count; i++) {
		msg = (PLArray*) [messages getObjectAtIndex:i];
		selname = [(PLString*) [msg getObjectAtIndex:0] string];
		sel = sel_get_uid (selname);
		va_list.count = [msg count] - 1;
		for (j = 0; j < va_list.count; j++) {
			paramstr = [(PLString*) [msg getObjectAtIndex:j + 1] string];
			switch (str_mid (paramstr, 0, 1)) {
				case "\"":
					va_list.list[j].string_val = str_mid (paramstr, 1, -1);
					break;
				case "$":
					item = [pldict getObjectForKey:str_mid (paramstr, 1)];
					va_list.list[j] = object_from_plist (item);
					break;
				case "0": case "1": case "2": case "3": case "4":
				case "5": case "6": case "7": case "8": case "9":
					if (str_str (paramstr, "."))
						va_list.list[j].float_val = stof (paramstr);
					else
						va_list.list[j].integer_val = stoi (paramstr);
					break;
			}
		}
		obj_msg_sendv (obj, sel, va_list);
	}
	ret.pointer_val = obj;
	return ret;
}

@static @param
array_from_plist (PLArray *plarray)
{
	local Array *array;
	local int i, count;
	local @param ret;

	ret = nil;
	array = [[Array alloc] init];
	count = [plarray count];
	for (i = 0; i < count; i++) {
		ret = object_from_plist ([plarray getObjectAtIndex:i]);
		[array addObject: (id) ret.pointer_val];
	}
	ret.pointer_val = array;
	return ret;
}

union ParamRect {
	@param p;
	Rect r;
};

@static @param
rect_from_plist (PLString *plstring)
{
	local string str = [plstring string];
	local string tmp;
	local int xp, yp, xl, yl;
	local PLItem *item;
	local union ParamRect pr;

	pr.r = makeRect (0, 0, 0, 0);
	if (str_mid (str, 0, 1) == "[") {
		tmp = "(" + str_mid (str, 1, -1) + ")";
		item = [PLItem fromString:tmp];
		xp = stoi ([(PLString*) [(PLArray*) item getObjectAtIndex:0] string]);
		yp = stoi ([(PLString*) [(PLArray*) item getObjectAtIndex:1] string]);
		xl = stoi ([(PLString*) [(PLArray*) item getObjectAtIndex:2] string]);
		yl = stoi ([(PLString*) [(PLArray*) item getObjectAtIndex:3] string]);
		pr.r = makeRect (xp, yp, xl, yl);
	}
	return pr.p;
}

@static @param
string_from_plist (PLString *plstring)
{
	local @param ret;
	local string str = [plstring string];

	ret = nil;	//FIXME should be ret = nil;
	if (str_mid (str, 0, 1) == "[")
		return rect_from_plist (plstring);

	ret.string_val = nil;
	return ret;
}

@param
object_from_plist (PLItem *plist)
{
	local @param ret;

	ret = nil;
	switch ([plist type]) {
		case QFDictionary:
			return class_from_plist ((PLDictionary *) plist);
		case QFArray:
			return array_from_plist ((PLArray *) plist);
		case QFBinary:
			ret.pointer_val = nil;
			return ret;
		case QFString:
			return string_from_plist ((PLString *) plist);
	}
	return nil;
}

PLItem *
read_plist (string fname)
{
	local QFile file;
	local PLItem *plist;

	file = QFS_OpenFile (fname);
	if (!file) {
		dprint ("could not load menu.plist\n");
		return nil;
	}
	plist = [PLItem fromFile:file];
	Qclose (file);
	return plist;
}
