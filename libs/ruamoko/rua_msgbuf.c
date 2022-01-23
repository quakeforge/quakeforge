/*
	bi_msgbuf.c

	CSQC message buffer builtins

	Copyright (C) 2012 Bill Currie <bill:taniwha.org>

	Author: Bill Currie <bill:taniwha.org>
	Date: 2012/2/6

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/msg.h"
#include "QF/progs.h"

#include "rua_internal.h"

typedef struct msgbuf_s {
	struct msgbuf_s *next;
	struct msgbuf_s **prev;
	qmsg_t      msg;
	sizebuf_t   sizebuf;
} msgbuf_t;

typedef struct {
	PR_RESMAP (msgbuf_t) msgbuf_map;
} msgbuf_resources_t;

static msgbuf_t *
msgbuf_new (msgbuf_resources_t *res)
{
	return PR_RESNEW (res->msgbuf_map);
}

static void
msgbuf_free (progs_t *pr, msgbuf_resources_t *res, msgbuf_t *msgbuf)
{
	PR_Zone_Free (pr, msgbuf->sizebuf.data);
	PR_RESFREE (res->msgbuf_map, msgbuf);
}

static void
msgbuf_reset (msgbuf_resources_t *res)
{
	PR_RESRESET (res->msgbuf_map);
}

static inline msgbuf_t *
msgbuf_get (msgbuf_resources_t *res, int index)
{
	return PR_RESGET(res->msgbuf_map, index);
}

static inline int __attribute__((pure))
msgbuf_index (msgbuf_resources_t *res, msgbuf_t *msgbuf)
{
	return PR_RESINDEX(res->msgbuf_map, msgbuf);
}

static void
bi_msgbuf_clear (progs_t *pr, void *data)
{
	msgbuf_resources_t *res = (msgbuf_resources_t *) data;

	msgbuf_reset (res);
}

static int
alloc_msgbuf (msgbuf_resources_t *res, byte *buf, int size)
{
	msgbuf_t   *msgbuf = msgbuf_new (res);

	if (!msgbuf)
		return 0;

	memset (&msgbuf->msg, 0, sizeof (msgbuf->msg));
	msgbuf->msg.message = &msgbuf->sizebuf;
	memset (&msgbuf->sizebuf, 0, sizeof (msgbuf->sizebuf));
	msgbuf->sizebuf.data = buf;
	msgbuf->sizebuf.maxsize = size;
	return msgbuf_index (res, msgbuf);
}

static msgbuf_t *
get_msgbuf (progs_t *pr, const char *name, int msgbuf)
{
	msgbuf_resources_t *res = PR_Resources_Find (pr, "MsgBuf");
	msgbuf_t   *mb = msgbuf_get (res, msgbuf);

	if (!mb)
		PR_RunError (pr, "invalid msgbuf handle passed to %s", name + 3);
	return mb;
}

static void
bi_MsgBuf_New (progs_t *pr)
{
	msgbuf_resources_t *res = PR_Resources_Find (pr, "MsgBuf");
	int         size = P_INT (pr, 0);
	byte       *buf;

	buf = PR_Zone_Malloc (pr, size);
	R_INT (pr) = alloc_msgbuf (res, buf, size);
}

static void
bi_MsgBuf_Delete (progs_t *pr)
{
	msgbuf_resources_t *res = PR_Resources_Find (pr, "MsgBuf");
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	msgbuf_free (pr, res, mb);
}

static void
bi_MsgBuf_FromFile (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	QFile      *file = QFile_GetFile (pr, P_INT (pr, 1));
	int         bytes;

	SZ_Clear (&mb->sizebuf);
	bytes = Qread (file, mb->sizebuf.data, mb->sizebuf.maxsize);
	mb->sizebuf.cursize = bytes;
	MSG_BeginReading (&mb->msg);
}

static void
bi_MsgBuf_MaxSize (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	R_INT (pr) = mb->sizebuf.maxsize;
}

static void
bi_MsgBuf_CurSize (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	R_INT (pr) = mb->sizebuf.cursize;
}

static void
bi_MsgBuf_ReadCount (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	R_INT (pr) = mb->msg.readcount;
}

static void
bi_MsgBuf_DataPtr (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	byte       *ptr = mb->sizebuf.data + mb->msg.readcount;
	R_INT (pr) = ptr - (byte *) pr->pr_strings;
}

static void
bi_MsgBuf_Clear (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	SZ_Clear (&mb->sizebuf);
}

static void
bi_MsgBuf_WriteByte (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	MSG_WriteByte (&mb->sizebuf, P_INT (pr, 1));
}

static void
bi_MsgBuf_WriteShort (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	MSG_WriteShort (&mb->sizebuf, P_INT (pr, 1));
}

static void
bi_MsgBuf_WriteLong (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	MSG_WriteLong (&mb->sizebuf, P_INT (pr, 1));
}

static void
bi_MsgBuf_WriteFloat (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	MSG_WriteFloat (&mb->sizebuf, P_FLOAT (pr, 1));
}

static void
bi_MsgBuf_WriteString (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	MSG_WriteString (&mb->sizebuf, P_GSTRING (pr, 1));
}
#if 0
static void
bi_MsgBuf_WriteBytes (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	MSG_WriteBytes (&mb->sizebuf, P_INT (pr, 1));
}
#endif
static void
bi_MsgBuf_WriteCoord (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	MSG_WriteCoord (&mb->sizebuf, P_FLOAT (pr, 1));
}

static void
bi_MsgBuf_WriteCoordV (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	MSG_WriteCoordV (&mb->sizebuf, P_VECTOR (pr, 1));
}

static void
bi_MsgBuf_WriteCoordAngleV (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	MSG_WriteCoordAngleV (&mb->sizebuf,
						  P_VECTOR (pr, 1), P_VECTOR (pr, 2));
}

static void
bi_MsgBuf_WriteAngle (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	MSG_WriteAngle (&mb->sizebuf, P_FLOAT (pr, 1));
}

static void
bi_MsgBuf_WriteAngleV (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	MSG_WriteAngleV (&mb->sizebuf, P_VECTOR (pr, 1));
}

static void
bi_MsgBuf_WriteAngle16 (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	MSG_WriteAngle16 (&mb->sizebuf, P_FLOAT (pr, 1));
}

static void
bi_MsgBuf_WriteAngle16V (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	MSG_WriteAngle16V (&mb->sizebuf, P_VECTOR (pr, 1));
}

static void
bi_MsgBuf_WriteUTF8 (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	MSG_WriteUTF8 (&mb->sizebuf, P_INT (pr, 1));
}

static void
bi_MsgBuf_BeginReading (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	MSG_BeginReading (&mb->msg);
}

static void
bi_MsgBuf_ReadByte (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	R_INT (pr) = MSG_ReadByte (&mb->msg);
}

static void
bi_MsgBuf_ReadShort (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	R_INT (pr) = MSG_ReadShort (&mb->msg);
}

static void
bi_MsgBuf_ReadLong (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	R_INT (pr) = MSG_ReadLong (&mb->msg);
}

static void
bi_MsgBuf_ReadFloat (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	R_FLOAT (pr) = MSG_ReadFloat (&mb->msg);
}

static void
bi_MsgBuf_ReadString (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	const char *str;

	str = MSG_ReadString (&mb->msg);
	RETURN_STRING (pr, str);
}
#if 0
static void
bi_MsgBuf_ReadBytes (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	MSG_ReadBytes (&mb->msg);
}
#endif
static void
bi_MsgBuf_ReadCoord (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	R_FLOAT (pr) = MSG_ReadCoord (&mb->msg);
}

static void
bi_MsgBuf_ReadCoordV (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	MSG_ReadCoordV (&mb->msg, R_VECTOR (pr));
}

static void
bi_MsgBuf_ReadCoordAngleV (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	MSG_ReadCoordAngleV (&mb->msg, P_VECTOR (pr, 1), P_VECTOR (pr, 2));
}

static void
bi_MsgBuf_ReadAngle (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	R_FLOAT (pr) = MSG_ReadAngle (&mb->msg);
}

static void
bi_MsgBuf_ReadAngleV (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	MSG_ReadAngleV (&mb->msg, R_VECTOR (pr));
}

static void
bi_MsgBuf_ReadAngle16 (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	R_FLOAT (pr) = MSG_ReadAngle16 (&mb->msg);
}

static void
bi_MsgBuf_ReadAngle16V (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	MSG_ReadAngle16V (&mb->msg, R_VECTOR (pr));
}

static void
bi_MsgBuf_ReadUTF8 (progs_t *pr)
{
	msgbuf_t   *mb = get_msgbuf (pr, __FUNCTION__, P_INT (pr, 0));
	R_INT (pr) = MSG_ReadUTF8 (&mb->msg);
}

#define bi(x,np,params...) {#x, bi_##x, -1, np, {params}}
#define p(type) PR_PARAM(type)
#define P(a, s) { .size = (s), .alignment = BITOP_LOG2 (a), }
static builtin_t builtins[] = {
	bi(MsgBuf_New,              1, p(int)),
	bi(MsgBuf_Delete,           1, p(ptr)),
	bi(MsgBuf_FromFile,         2, p(ptr), p(ptr)),
	bi(MsgBuf_MaxSize,          1, p(ptr)),
	bi(MsgBuf_CurSize,          1, p(ptr)),
	bi(MsgBuf_ReadCount,        1, p(ptr)),
	bi(MsgBuf_DataPtr,          1, p(ptr)),

	bi(MsgBuf_Clear,            1, p(ptr)),
	bi(MsgBuf_WriteByte,        2, p(ptr), p(int)),
	bi(MsgBuf_WriteShort,       2, p(ptr), p(int)),
	bi(MsgBuf_WriteLong,        2, p(ptr), p(int)),
	bi(MsgBuf_WriteFloat,       2, p(ptr), p(float)),
	bi(MsgBuf_WriteString,      2, p(ptr), p(string)),
//	bi(MsgBuf_WriteBytes,       _, _),
	bi(MsgBuf_WriteCoord,       2, p(ptr), p(float)),
	bi(MsgBuf_WriteCoordV,      2, p(ptr), p(vector)),
	bi(MsgBuf_WriteCoordAngleV, 2, p(ptr), p(vector)),
	bi(MsgBuf_WriteAngle,       2, p(ptr), p(float)),
	bi(MsgBuf_WriteAngleV,      2, p(ptr), p(vector)),
	bi(MsgBuf_WriteAngle16,     2, p(ptr), p(float)),
	bi(MsgBuf_WriteAngle16V,    2, p(ptr), p(vector)),
	bi(MsgBuf_WriteUTF8,        2, p(ptr), p(int)),

	bi(MsgBuf_BeginReading,     1, p(ptr)),
	bi(MsgBuf_ReadByte,         1, p(ptr)),
	bi(MsgBuf_ReadShort,        1, p(ptr)),
	bi(MsgBuf_ReadLong,         1, p(ptr)),
	bi(MsgBuf_ReadFloat,        1, p(ptr)),
	bi(MsgBuf_ReadString,       1, p(ptr)),
//	bi(MsgBuf_ReadBytes,        _, _),
	bi(MsgBuf_ReadCoord,        1, p(ptr)),
	bi(MsgBuf_ReadCoordV,       1, p(ptr)),
	bi(MsgBuf_ReadCoordAngleV,  2, p(ptr), p(ptr)),
	bi(MsgBuf_ReadAngle,        1, p(ptr)),
	bi(MsgBuf_ReadAngleV,       1, p(ptr)),
	bi(MsgBuf_ReadAngle16,      1, p(ptr)),
	bi(MsgBuf_ReadAngle16V,     1, p(ptr)),
	bi(MsgBuf_ReadUTF8,         1, p(ptr)),
	{0}
};

void
RUA_MsgBuf_Init (progs_t *pr, int secure)
{
	msgbuf_resources_t *res = calloc (sizeof (msgbuf_resources_t), 1);

	PR_Resources_Register (pr, "MsgBuf", res, bi_msgbuf_clear);
	PR_RegisterBuiltins (pr, builtins);
}
