#include <message.h>

void (float level, string s) bprint = #23;
void (entity client, float level, string s) sprint = #24;
void (float to, float f) WriteByte = #52;
void (float to, float f) WriteChar = #53;
void (float to, float f) WriteShort = #54;
void (float to, float f) WriteLong = #55;
void (float to, float f) WriteCoord = #56;
void (float to, float f) WriteAngle = #57;
void (float to, string s) WriteString = #58;
void (float to, entity s) WriteEntity = #59;
void (float to, ...) WriteBytes = #0;
void (float to, vector v) WriteCoordV = #0;
void (float to, vector v) WriteAngleV = #0;
void (entity client, string s) centerprint = #73;
