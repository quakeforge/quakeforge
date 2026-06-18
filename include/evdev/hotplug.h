#ifndef evdev_hotplug_h
#define evdev_hotplug_h

typedef void (*hotplugfunc_t) (const char *);

typedef struct hotplug_s hotplug_t;

hotplug_t *inputlib_hotplug_init(const char *path, const char *prefix,
								 hotplugfunc_t created, hotplugfunc_t deleted);

void inputlib_hotplug_close (hotplug_t *hp);

int inputlib_hotplug_add_select (hotplug_t *hp, fd_set *fdset, int *maxfd);

int inputlib_hotplug_check_select (hotplug_t *hp, fd_set *fdset);

#endif//evdev_hotplug_h
