#ifndef __ruamoko_msgbuf_h
#define __ruamoko_msgbuf_h

#include <qfile.h>

struct msgbuf_s;
typedef struct msgbuf_s msgbuf_t;

@overload msgbuf_t *MsgBuf_New (void *data, int size);
msgbuf_t *MsgBuf_New (int size);
void MsgBuf_Delete (msgbuf_t *msgbuf);
void MsgBuf_FromFile (msgbuf_t *msgbuf, QFile file);
int MsgBuf_MaxSize (msgbuf_t *msgbuf);
int MsgBuf_CurSize (msgbuf_t *msgbuf);
int MsgBuf_ReadCount (msgbuf_t *msgbuf);
string MsgBuf_DataPtr (msgbuf_t *msgbuf);

void MsgBuf_Clear (msgbuf_t *msgbuf);
void MsgBuf_WriteByte (msgbuf_t *msgbuf, int val);
void MsgBuf_WriteShort (msgbuf_t *msgbuf, int val);
void MsgBuf_WriteLong (msgbuf_t *msgbuf, int val);
void MsgBuf_WriteFloat (msgbuf_t *msgbuf, float val);
void MsgBuf_WriteString (msgbuf_t *msgbuf, string str);
void MsgBuf_WriteCoord (msgbuf_t *msgbuf, float coord);
void MsgBuf_WriteCoordV (msgbuf_t *msgbuf, vector coord);
void MsgBuf_WriteCoordAngleV (msgbuf_t *msgbuf, vector coords,
							  vector angles);
void MsgBuf_WriteAngle (msgbuf_t *msgbuf, float angle);
void MsgBuf_WriteAngleV (msgbuf_t *msgbuf, vector angles);
void MsgBuf_WriteAngle16 (msgbuf_t *msgbuf, float angle);
void MsgBuf_WriteAngle16V (msgbuf_t *msgbuf, vector angles);
void MsgBuf_WriteUTF8 (msgbuf_t *msgbuf, int val);

void MsgBuf_BeginReading (msgbuf_t *msgbuf);
int MsgBuf_ReadByte (msgbuf_t *msgbuf);
int MsgBuf_ReadShort (msgbuf_t *msgbuf);
int MsgBuf_ReadLong (msgbuf_t *msgbuf);
float MsgBuf_ReadFloat (msgbuf_t *msgbuf);
string MsgBuf_ReadString (msgbuf_t *msgbuf);
float MsgBuf_ReadCoord (msgbuf_t *msgbuf);
vector MsgBuf_ReadCoordV (msgbuf_t *msgbuf);
void MsgBuf_ReadCoordAngleV (msgbuf_t *msgbuf, vector *coords,
							 vector *angles);
float MsgBuf_ReadAngle (msgbuf_t *msgbuf);
vector MsgBuf_ReadAngleV (msgbuf_t *msgbuf);
float MsgBuf_ReadAngle16 (msgbuf_t *msgbuf);
vector MsgBuf_ReadAngle16V (msgbuf_t *msgbuf);
int MsgBuf_ReadUTF8 (msgbuf_t *msgbuf);

#endif//__ruamoko_msgbuf_h
