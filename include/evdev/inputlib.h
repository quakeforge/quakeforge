#ifndef evdev_inputlib_h
#define evdev_inputlib_h

typedef struct {
	int         num;	///< The high-level index of the button. Always 0-N
	int         evnum;	///< The low-level index of the button. May be sparse
	int         state;	///< Current state of the button.
} button_t;

typedef struct {
	int         num;	///< The high-level index of the axis. Always 0-N
	int         evnum;	///< The low-level index of the axis. May be sparse
	int         value;	///< Current value of the input axis.
	// relative axes set these to 0
	int         min;	///< Minimum value for the axis (usually constant).
	int         max;	///< Maximum value for the axis (usually constant).
} axis_t;

typedef struct device_s {
	struct device_s *next;
	struct device_s **prev;
	char       *path;
	char       *name;
	char       *phys;
	char       *uniq;
	int         fd;
	int         max_button;
	int        *button_map;
	int         num_buttons;
	button_t   *buttons;
	int         max_abs_axis;
	int        *abs_axis_map;
	int         max_rel_axis;
	int        *rel_axis_map;
	int         num_abs_axes;
	int         num_rel_axes;
	// includes both abs and rel axes, with abs first
	int         num_axes;
	axis_t     *axes;
	int         event_count;

	void       *data;
	void      (*axis_event) (axis_t *axis, void *data);
	void      (*button_event) (button_t *button, void *data);
} device_t;

void inputlib_add_select (fd_set *fdset, int *maxfd);
void inputlib_check_select (fd_set *fdset);
int inputlib_check_input (void);
void inputlib_close (void);
int inputlib_init (void (*dev_add) (device_t *), void (*dev_rem) (device_t *));

#endif//evdev_inputlib_h
