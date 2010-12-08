#ifndef CLAYLAND_H
#define CLAYLAND_H

#include <glib.h>

GSource *wl_glib_source_new(struct wl_event_loop *loop);

int dri2_connect(void);
int dri2_authenticate(uint32_t magic);

#endif /* CLAYLAND_H */
