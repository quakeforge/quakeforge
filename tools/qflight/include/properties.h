/*
	properties.c

	Light properties handling.

	Copyright (C) 2004 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2004/01/27

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
#ifndef __properties_h
#define __properties_h

/** \defgroup qflight_properties Lighting properties
	\ingroup qflight
*/
///@{

struct plitem_s;

/** Parse a float from a string.

	\param str		The string holding the float.
	\return			The floating point value parsed from the string.

	\bug			No error checking is performed.
*/
float parse_float (const char *str);

/** Parse a vector from a string.

	\param str		The string holding the float.
	\param vec		The destination vector into which the vector values will
					be written.

	\bug			No error checking is performed.
*/
void parse_vector (const char *str, vec3_t vec);

/** Parse an RGB color vector from a string.

	The RGB values are normalized such that the magnitude of the largest
	component is 1.0. The signs of all components are preserved.

	\param str		The string holding the RGB color vector.
	\param color	The destination vector into which the RGB values will
					be written.

	\note			If any error occurs while parsing the RGB values, the
					resulting color will be white ([1.0 1.0 1.0])
*/
void parse_color (const char *str, vec3_t color);

/** Parse a light intensity/color specification.

	There are three possible specification formats:
	\arg HalfLife	"R G B i" where R G & B are 0-255 and i is the same as
					for normal id lights. The RGB values will be scaled by
					1/255 such that 255 becomes 1.0.
	\arg RGB		"R G B" where R G & B are left as-is and the intensity
					is set to 1.0.
	\arg id			Standard quake single value light intensity. The color
					is set to white ([1.0 1.0 1.0]).

	\param str		The string holding the light specification.
	\param color	The destination vector into which the RGB values will
					be written.
	\return			The intensity of the light.

	\note			If any error occurs while parsing the specification, the
					resulting color will be while ([1.0 1.0 1.0]) and the
					intensity will be ::DEFAULTLIGHTLEVEL.
*/
float parse_light (const char *str, vec3_t color);

/** Parse the light attenuation style from a string.

	Valid attenuations:
	\arg \c linear		id style. The light level falls off linearly from
						"light" at 0 to 0 at distance "light".
	\arg \c radius		Similar to linear, but the cut-off distance is at
						"radius"
	\arg \c inverse		1/r attenuation
	\arg \c realistic	1/(r*r) (inverse-square) attenuation
	\arg \c none		No attenuation. The level is always "light".
	\arg \c havoc		Like realistic, but with a cut-off (FIXME, math?)

	\param arg		The string holding the attenuation style.
	\return 		The parsed attenuation style value, or -1 if ivalid.
*/
int parse_attenuation (const char *arg);

/** Parse the light's noise type from a string.

	Valid noise types:
	\arg \c random	completely random noise (default)
	\arg \c smooth	low res noise with interpolation
	\arg \c perlin	combines several noise frequencies (Perlin noise).
	\return 		The parsed noise type value, or -1 if ivalid.
*/
int parse_noise (const char *arg);

/** Set the world entity's sun values based on its fields and the loaded
	lighting properties database.

	The database is loaded via LoadProperties().

	If a set of properties named "worldspawn" is in the database, then it
	will be used for default values, otherwise the database will be
	ignored.

	Supported properties:
	\arg \c sunlight		see \ref parse_light
	\arg \c sunlight2		see \ref parse_light
	\arg \c sunlight3		see \ref parse_light
	\arg \c sunlight_color	see \ref parse_color
	\arg \c sunlight_color2	see \ref parse_color
	\arg \c sunlight_color3	see \ref parse_color
	\arg \c sun_mangle		see \ref parse_vector

	\param ent		The entity for which to set the lighting values.
	\param dict		A dictionary property list item representing the fields
					of the entity. Values specified in \a dict will override
					those specified in the database.
*/
void set_sun_properties (entity_t *ent, struct plitem_s *dict);

/** Set the light entity's lighting values based on its fields and the loaded
	lighting properties database.

	The database is loaded via LoadProperties().

	If the entity has a key named "_light_name", then the value will be used
	to select a set of default properties from the database.

	Should that fail (either no "_light_name" key, or no properties with the
	given name), then the entity's classname is used.

	Should that fail, the set of properties named "default" will be used.

	If no set of properties can be found in the database, then the database
	will be ignored.

	Supported properties:
	\arg \c light			see \ref parse_light
	\arg \c style			light style: 0-254
	\arg \c angle			spotlight cone angle in degress. defaults to 20
	\arg \c wait			light "falloff". defaults to 1.0
	\arg \c lightradius		size of light. interacts with falloff for
							distance clipping (?). defaults to 0
	\arg \c color			see \ref parse_color
	\arg \c attenuation		see \ref parse_attenuation
	\arg \c radius			the range of the light.
	\arg \c noise			noise intensity (?)
	\arg \c noisetype		see \ref parse_noise
	\arg \c persistence		noise parameter
	\arg \c resolution		noise parameter

	\param ent		The entity for which to set the lighting values.
	\param dict		A dictionary property list item representing the fields
					of the entity. Values specified in \a dict will override
					those specified in the database.
*/
void set_properties (entity_t *ent, struct plitem_s *dict);

/** Load the lighting properties database from the specified file.

	The lighting properties database is a \ref property-list. The property
	list must be a dictionary of dictionaries. The outer dictionary is keyed
	by the light entity's _light_name field or classname field (_light_name
	overrides classname so lights of the same class can have different
	properties). If a "default" key is provided, that will be used as default
	settings for all lights that don't find an entry via _light_name or
	classname.

	The inner dictionary is keyed by the fields of the entity
	(see \ref set_properties), and all values must be strings.

	\param filename	The path to the lighting properties database.
*/
void LoadProperties (const char *filename);

///@}

#endif//__properties_h
