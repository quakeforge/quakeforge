/*
	sy_type_names.h

	Symbol type names

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/

/**	\defgroup qfcc_symtab_sy_type_names Symbol Types
	\ingroup qfcc_symtab
*/
///@{

#ifndef SY_TYPE
#define SY_TYPE(type)
#endif

SY_TYPE(name)					///< just a name (referent tbd)
SY_TYPE(var)					///< symbol refers to a variable
SY_TYPE(const)					///< symbol refers to a constant
SY_TYPE(type)					///< symbol refers to a type
SY_TYPE(type_param)				///< symbol refers to a generic type parameter
SY_TYPE(expr)					///< symbol refers to an expression
SY_TYPE(func)					///< symbol refers to a function
SY_TYPE(class)					///< symbol refers to a class
SY_TYPE(convert)				///< symbol refers to a conversion function
SY_TYPE(macro)					///< symbol refers to a macro definition
SY_TYPE(namespace)				///< symbol refers to a namespace definition
SY_TYPE(list)

#undef SY_TYPE

///@}
