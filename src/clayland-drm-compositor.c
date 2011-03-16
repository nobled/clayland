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

#include <clutter/clutter.h>
#if defined(CLUTTER_WINDOWING_EGL)
#include <clutter/egl/clutter-egl.h>
#endif

#include "clayland-drm.h"

#include <string.h>

G_DEFINE_TYPE (ClaylandDRMCompositor, clayland_drm_compositor, CLAYLAND_TYPE_COMPOSITOR);

static void
clayland_drm_compositor_finalize (GObject *object)
{
	ClaylandDRMCompositor *drm = CLAYLAND_DRM_COMPOSITOR(object);

	g_debug("finalizing compositor %p of type '%s'", object,
	        G_OBJECT_TYPE_NAME(object));

	if (drm->drm_fd >= 0)
		(void) close(drm->drm_fd);
	if (drm->drm_path != NULL)
		g_free(drm->drm_path);

	G_OBJECT_CLASS (clayland_drm_compositor_parent_class)->finalize (object);
}

static void
clayland_drm_compositor_add_interfaces(ClaylandCompositor *compositor);

static void
clayland_drm_compositor_class_init (ClaylandDRMCompositorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ClaylandCompositorClass *compositor_class =
	        CLAYLAND_COMPOSITOR_CLASS (klass);

	object_class->finalize = clayland_drm_compositor_finalize;
	compositor_class->add_interfaces =
	        clayland_drm_compositor_add_interfaces;
}

static void
clayland_drm_compositor_init (ClaylandDRMCompositor *drm)
{
	drm->egl_display = EGL_NO_DISPLAY;
	drm->drm_path = NULL;
	drm->drm_fd = -1;
	drm->xconn = NULL;
	drm->root_xwindow = 0UL;
}

static gboolean
has_extension(const char *extensions, size_t target_len, const char *target)
{
	const char *ext, *next;
	size_t len;

	ext = extensions;
	while (ext) {
		next = strchr(ext, ' ');
		/* if 'ext' is the last extension string,
		   include the target's null terminator in the comparison */
		if (next == NULL)
			len = target_len+1;
		else {
			len = (size_t)(next - ext);
			next++; /* point to the next extension string */
			if (len != target_len)
				goto no_match;
		}

		if (strncmp(target, ext, len) == 0)
			break;
no_match:
		ext = next;
	};

        if (ext)
                g_debug("  found extension: %s", target);
        else
                g_debug("missing extension: %s", target);

	return ext != NULL;
}

static void
post_drm_device(struct wl_client *client, struct wl_object *global)
{
	ClaylandDRMCompositor *drm =
	    container_of(global, ClaylandDRMCompositor, object);

	g_debug("Advertising DRM device: %s", drm->drm_path);

	wl_client_post_event(client, global, WL_DRM_DEVICE,
	                     drm->drm_path);
}

static void
clayland_drm_compositor_add_interfaces(ClaylandCompositor *compositor)
{
	ClaylandDRMCompositor *drm;
	ClaylandCompositorClass *parent_class;
	EGLDisplay edpy;
	const char *extensions, *glextensions;
	gboolean ext;

	/* chain up to parent */
	parent_class = clayland_drm_compositor_parent_class;
	parent_class->add_interfaces(compositor);

	drm = CLAYLAND_DRM_COMPOSITOR(compositor);

	if (dri2_connect(drm) < 0) {
		g_debug("Could not connect to DRI2, disabling DRM buffers");
		return;
	}
#if defined(CLUTTER_WINDOWING_X11) && defined(CLUTTER_WINDOWING_EGL)
	edpy = clutter_egl_display ();
#else
	edpy = EGL_NO_DISPLAY;
#endif
	extensions = eglQueryString(edpy, EGL_EXTENSIONS);
	glextensions = (const char*)glGetString(GL_EXTENSIONS);
	ext = has_extension(extensions, 18, "EGL_MESA_drm_image");
	ext = has_extension(extensions, 18, "EGL_KHR_image_base") && ext;
	ext = has_extension(glextensions, 16, "GL_OES_EGL_image") && ext;

	if (ext == FALSE)
		return;

	drm->create_image =
	    (PFNEGLCREATEIMAGEKHRPROC)
	        eglGetProcAddress("eglCreateImageKHR");
	drm->destroy_image =
	    (PFNEGLDESTROYIMAGEKHRPROC)
	        eglGetProcAddress("eglDestroyImageKHR");
	drm->image2tex =
	    (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)
	        eglGetProcAddress("glEGLImageTargetTexture2DOES");

	if (!(drm->create_image) || !(drm->destroy_image)
	    || !(drm->image2tex)) {
		g_warning("failed to get function pointers");
		return;
	}

	drm->object.interface = &wl_drm_interface;
	drm->object.implementation =
	    (void (**)(void)) &clayland_drm_interface;
	wl_display_add_object(compositor->display, &drm->object);

	if (wl_display_add_global(compositor->display, &drm->object,
	                          post_drm_device)) {
		g_warning("failed to add drm global object");
		return;
	}

	drm->egl_display = edpy;
	g_debug("egl display %p", edpy);
}

