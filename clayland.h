#ifndef CLAYLAND_H
#define CLAYLAND_H

#include <clutter/clutter.h>
#include <cogl/cogl.h>
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

#define CLAYLAND_TYPE_BUFFER            (clayland_buffer_get_type ())
#define CLAYLAND_BUFFER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLAYLAND_TYPE_BUFFER, ClaylandBuffer))
#define CLAYLAND_BUFFER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLAYLAND_TYPE_BUFFER, ClaylandBufferClass))
#define CLAYLAND_IS_BUFFER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLAYLAND_TYPE_BUFFER))
#define CLAYLAND_IS_BUFFER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLAYLAND_TYPE_BUFFER))
#define CLAYLAND_BUFFER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLAYLAND_TYPE_BUFFER, ClaylandBufferClass))

typedef struct _ClaylandCompositor ClaylandCompositor;
typedef struct _ClaylandCompositorClass ClaylandCompositorClass;
typedef struct _ClaylandSurface ClaylandSurface;
typedef struct _ClaylandSurfaceClass ClaylandSurfaceClass;
typedef struct _ClaylandBuffer ClaylandBuffer;
typedef struct _ClaylandBufferClass ClaylandBufferClass;

GSource *wl_glib_source_new(struct wl_event_loop *loop);

int dri2_connect(void);
int dri2_authenticate(uint32_t magic);

extern const struct wl_shm_interface clayland_shm_interface;

CoglPixelFormat
_clayland_init_buffer(ClaylandBuffer *cbuffer,
                      ClaylandCompositor *compositor,
                      uint32_t id, int32_t width, int32_t height,
                      struct wl_visual *visual);

GType clayland_compositor_get_type(void);
GType clayland_surface_get_type(void);
GType clayland_buffer_get_type(void);

struct _ClaylandCompositor {
	GObject			 object;
	ClutterActor		*hand;
	ClutterActor		*stage;
	GSource			*source;
	struct wl_display	*display;
	struct wl_event_loop	*loop;

	struct wl_compositor	 compositor;
	struct wl_object	 shm_object;

	EGLDisplay		 egl_display;

	gint stage_width;
	gint stage_height;
};

struct _ClaylandCompositorClass {
	GObjectClass		 object_class;
};


struct _ClaylandSurface {
	ClutterTexture		 texture;
	struct wl_surface	 surface;
	ClaylandCompositor	*compositor;
};

struct _ClaylandSurfaceClass {
	ClutterTextureClass	 texture_class;
};


struct _ClaylandBuffer {
	GObject			 object;
	CoglHandle		 tex_handle;
	struct wl_buffer	 buffer;
};

struct _ClaylandBufferClass {
	GObjectClass		 object_class;
};

#endif /* CLAYLAND_H */
