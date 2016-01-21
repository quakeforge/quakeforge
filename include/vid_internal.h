#ifndef __vid_internal_h
#define __vid_internal_h

#include "QF/vid.h"
#include "QF/plugin/vid_render.h"

extern struct cvar_s *vid_fullscreen;
extern struct cvar_s *vid_system_gamma;
extern struct cvar_s *vid_gamma;

extern unsigned short  sw32_8to16table[256];

void VID_GetWindowSize (int def_w, int def_h);

void VID_InitGamma (unsigned char *);
qboolean VID_SetGamma (double);
void VID_UpdateGamma (struct cvar_s *);

void VID_Update (vrect_t *rects);
void VID_LockBuffer (void);
void VID_UnlockBuffer (void);
void VID_InitBuffers (void);
void VID_MakeColormaps (void);


#endif//__vid_internal_h
