#ifndef CLAYLAND_PRIVATE_H
#define CLAYLAND_PRIVATE_H

#include "clayland.h"

#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <glib.h>
#include <glib-object.h>
#include <wayland-server.h>
#include <X11/Xlib-xcb.h>

#if defined(COGL_HAS_GL)
#include <GL/gl.h>
#elif defined(COGL_HAS_GLES2)
#include <GLES2/gl2.h>
#elif defined(COGL_HAS_GLES1)
#include <GLES/gl.h>
#endif

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

typedef struct _ClaylandSurface ClaylandSurface;
typedef struct _ClaylandSurfaceClass ClaylandSurfaceClass;
typedef struct _ClaylandBuffer ClaylandBuffer;
typedef struct _ClaylandBufferClass ClaylandBufferClass;

GSource *wl_glib_source_new(struct wl_event_loop *loop);

int dri2_connect(ClaylandCompositor *compositor);
int dri2_authenticate(ClaylandCompositor *compositor, uint32_t magic);

extern const struct wl_drm_interface clayland_drm_interface;
extern const struct wl_shm_interface clayland_shm_interface;

CoglPixelFormat
_clayland_init_buffer(ClaylandBuffer *cbuffer,
                      ClaylandCompositor *compositor,
                      uint32_t id, int32_t width, int32_t height,
                      struct wl_visual *visual);

GType clayland_surface_get_type(void);
GType clayland_buffer_get_type(void);

struct _ClaylandCompositor {
	GObject			 object;
	ClutterActor		*stage;
	gulong			 event_handler_id;
	GSource			*source;
	struct wl_display	*display;
	struct wl_event_loop	*loop;

	struct wl_compositor	 compositor;
	struct wl_object	 shm_object;
	struct wl_object	 drm_object;

	/* We implement the shell interface. */
	struct wl_shell shell;

	EGLDisplay		 egl_display;
	PFNEGLCREATEIMAGEKHRPROC create_image;
	PFNEGLDESTROYIMAGEKHRPROC destroy_image;
	PFNGLEGLIMAGETARGETTEXTURE2DOESPROC image2tex;

	int			 drm_fd;
	char			*drm_path;
	xcb_connection_t	*xconn;
	Window			 root_window;
};

struct _ClaylandCompositorClass {
	GObjectClass		 object_class;
};


struct _ClaylandSurface {
	ClutterTexture		 texture;
	struct wl_surface	 surface;
	ClaylandBuffer		*buffer;
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

#endif /* CLAYLAND_PRIVATE_H */