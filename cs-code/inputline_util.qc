/*          
    inputline_util.qc

    Utilities for the inputline
                    
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


/*
    InputLine_SetText

    Inserts characters of a string into
    a inputline.
*/

void (inputline_t il, string str)
InputLine_SetText = 
{
	local integer i, charint;
	charint = String_GetChar(str,0);
	for(i = 0; charint != 0; i++) {
		InputLine_Process(il, String_GetChar(str,i));
		charint = String_GetChar(str,i);
	}
};
