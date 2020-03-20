#ifndef __ruamoko_msgbuf_h
#define __ruamoko_msgbuf_h

#include <qfile.h>

struct msgbuf_s;
typedef struct msgbuf_s msgbuf_t;

@extern msgbuf_t *MsgBuf_New (int size);
@extern void MsgBuf_Delete (msgbuf_t *msgbuf);
@extern void MsgBuf_FromFile (msgbuf_t *msgbuf, QFile file);
@extern int MsgBuf_MaxSize (msgbuf_t *msgbuf);
@extern int MsgBuf_CurSize (msgbuf_t *msgbuf);
@extern int MsgBuf_ReadCount (msgbuf_t *msgbuf);
@extern string MsgBuf_DataPtr (msgbuf_t *msgbuf);

@extern void MsgBuf_Clear (msgbuf_t *msgbuf);
@extern void MsgBuf_WriteByte (msgbuf_t *msgbuf, int val);
@extern void MsgBuf_WriteShort (msgbuf_t *msgbuf, int val);
@extern void MsgBuf_WriteLong (msgbuf_t *msgbuf, int val);
@extern void MsgBuf_WriteFloat (msgbuf_t *msgbuf, float val);
@extern void MsgBuf_WriteString (msgbuf_t *msgbuf, string str);
@extern void MsgBuf_WriteCoord (msgbuf_t *msgbuf, float coord);
@extern void MsgBuf_WriteCoordV (msgbuf_t *msgbuf, vector coord);
@extern void MsgBuf_WriteCoordAngleV (msgbuf_t *msgbuf, vector coords,
									  vector angles);
@extern void MsgBuf_WriteAngle (msgbuf_t *msgbuf, float angle);
@extern void MsgBuf_WriteAngleV (msgbuf_t *msgbuf, vector angles);
@extern void MsgBuf_WriteAngle16 (msgbuf_t *msgbuf, float angle);
@extern void MsgBuf_WriteAngle16V (msgbuf_t *msgbuf, vector angles);
@extern void MsgBuf_WriteUTF8 (msgbuf_t *msgbuf, int val);

@extern void MsgBuf_BeginReading (msgbuf_t *msgbuf);
@extern int MsgBuf_ReadByte (msgbuf_t *msgbuf);
@extern int MsgBuf_ReadShort (msgbuf_t *msgbuf);
@extern int MsgBuf_ReadLong (msgbuf_t *msgbuf);
@extern float MsgBuf_ReadFloat (msgbuf_t *msgbuf);
@extern string MsgBuf_ReadString (msgbuf_t *msgbuf);
@extern float MsgBuf_ReadCoord (msgbuf_t *msgbuf);
@extern vector MsgBuf_ReadCoordV (msgbuf_t *msgbuf);
@extern void MsgBuf_ReadCoordAngleV (msgbuf_t *msgbuf, vector *coords,
									 vector *angles);
@extern float MsgBuf_ReadAngle (msgbuf_t *msgbuf);
@extern vector MsgBuf_ReadAngleV (msgbuf_t *msgbuf);
@extern float MsgBuf_ReadAngle16 (msgbuf_t *msgbuf);
@extern vector MsgBuf_ReadAngle16V (msgbuf_t *msgbuf);
@extern int MsgBuf_ReadUTF8 (msgbuf_t *msgbuf);

#endif//__ruamoko_msgbuf_h
