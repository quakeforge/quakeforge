#ifndef __tempent_h
#define __tempent_h

#define TE_SPIKE 0
#define TE_SUPERSPIKE 1
#define TE_GUNSHOT 2
#define TE_EXPLOSION 3
#define TE_TAREXPLOSION 4
#define TE_LIGHTNING1 5
#define TE_LIGHTNING2 6
#define TE_WIZSPIKE 7
#define TE_KNIGHTSPIKE 8
#define TE_LIGHTNING3 9
#define TE_LAVASPLASH 10
#define TE_TELEPORT 11
#define TE_BLOOD 12
#define TE_LIGHTNINGBLOOD 13

@extern void(vector org, float damage) SpawnBlood;

#endif//__tempent_h
