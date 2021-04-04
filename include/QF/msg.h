/*
	msg.h

	(description)

	Copyright (C) 1996-1997  Id Software, Inc.

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
#ifndef __QF_msg_h
#define __QF_msg_h

/** \defgroup msg Message reading and writing
	\ingroup utils
*/
///@{

#include "QF/sizebuf.h"

void MSG_WriteByte (sizebuf_t *sb, int c);
void MSG_WriteShort (sizebuf_t *sb, int c);
void MSG_WriteLong (sizebuf_t *sb, int c);
void MSG_WriteLongBE (sizebuf_t *sb, int c);
void MSG_WriteFloat (sizebuf_t *sb, float f);
void MSG_WriteString (sizebuf_t *sb, const char *s);
void MSG_WriteBytes (sizebuf_t *sb, const void *buf, int len);
void MSG_WriteCoord (sizebuf_t *sb, float coord);
void MSG_WriteCoordV (sizebuf_t *sb, const vec3_t coord);
void MSG_WriteCoordAngleV (sizebuf_t *sb, const vec3_t coord,
						   const vec3_t angles);
void MSG_WriteAngle (sizebuf_t *sb, float angle);
void MSG_WriteAngleV (sizebuf_t *sb, const vec3_t angles);
void MSG_WriteAngle16 (sizebuf_t *sb, float angle);
void MSG_WriteAngle16V (sizebuf_t *sb, const vec3_t angle);
void MSG_WriteUTF8 (sizebuf_t *sb, unsigned utf8);

typedef struct msg_s {
	int readcount;
	qboolean badread;		// set if a read goes beyond end of message
	sizebuf_t *message;
	size_t badread_string_size;
	char *badread_string;
} qmsg_t;

/** Reset the message read status.

	Clears the error flag and resets the read index to 0.

	\param msg		The message to be reset.
*/
void MSG_BeginReading (qmsg_t *msg);

/** Get the number of bytes that have been read from the message.

	The result can also be used as the index to the next byte to be read.

	\param msg		The message to check.
	\return			The number of bytes that have been read.
*/
int MSG_GetReadCount(qmsg_t *msg) __attribute__((pure));

/** Read a single byte from the message.

	Advances the read index.

	\param msg		The message from which the byte will be read.
	\return			The byte value (0 - 255), or -1 if already at the end of
					the message.
*/
int MSG_ReadByte (qmsg_t *msg);

/** Read a single little-endian unsigned short from the message.

	Advances the read index.

	\param msg		The message from which the short will be read.
	\return			The short value (0 - 65535), or -1 if already at
					the end of the message.
*/
int MSG_ReadShort (qmsg_t *msg);

/** Read a single little-endian long from the message.

	Advances the read index.

	\param msg		The message from which the long will be read.
	\return			The signed long value or -1 if already at the end of
					the message.
	\note	-1 may be either an error or a value. Check qmsg_t::badread to
			differentiate the two cases (false for a value).
	\todo	Fix?
*/
int MSG_ReadLong (qmsg_t *msg);

/** Read a single big-endian long from the message.

	Advances the read index.

	\param msg		The message from which the long will be read.
	\return			The signed long value or -1 if already at the end of
					the message.
	\note	-1 may be either an error or a value. Check qmsg_t::badread to
			differentiate the two cases (false for a value).
	\todo	Fix?
*/
int MSG_ReadLongBE (qmsg_t *msg);

/** Read a single little-endian float from the message.

	Advances the read index.

	\param msg		The message from which the float will be read.
	\return			The float value or -1 if already at the end of the
					message.
	\note	-1 may be either an error or a value. Check qmsg_t::badread to
			differentiate the two cases (false for a value).
	\todo	Fix?
*/
float MSG_ReadFloat (qmsg_t *msg);

/** Read a nul terminated string from the message.

	Advances the read index to the first byte after the string.

	The returned string is guaranteed to be valid. If the string in the
	message is not nul terminated, the string will be safely extracted,
	qmst_t::badread set to true, and the extracted string will be returned.

	\param msg		The message from which the string will be read.
	\return			The string within the message, or "" if alread at the
					end of the message.
	\note	"" may be either an error or a value. Check qmsg_t::badread to
			differentiate the two cases (false for a value).
	\todo	Fix?
*/
const char *MSG_ReadString (qmsg_t *msg);

/** Read a block of bytes from the message.

	Advances the read index to the first byte after the block.

	If not all bytes could be read, qmsg_t::badread will be set to true.

	\param msg		The message from which the string will be read.
	\param buf		Pointer to the buffer into which the bytes will be
					placed.
	\param len		The number of bytes to read.
	\return			The number of bytes read from the message.
*/
int MSG_ReadBytes (qmsg_t *msg, void *buf, int len);

/** Read a little-endian 16-bit fixed point (13.3) coordinate value from the
	message.

	Advances the read index to the first byte after the block.

	\param msg		The message from which the coordinate will be read.
	\return			The coordinate value converted to floating point.

	\note	-0.125 may be either an error or a value. Check qmsg_t::badread
			to differentiate the two cases (false for a value).
	\todo	Fix?
*/
float MSG_ReadCoord (qmsg_t *msg);

/** Read three little-endian 16-bit fixed point (s12.3) coordinate values
	from the message.

	Advances the read index.

	\param msg		The message from which the coordinates will be read.
	\param coord	The vector into which the three coordinates will be
					placed.

	\note	-0.125 may be either an error or a value. Check qmsg_t::badread
			to differentiate the two cases (false for a value).
	\todo	Fix?
*/
void MSG_ReadCoordV (qmsg_t *msg, vec3_t coord);

/** Read an 8-bit encoded angle from the message.

	Advances the read index.

	\param msg		The message from which the angle will be read.
	\return			The angle converted to the range -180 - 180.
*/
float MSG_ReadAngle (qmsg_t *msg);

/** Read interleaved little-endian 16-bit coordinate/8-bit angle vectors
	from the message.

	Advances the read index.

	\param msg		The message from which the angle will be read.
	\param coord	The vector into which the converted coordinate vector
					will be placed.
	\param angles	The vector into which the converted coordinate angles
					will be placed.
	\see MSG_ReadCoord
	\see MSG_ReadAngle
*/
void MSG_ReadCoordAngleV (qmsg_t *msg, vec3_t coord, vec3_t angles);

/** Read three 8-bit encoded angle values from the message.

	Advances the read index.

	\param msg		The message from which the angles will be read.
	\param angles	The vector into which the three angles will be placed.

	\note The angles will be converted to the range -180 - 180.
*/
void MSG_ReadAngleV (qmsg_t *msg, vec3_t angles);

/** Read a little-endian 16-bit encoded angle from the message.

	Advances the read index.

	\param msg		The message from which the angle will be read.
	\return			The angle converted to the range -180 - 180.
*/
float MSG_ReadAngle16 (qmsg_t *msg);

/** Read three little-endian 16-bit encoded angle values from the message.

	Advances the read index.

	\param msg		The message from which the angles will be read.
	\param angles	The vector into which the three angles will be placed.

	\note The angles will be converted to the range -180 - 180.
*/
void MSG_ReadAngle16V (qmsg_t *msg, vec3_t angles);

/** Read a single UTF-8 encoded value.

	Advances the read index the the first byte after the value. However,
	the read index will not be advanced if any error is detected.

	A successfull read will read between 1 and 6 bytes from the message.

	\param msg		The message from which the UTF-8 encoded value will be
					read.
	\return			The 31-bit value, or -1 on error.
*/
int MSG_ReadUTF8 (qmsg_t *msg);

///@}

#endif//__QF_msg_h
