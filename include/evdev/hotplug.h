#ifndef evdev_hotplug_h
#define evdev_hotplug_h

int inputlib_hotplug_init(const char *path,
						  void (*created) (const char*),
						  void (*deleted) (const char *));

void inputlib_hotplug_close (void);

int inputlib_hotplug_add_select (fd_set *fdset, int *maxfd);

int inputlib_hotplug_check_select (fd_set *fdset);

#endif//evdev_hotplug_h
