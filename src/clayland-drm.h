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

#ifndef CLAYLAND_DRM_H
#define CLAYLAND_DRM_H

#include <cogl/cogl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#if defined(COGL_HAS_GL)
#include <GL/gl.h>
#elif defined(COGL_HAS_GLES2)
#include <GLES2/gl2.h>
#elif defined(COGL_HAS_GLES1)
#include <GLES/gl.h>
#endif

#include "clayland.h"
#include "clayland-private.h"
#include "clayland-config.h"

#define CLAYLAND_TYPE_DRM_COMPOSITOR            (clayland_drm_compositor_get_type ())
#define CLAYLAND_DRM_COMPOSITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLAYLAND_TYPE_DRM_COMPOSITOR, ClaylandDRMCompositor))
#define CLAYLAND_IS_DRM_COMPOSITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLAYLAND_TYPE_DRM_COMPOSITOR))
#define CLAYLAND_DRM_COMPOSITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLAYLAND_TYPE_DRM_COMPOSITOR, ClaylandDRMCompositorClass))
#define CLAYLAND_IS_DRM_COMPOSITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLAYLAND_TYPE_DRM_COMPOSITOR))
#define CLAYLAND_DRM_COMPOSITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLAYLAND_TYPE_DRM_COMPOSITOR, ClaylandDRMCompositorClass))

typedef struct _ClaylandDRMCompositor ClaylandDRMCompositor;
typedef struct _ClaylandDRMCompositorClass ClaylandDRMCompositorClass;

G_GNUC_INTERNAL
extern const struct wl_drm_interface clayland_drm_interface;

#if defined(HAVE_DRI2_X11)
G_GNUC_INTERNAL
int dri2_x11_connect(ClaylandDRMCompositor *compositor);
G_GNUC_INTERNAL
int dri2_x11_authenticate(ClaylandDRMCompositor *compositor, uint32_t magic);
#endif

static inline
int dri2_connect(ClaylandDRMCompositor *drm)
{
#if defined(HAVE_DRI2_X11)
return dri2_x11_connect(drm);
#else
return -1;
#endif
}

static inline
int dri2_authenticate(ClaylandDRMCompositor *drm, uint32_t magic)
{
#if defined(HAVE_DRI2_X11)
return dri2_x11_authenticate(drm, magic);
#else
return -1;
#endif
}

struct _ClaylandDRMCompositor {
	ClaylandCompositor	 compositor;
	struct wl_object	 object; /* drm object */

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

struct _ClaylandDRMCompositorClass {
	ClaylandCompositorClass		 compositor_class;
};

#endif /* CLAYLAND_DRM_H */
