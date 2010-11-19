#ifndef CLAYLAND_H
#define CLAYLAND_H

#include <glib.h>

GSource *wl_glib_source_new(struct wl_event_loop *loop);

#endif /* CLAYLAND_H */
