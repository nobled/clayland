#ifndef CLAYLAND_H
#define CLAYLAND_H

#include <glib.h>

#define CLAYLAND_TYPE_COMPOSITOR            (clayland_compositor_get_type ())
#define CLAYLAND_COMPOSITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLAYLAND_TYPE_COMPOSITOR, ClaylandCompositor))
#define CLAYLAND_COMPOSITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLAYLAND_TYPE_COMPOSITOR, ClaylandCompositorClass))
#define CLAYLAND_IS_COMPOSITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLAYLAND_TYPE_COMPOSITOR))
#define CLAYLAND_IS_COMPOSITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLAYLAND_TYPE_COMPOSITOR))
#define CLAYLAND_COMPOSITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLAYLAND_TYPE_COMPOSITOR, ClaylandCompositorClass))

#define CLAYLAND_TYPE_SURFACE            (clayland_surface_get_type ())
#define CLAYLAND_SURFACE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLAYLAND_TYPE_SURFACE, ClaylandSurface))
#define CLAYLAND_SURFACE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLAYLAND_TYPE_SURFACE, ClaylandSurfaceClass))
#define CLAYLAND_IS_SURFACE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLAYLAND_TYPE_SURFACE))
#define CLAYLAND_IS_SURFACE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLAYLAND_TYPE_SURFACE))
#define CLAYLAND_SURFACE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLAYLAND_TYPE_SURFACE, ClaylandSurfaceClass))

GSource *wl_glib_source_new(struct wl_event_loop *loop);

int dri2_connect(void);
int dri2_authenticate(uint32_t magic);

#endif /* CLAYLAND_H */
