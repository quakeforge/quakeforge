#ifndef __message_h
#define __message_h

// protocol bytes  
#define SVC_TEMPENTITY 23
#define SVC_KILLEDMONSTER 27
#define SVC_FOUNDSECRET 28
#define SVC_INTERMISSION 30
#define SVC_FINALE 31
#define SVC_CDTRACK 32
#define SVC_SELLSCREEN 33
#define SVC_SMALLKICK 34
#define SVC_BIGKICK 35
#define SVC_MUZZLEFLASH 39

// messages
#define MSG_BROADCAST 0
#define MSG_ONE 1
#define MSG_ALL 2
#define MSG_INIT 3
#define MSG_MULTICAST 4

// message levels
#define PRINT_LOW 0
#define PRINT_MEDIUM 1
#define PRINT_HIGH 2
#define PRINT_CHAT 3

// multicast sets
#define MULTICAST_ALL 0
#define MULTICAST_PHS 1
#define MULTICAST_PVS 2
#define MULTICAST_ALL_R 3
#define MULTICAST_PHS_R 4
#define MULTICAST_PVS_R 5

@extern void (...) bprint;
@extern void (float to, float f) WriteByte;
@extern void (float to, float f) WriteChar;
@extern void (float to, float f) WriteShort;
@extern void (float to, float f) WriteLong;
@extern void (float to, float f) WriteCoord;
@extern void (float to, float f) WriteAngle;
@extern void (float to, string s) WriteString;
@extern void (float to, entity s) WriteEntity;
@extern void (...) centerprint;

#endif//__message_h
