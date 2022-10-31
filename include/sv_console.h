#ifndef __sv_console_h
#define __sv_console_h

enum {
	server_href,
	server_view,
	server_window,

	server_comp_count
};

struct view_s;

typedef struct sv_view_s {
	void       *win;
	void       *obj;
	void      (*draw) (struct view_s view);
	void      (*setgeometry) (struct view_s view);
} sv_view_t;

typedef struct sv_sbar_s {
	int         width;
	char       *text;
} sv_sbar_t;

#endif//__sv_console_h
