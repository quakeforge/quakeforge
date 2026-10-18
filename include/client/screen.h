#ifndef __client_screen_h
#define __client_screen_h

typedef struct dstring_s dstring_t;
typedef struct cbuf_s cbuf_t;
typedef struct view_s view_t;
typedef struct canvas_system_s canvas_system_t;
typedef struct viewstate_s viewstate_t;

extern view_t cl_screen_view;
extern canvas_system_t cl_canvas_sys;

typedef struct clscr_data_s {
	dstring_t  *dl_name;
	int        *dl_percent;
	double     *realtime;
	double     *frametime;
	void      (*quit) (void);
	cbuf_t     *cbuf;
	int        *stats;
	float      *item_gettime;
} clscr_data_t;

void CL_Init_Screen (clscr_data_t *clscr_data);
void CL_UpdateScreen (viewstate_t *vs);

#endif//__client_screen_h
