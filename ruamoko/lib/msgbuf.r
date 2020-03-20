#include <msgbuf.h>

msgbuf_t *MsgBuf_New (int size) = #0;
void MsgBuf_Delete (msgbuf_t *msgbuf) = #0;
void MsgBuf_FromFile (msgbuf_t *msgbuf, QFile file) = #0;
int MsgBuf_MaxSize (msgbuf_t *msgbuf) = #0;
int MsgBuf_CurSize (msgbuf_t *msgbuf) = #0;
int MsgBuf_ReadCount (msgbuf_t *msgbuf) = #0;
string MsgBuf_DataPtr (msgbuf_t *msgbuf) = #0;

void MsgBuf_Clear (msgbuf_t *msgbuf) = #0;
void MsgBuf_WriteByte (msgbuf_t *msgbuf, int val) = #0;
void MsgBuf_WriteShort (msgbuf_t *msgbuf, int val) = #0;
void MsgBuf_WriteLong (msgbuf_t *msgbuf, int val) = #0;
void MsgBuf_WriteFloat (msgbuf_t *msgbuf, float val) = #0;
void MsgBuf_WriteString (msgbuf_t *msgbuf, string str) = #0;
void MsgBuf_WriteCoord (msgbuf_t *msgbuf, float coord) = #0;
void MsgBuf_WriteCoordV (msgbuf_t *msgbuf, vector coord) = #0;
void MsgBuf_WriteCoordAngleV (msgbuf_t *msgbuf, vector coords,
									  vector angles) = #0;
void MsgBuf_WriteAngle (msgbuf_t *msgbuf, float angle) = #0;
void MsgBuf_WriteAngleV (msgbuf_t *msgbuf, vector angles) = #0;
void MsgBuf_WriteAngle16 (msgbuf_t *msgbuf, float angle) = #0;
void MsgBuf_WriteAngle16V (msgbuf_t *msgbuf, vector angles) = #0;
void MsgBuf_WriteUTF8 (msgbuf_t *msgbuf, int val) = #0;

void MsgBuf_BeginReading (msgbuf_t *msgbuf) = #0;
int MsgBuf_ReadByte (msgbuf_t *msgbuf) = #0;
int MsgBuf_ReadShort (msgbuf_t *msgbuf) = #0;
int MsgBuf_ReadLong (msgbuf_t *msgbuf) = #0;
float MsgBuf_ReadFloat (msgbuf_t *msgbuf) = #0;
string MsgBuf_ReadString (msgbuf_t *msgbuf) = #0;
float MsgBuf_ReadCoord (msgbuf_t *msgbuf) = #0;
vector MsgBuf_ReadCoordV (msgbuf_t *msgbuf) = #0;
void MsgBuf_ReadCoordAngleV (msgbuf_t *msgbuf, vector *coords,
									 vector *angles) = #0;
float MsgBuf_ReadAngle (msgbuf_t *msgbuf) = #0;
vector MsgBuf_ReadAngleV (msgbuf_t *msgbuf) = #0;
float MsgBuf_ReadAngle16 (msgbuf_t *msgbuf) = #0;
vector MsgBuf_ReadAngle16V (msgbuf_t *msgbuf) = #0;
int MsgBuf_ReadUTF8 (msgbuf_t *msgbuf) = #0;
