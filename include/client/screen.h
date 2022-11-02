#ifndef __client_screen_h
#define __client_screen_h

extern struct view_s cl_screen_view;

struct viewstate_s;
void CL_Init_Screen (void);
void CL_UpdateScreen (struct viewstate_s *vs);

#endif//__client_screen_h
