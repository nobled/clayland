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

#include <cogl/cogl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "clayland-drm.h"

#define CLAYLAND_TYPE_DRM_BUFFER            (clayland_drm_buffer_get_type ())
#define CLAYLAND_DRM_BUFFER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLAYLAND_TYPE_DRM_BUFFER, ClaylandDrmBuffer))
#define CLAYLAND_DRM_BUFFER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLAYLAND_TYPE_DRM_BUFFER, ClaylandDrmBufferClass))
#define CLAYLAND_IS_DRM_BUFFER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLAYLAND_TYPE_DRM_BUFFER))
#define CLAYLAND_IS_DRM_BUFFER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLAYLAND_TYPE_DRM_BUFFER))
#define CLAYLAND_DRM_BUFFER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLAYLAND_TYPE_DRM_BUFFER, ClaylandDrmBufferClass))

typedef struct _ClaylandDrmBuffer ClaylandDrmBuffer;
typedef struct _ClaylandDrmBufferClass ClaylandDrmBufferClass;

struct _ClaylandDrmBuffer {
	ClaylandBuffer		 cbuffer;
	GLuint			 gltex_handle;
};

struct _ClaylandDrmBufferClass {
	ClaylandBufferClass	 cbuffer_class;
};

G_DEFINE_TYPE (ClaylandDrmBuffer, clayland_drm_buffer, CLAYLAND_TYPE_BUFFER);

static void
clayland_drm_buffer_finalize (GObject *object)
{
	ClaylandDrmBuffer *buffer = CLAYLAND_DRM_BUFFER(object);

	G_OBJECT_CLASS (clayland_drm_buffer_parent_class)->finalize (object);
	/* Cogl doesn't delete foreign texture handles itself on destruction,
	   so we need to delete this handle ourselves - *after* the above call
	   deletes the final reference to the CoglHandle. */
	if (buffer->gltex_handle != 0)
		glDeleteTextures(1, &buffer->gltex_handle);
}

static void
clayland_drm_buffer_class_init (ClaylandDrmBufferClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = clayland_drm_buffer_finalize;
}

static void
clayland_drm_buffer_init (ClaylandDrmBuffer *buffer)
{
	buffer->gltex_handle = 0;
}

static void
drm_buffer_create(struct wl_client *client, struct wl_drm *_drm,
                  uint32_t id, uint32_t name, int32_t width, int32_t height,
                  uint32_t stride, struct wl_visual *visual)
{
	ClaylandDRMCompositor *drm =
	    container_of((struct wl_object *)_drm, ClaylandDRMCompositor, object);
	ClaylandDrmBuffer *buffer;
	EGLImageKHR image;
	EGLint attribs[] = {
		EGL_WIDTH,  0,
		EGL_HEIGHT, 0,
		EGL_DRM_BUFFER_STRIDE_MESA, 0,
		EGL_DRM_BUFFER_FORMAT_MESA, EGL_DRM_BUFFER_FORMAT_ARGB32_MESA,
		EGL_NONE
	};
	CoglPixelFormat pformat;

	buffer = g_object_new(CLAYLAND_TYPE_DRM_BUFFER, NULL);
	if (buffer == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	pformat = _clayland_init_buffer(&buffer->cbuffer, &drm->compositor,
	                                id, width, height, visual);
	if (pformat == COGL_PIXEL_FORMAT_ANY) {
		/* XXX: report error? */
		g_object_unref(buffer);
		return;
	}

	attribs[1] = width;
	attribs[3] = height;
	attribs[5] = stride / 4; /* convert from bytes to pixels */

	image = drm->create_image(drm->egl_display,
	                          EGL_NO_CONTEXT,
	                          EGL_DRM_BUFFER_MESA,
	                          (EGLClientBuffer)name, attribs);
	if (image == EGL_NO_IMAGE_KHR) {
		g_warning("buffer %p: failed to create EGLImage", buffer);
		/* XXX: report error? */
		g_object_unref(buffer);
		return;
	}

	glGenTextures(1, &buffer->gltex_handle);
	glBindTexture(GL_TEXTURE_2D, buffer->gltex_handle);
	drm->image2tex(GL_TEXTURE_2D, image);
	drm->destroy_image(drm->egl_display, image);

	buffer->cbuffer.tex_handle =
	    cogl_texture_new_from_foreign(buffer->gltex_handle, GL_TEXTURE_2D,
	                (GLuint)width, (GLuint)height, 0, 0, pformat);

	if (buffer->cbuffer.tex_handle == COGL_INVALID_HANDLE) {
		g_warning("buffer %p: failed to create CoglHandle", buffer);
		g_object_unref(buffer);
		return;
	}

	wl_client_add_resource(client, &buffer->cbuffer.buffer.resource);
}

static void
drm_authenticate(struct wl_client *client, struct wl_drm *_drm, uint32_t magic)
{
	ClaylandDRMCompositor *drm =
	    container_of((struct wl_object *)_drm, ClaylandDRMCompositor, object);

	if (dri2_authenticate(drm, magic) < 0) {
		wl_client_post_event(client,
				(struct wl_object *)drm->compositor.display,
				WL_DISPLAY_INVALID_OBJECT, 0);
		return;
	}

	wl_client_post_event(client, &drm->object,
	                        WL_DRM_AUTHENTICATED);
}


const struct wl_drm_interface clayland_drm_interface = {
	drm_authenticate,
	drm_buffer_create
};

