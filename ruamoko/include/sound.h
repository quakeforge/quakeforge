#ifndef __ruamoko_sound_h
#define __ruamoko_sound_h

#define CHAN_AUTO 0
#define CHAN_WEAPON 1
#define CHAN_VOICE 2
#define CHAN_ITEM 3
#define CHAN_BODY 4
#define CHAN_NO_PHS_ADD 8

#define ATTN_NONE 0
#define ATTN_NORM 1
#define ATTN_IDLE 2
#define ATTN_STATIC 3

@extern void (entity e, float chan, string samp, float vol, float atten) sound;
@extern void (vector pos, string samp, float vol, float atten) ambientsound;

#endif//__ruamoko_sound_h
