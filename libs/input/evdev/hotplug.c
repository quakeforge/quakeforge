#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <alloca.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "evdev/hotplug.h"

typedef struct hotplug_s {
	int         fd;
	int         wd;
	char       *path;
	char       *prefix;
	int         prefix_len;
	hotplugfunc_t created;
	hotplugfunc_t deleted;
} hotplug_t;

static unsigned
get_queue_size (int fd)
{
	unsigned queue_len;
	int ret = ioctl (fd, FIONREAD, &queue_len);
	if (ret < 0) {
		perror ("ioctl");
		return 0;
	}
	return queue_len;
}

static void
parse_inotify_events (hotplug_t *hp)
{
	ssize_t len, i;
	ssize_t queue_len = get_queue_size (hp->fd);
	char *buf = alloca (queue_len);
	struct inotify_event *event;

	len = read (hp->fd, buf, queue_len);
	for (i = 0; i < len; i += sizeof (struct inotify_event) + event->len) {
		event = (struct inotify_event *) &buf[i];
		//printf ("%-3d %08x %5u %4d %s\n", event->wd, event->mask,
		//		event->cookie, event->len,
		//		event->len ? event->name : "<no name>");
		if ((event->mask & IN_DELETE) && event->len) {
			if (strncmp (event->name, hp->prefix, hp->prefix_len) == 0) {
				// interested in only matching devices
				//printf("deleted device %s\n", event->name);
				hp->deleted (event->name);
			}
		}
		if (((event->mask & IN_ATTRIB) || (event->mask & IN_CREATE))
			&& event->len) {
			// done this way because may not have read permission when the
			// device is created, so try again when (presumabely) permission is
			// granted
			if (strncmp (event->name, hp->prefix, hp->prefix_len) == 0) {
				// interested in only matching devices
				//printf("created device %s\n", event->name);
				hp->created (event->name);
			}
		}
	}
}

hotplug_t *
inputlib_hotplug_init(const char *path, const char *prefix,
					  void (*created) (const char *),
					  void (*deleted) (const char *))
{
	int fd = inotify_init ();
	if (fd == -1) {
		perror ("inotify_init");
		return nullptr;
	}
	uint32_t mask = IN_CREATE | IN_DELETE | IN_ATTRIB | IN_ONLYDIR;
	int wd = inotify_add_watch (fd, path, mask);
	if (wd == -1) {
		perror ("inotify_add_watch");
		close (fd);
		return nullptr;
	}
	hotplug_t  *hp = malloc (sizeof (hotplug_t));
	*hp = (hotplug_t) {
		.fd = fd,
		.wd = wd,
		.path = strdup (path),
		.prefix = strdup (prefix),
		.prefix_len = strlen (prefix),
		.created = created,
		.deleted = deleted,
	};
	//printf ("inputlib_hotplug_init: %s %d %d\n", path, fd, wd);
	return hp;
}

void
inputlib_hotplug_close (hotplug_t *hp)
{
	if (hp) {
		close (hp->fd);
		free (hp->path);
		free (hp->prefix);
		free (hp);
	}
}

int
inputlib_hotplug_add_select (hotplug_t *hp, fd_set *fdset, int *maxfd)
{
	if (hp) {
		FD_SET (hp->fd, fdset);
		if (hp->fd > *maxfd) {
			*maxfd = hp->fd;
		}
	}
	return hp->fd;
}

int
inputlib_hotplug_check_select (hotplug_t *hp, fd_set *fdset)
{
	if (hp) {
		if (FD_ISSET (hp->fd, fdset)) {
			parse_inotify_events (hp);
			return 1;
		}
	}
	return 0;
}
