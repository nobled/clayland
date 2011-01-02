#ifndef CLAYLAND_H
#define CLAYLAND_H

#include <clutter/clutter.h>
#include <EGL/egl.h>
#include <glib.h>
#include <glib-object.h>
#include <wayland-server.h>

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

GType clayland_compositor_get_type(void);
GType clayland_surface_get_type(void);

typedef struct ClaylandCompositor {
	GObject			 object;
	ClutterActor		*hand;
	ClutterActor		*stage;
	GSource			*source;
	struct wl_display	*display;
	struct wl_event_loop	*loop;

	struct wl_compositor	 compositor;

	EGLDisplay		 egl_display;

	gint stage_width;
	gint stage_height;
} ClaylandCompositor;

typedef struct ClaylandCompositorClass {
	GObjectClass		 object_class;
} ClaylandCompositorClass;


typedef struct ClaylandSurface {
	ClutterActor		 actor;
	struct wl_surface	 surface;
	ClaylandCompositor	*compositor;
	ClutterActor		*hand;
} ClaylandSurface;

typedef struct ClaylandSurfaceClass {
	ClutterActorClass	 actor_class;
} ClaylandSurfaceClass;

#endif /* CLAYLAND_H */
