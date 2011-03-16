/* Clayland.
 * A library for building Wayland compositors.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 */

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

#if defined(COGL_HAS_GL)
#include <GL/gl.h>
#elif defined(COGL_HAS_GLES2)
#include <GLES2/gl2.h>
#elif defined(COGL_HAS_GLES1)
#include <GLES/gl.h>
#endif

#include "clayland-config.h"

#define CLAYLAND_COMPOSITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLAYLAND_TYPE_COMPOSITOR, ClaylandCompositorClass))
#define CLAYLAND_IS_COMPOSITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLAYLAND_TYPE_COMPOSITOR))
#define CLAYLAND_COMPOSITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLAYLAND_TYPE_COMPOSITOR, ClaylandCompositorClass))

#define CLAYLAND_TYPE_OUTPUT            (clayland_output_get_type ())
#define CLAYLAND_OUTPUT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLAYLAND_TYPE_OUTPUT, ClaylandOutput))
#define CLAYLAND_OUTPUT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLAYLAND_TYPE_OUTPUT, ClaylandOutputClass))
#define CLAYLAND_IS_OUTPUT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLAYLAND_TYPE_OUTPUT))
#define CLAYLAND_IS_OUTPUT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLAYLAND_TYPE_OUTPUT))
#define CLAYLAND_OUTPUT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLAYLAND_TYPE_OUTPUT, ClaylandOutputClass))

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

typedef struct _ClaylandCompositorClass ClaylandCompositorClass;
typedef struct _ClaylandOutput ClaylandOutput;
typedef struct _ClaylandOutputClass ClaylandOutputClass;
typedef struct _ClaylandSurface ClaylandSurface;
typedef struct _ClaylandSurfaceClass ClaylandSurfaceClass;
typedef struct _ClaylandBuffer ClaylandBuffer;
typedef struct _ClaylandBufferClass ClaylandBufferClass;

enum visual {
VISUAL_UNKNOWN,
VISUAL_ARGB_PRE,
VISUAL_ARGB,
VISUAL_RGB
};

#if defined(HAVE_DRI2_X11)
G_GNUC_INTERNAL
int dri2_x11_connect(ClaylandCompositor *compositor);
G_GNUC_INTERNAL
int dri2_x11_authenticate(ClaylandCompositor *compositor, uint32_t magic);
#endif

static inline
int dri2_connect(ClaylandCompositor *compositor)
{
#if defined(HAVE_DRI2_X11)
return dri2_x11_connect(compositor);
#else
return -1;
#endif
}

static inline
int dri2_authenticate(ClaylandCompositor *compositor, uint32_t magic)
{
#if defined(HAVE_DRI2_X11)
return dri2_x11_authenticate(compositor, magic);
#else
return -1;
#endif
}

G_GNUC_INTERNAL
extern const struct wl_surface_interface clayland_surface_interface;
G_GNUC_INTERNAL
extern const struct wl_drm_interface clayland_drm_interface;
G_GNUC_INTERNAL
extern const struct wl_shm_interface clayland_shm_interface;
G_GNUC_INTERNAL
extern const struct wl_shell_interface clayland_shell_interface;

G_GNUC_INTERNAL void
_clayland_add_output(ClaylandCompositor *compositor,
                     ClutterContainer *container);
G_GNUC_INTERNAL void
_clayland_add_devices(ClaylandCompositor *compositor);
G_GNUC_INTERNAL void
_clayland_add_buffer_interfaces(ClaylandCompositor *compositor);

G_GNUC_INTERNAL ClutterActor *
_clayland_output_get_container(ClaylandOutput *output);

G_GNUC_INTERNAL CoglPixelFormat
_clayland_init_buffer(ClaylandBuffer *cbuffer,
                      ClaylandCompositor *compositor,
                      uint32_t id, int32_t width, int32_t height,
                      struct wl_visual *visual);

G_GNUC_INTERNAL
GType clayland_output_get_type(void);
G_GNUC_INTERNAL
GType clayland_surface_get_type(void);
G_GNUC_INTERNAL
GType clayland_buffer_get_type(void);

struct _ClaylandCompositor {
	GObject			 object;
	ClaylandOutput		*output;
	struct wl_display	*display;

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
	/* for dri2 on x11: */
	gpointer		 xconn;
	gulong			 root_xwindow;
};

struct _ClaylandCompositorClass {
	GObjectClass		 object_class;
};

struct constraints {
	ClutterConstraint *x, *y, *w, *h;
};

#define SURFACE_IS_FULLSCREEN(cs) \
	((cs)->constraints.w != NULL)

#define SURFACE_IS_TRANSIENT(cs) \
	((!SURFACE_IS_FULLSCREEN(cs)) && \
	((cs)->constraints.x != NULL))

#define SURFACE_GET_TRANSIENT_PARENT(cs) \
	(SURFACE_IS_TRANSIENT(cs) ? \
	clutter_bind_constraint_get_source( \
	 CLUTTER_BIND_CONSTRAINT((cs)->constraints.x)) : NULL)

struct _ClaylandSurface {
	ClutterTexture		 texture;
	struct wl_surface	 surface;
	ClaylandBuffer		*buffer;
	ClaylandCompositor	*compositor;
	struct constraints	 constraints;
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
