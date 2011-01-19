#include <stdlib.h>
#include <string.h>
#include <wayland-server.h>
#include <clutter/clutter.h>
#include <clutter/egl/clutter-egl.h>

#include <sys/time.h>
#include <math.h>
#include <errno.h>
#include <stdlib.h>
#include <glib.h>

#include "clayland.h"

G_DEFINE_TYPE (ClaylandCompositor, clayland_compositor, G_TYPE_OBJECT);

static void
clayland_compositor_finalize (GObject *object)
{
	ClaylandCompositor *compositor = CLAYLAND_COMPOSITOR(object);

	if (!clutter_stage_is_default(CLUTTER_STAGE(compositor->stage)))
		g_object_unref(compositor->stage);
	if (compositor->drm_fd >= 0)
		(void) close(compositor->drm_fd);
	if (compositor->drm_path != NULL)
		g_free(compositor->drm_path);

	G_OBJECT_CLASS (clayland_compositor_parent_class)->finalize (object);
}

static void
clayland_compositor_class_init (ClaylandCompositorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = clayland_compositor_finalize;
}

static void
clayland_compositor_init (ClaylandCompositor *compositor)
{
	compositor->stage = NULL;
	compositor->drm_path = NULL;
	compositor->drm_fd = -1;
}


G_DEFINE_TYPE (ClaylandSurface, clayland_surface, CLUTTER_TYPE_TEXTURE);

static void
clayland_surface_dispose (GObject *object)
{
	ClaylandSurface *csurface = CLAYLAND_SURFACE(object);
	ClutterActor *stage;

	G_OBJECT_CLASS (clayland_surface_parent_class)->dispose (object);

	if (csurface->compositor == NULL)
		return;
	stage = csurface->compositor->stage;
	clutter_container_remove_actor (CLUTTER_CONTAINER (stage),
					CLUTTER_ACTOR (csurface));
	g_object_unref(csurface->compositor);
	csurface->compositor = NULL;
}

static void
clayland_surface_finalize (GObject *object)
{
	ClaylandSurface *csurface = CLAYLAND_SURFACE(object);

	G_OBJECT_CLASS (clayland_surface_parent_class)->finalize (object);
	/* The above call has to come first so that the ClutterTexture class
	   releases its ref on the CoglHandle, making sure the last ref on the
	   CoglHandle is always held by the ClaylandBuffer. */
	if (csurface->buffer)
		g_object_unref(csurface->buffer);
}

static void
clayland_surface_class_init (ClaylandSurfaceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose  = clayland_surface_dispose;
	object_class->finalize = clayland_surface_finalize;
}

static void
clayland_surface_init (ClaylandSurface *surface)
{
	surface->buffer = NULL;
	surface->compositor = NULL;
}

G_DEFINE_TYPE (ClaylandBuffer, clayland_buffer, G_TYPE_OBJECT);

static void
clayland_buffer_finalize (GObject *object)
{
	ClaylandBuffer *cbuffer = CLAYLAND_BUFFER(object);

	if (cbuffer->tex_handle != COGL_INVALID_HANDLE)
		cogl_handle_unref(cbuffer->tex_handle);
	G_OBJECT_CLASS (clayland_buffer_parent_class)->finalize (object);
}

static void
clayland_buffer_class_init (ClaylandBufferClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = clayland_buffer_finalize;
}

static void
clayland_buffer_init (ClaylandBuffer *buffer)
{
	buffer->tex_handle = COGL_INVALID_HANDLE;
}

typedef struct ClaylandInputDevice {
	struct wl_input_device input_device;
	ClutterInputDevice *clutter_device;
} ClaylandInputDevice;


typedef struct _ClaylandMoveGrab {
	struct wl_grab grab;
	int32_t dx, dy;
} ClaylandMoveGrab;

static void
move_grab_motion(struct wl_grab *grab,
		   uint32_t time, int32_t x, int32_t y)
{
	ClaylandMoveGrab *move = (ClaylandMoveGrab *) grab;
	ClaylandSurface *cs =
		container_of(grab->input_device->pointer_focus,
			     ClaylandSurface, surface);

	clutter_actor_set_position (CLUTTER_ACTOR (cs),
				    x + move->dx, y + move->dy);
}

static void
move_grab_button(struct wl_grab *grab,
		 uint32_t time, int32_t button, int32_t state)
{
}

static void
move_grab_end(struct wl_grab *grab, uint32_t time)
{
	free(grab);
}

static const struct wl_grab_interface move_grab_interface = {
	move_grab_motion,
	move_grab_button,
	move_grab_end
};

static void
shell_move(struct wl_client *client, struct wl_shell *shell,
	   struct wl_surface *surface,
	   struct wl_input_device *device, uint32_t time)
{
	ClaylandSurface *cs = container_of(surface, ClaylandSurface, surface);
	ClaylandMoveGrab *move;
	gfloat x, y;

	move = malloc(sizeof *move);
	if (!move) {
		wl_client_post_no_memory(client);
		return;
	}

	clutter_actor_get_position (CLUTTER_ACTOR (cs), &x, &y);

	move->grab.interface = &move_grab_interface;
	move->dx = x - device->grab_x;
	move->dy = y - device->grab_y;

	if (wl_input_device_update_grab(device,
					&move->grab, surface, time) < 0)
		return;
}

typedef struct _ClaylandResizeGrab {
	struct wl_grab grab;
	uint32_t edges;
	int32_t dx, dy, width, height;
} ClaylandResizeGrab;

static void
resize_grab_motion(struct wl_grab *grab,
		   uint32_t time, int32_t x, int32_t y)
{
	ClaylandResizeGrab *resize = (ClaylandResizeGrab *) grab;
	struct wl_input_device *device = grab->input_device;
	ClaylandCompositor *compositor =
		container_of(device->compositor,
			     ClaylandCompositor, compositor);
	struct wl_surface *surface = device->pointer_focus;
	int32_t width, height;

	if (resize->edges & WL_GRAB_RESIZE_LEFT) {
		width = device->grab_x - x + resize->width;
	} else if (resize->edges & WL_GRAB_RESIZE_RIGHT) {
		width = x - device->grab_x + resize->width;
	} else {
		width = resize->width;
	}

	if (resize->edges & WL_GRAB_RESIZE_TOP) {
		height = device->grab_y - y + resize->height;
	} else if (resize->edges & WL_GRAB_RESIZE_BOTTOM) {
		height = y - device->grab_y + resize->height;
	} else {
		height = resize->height;
	}

	wl_client_post_event(surface->client, &compositor->shell.object,
			     WL_SHELL_CONFIGURE, time, resize->edges,
			     surface, width, height);
}

static void
resize_grab_button(struct wl_grab *grab,
		   uint32_t time, int32_t button, int32_t state)
{
}

static void
resize_grab_end(struct wl_grab *grab, uint32_t time)
{
	free(grab);
}

static const struct wl_grab_interface resize_grab_interface = {
	resize_grab_motion,
	resize_grab_button,
	resize_grab_end
};

static void
shell_resize(struct wl_client *client, struct wl_shell *shell,
	     struct wl_surface *surface,
	     struct wl_input_device *device, uint32_t time, uint32_t edges)
{
	ClaylandResizeGrab *resize;
	ClaylandSurface *cs = container_of(surface, ClaylandSurface, surface);
	gfloat x, y, width, height;

	resize = malloc(sizeof *resize);
	if (!resize) {
		wl_client_post_no_memory(client);
		return;
	}

	clutter_actor_get_position (CLUTTER_ACTOR (cs), &x, &y);
	clutter_actor_get_size (CLUTTER_ACTOR (cs), &width, &height);

	resize->grab.interface = &resize_grab_interface;
	resize->edges = edges;
	resize->dx = x - device->grab_x;
	resize->dy = y - device->grab_y;
	resize->width = width;
	resize->height = height;

	if (edges == 0 || edges > 15 ||
	    (edges & 3) == 3 || (edges & 12) == 12)
		return;

	if (wl_input_device_update_grab(device,
					&resize->grab, surface, time) < 0)
		return;
}

static void
shell_create_drag(struct wl_client *client,
		  struct wl_shell *shell, uint32_t id)
{
}

const static struct wl_shell_interface shell_interface = {
	shell_move,
	shell_resize,
	shell_create_drag
};


static void
motion_grab_motion(struct wl_grab *grab,
		   uint32_t time, int32_t x, int32_t y)
{
	ClaylandInputDevice *clayland_device =
		container_of(grab->input_device,
			     ClaylandInputDevice, input_device);
	ClaylandSurface *cs =
		container_of(grab->input_device->pointer_focus,
			     ClaylandSurface, surface);
	gfloat sx, sy;

	clutter_actor_transform_stage_point (CLUTTER_ACTOR (cs),
					     x, y, &sx, &sy);
	wl_client_post_event(cs->surface.client,
			     &clayland_device->input_device.object,
			     WL_INPUT_DEVICE_MOTION,
			     time, x, y, (int32_t) sx, (int32_t) sy);
}

static void
motion_grab_button(struct wl_grab *grab,
		   uint32_t time, int32_t button, int32_t state)
{
	wl_client_post_event(grab->input_device->pointer_focus->client,
			     &grab->input_device->object,
			     WL_INPUT_DEVICE_BUTTON,
			     time, button, state);
}

static void
motion_grab_end(struct wl_grab *grab, uint32_t time)
{
}

static const struct wl_grab_interface motion_grab_interface = {
	motion_grab_motion,
	motion_grab_button,
	motion_grab_end
};

static gboolean
event_cb (ClutterActor *stage, ClutterEvent *event, gpointer      data)
{
	const struct wl_grab_interface *interface;
	struct wl_input_device *device;
	ClutterInputDevice *clutter_device;
	ClaylandInputDevice *clayland_device;
	ClaylandSurface *cs;
	gfloat sx, sy;
	uint32_t state, button, key;

	clutter_device = clutter_event_get_device (event);
	if (clutter_device) {
		clayland_device =
			g_object_get_data (G_OBJECT(clutter_device),
					   "clayland");
		device = &clayland_device->input_device;
	}

	if (CLAYLAND_IS_SURFACE (event->any.source))
		cs = CLAYLAND_SURFACE (event->any.source);
	else
		cs = NULL;

	switch (event->type) {
	case CLUTTER_NOTHING:
	case CLUTTER_STAGE_STATE:
	case CLUTTER_DESTROY_NOTIFY:
	case CLUTTER_CLIENT_MESSAGE:
	case CLUTTER_DELETE:
	case CLUTTER_SCROLL:
 	default:
		return FALSE;

	case CLUTTER_KEY_PRESS:
	case CLUTTER_KEY_RELEASE:
		state = event->type == CLUTTER_KEY_PRESS ? 1 : 0;

		if (device->keyboard_focus == NULL)
			return FALSE;

		key = event->key.hardware_keycode - 8;
		wl_client_post_event(device->keyboard_focus->client,
				     &device->object,
				     WL_INPUT_DEVICE_KEY,
				     event->any.time, key, state);
		return TRUE;

	case CLUTTER_MOTION:
		device->x = event->motion.x;
		device->y = event->motion.y;

		if (device->grab) {
			/* FIXME: Need to pass cs to motion callback always. */
			interface = device->grab->interface;
			interface->motion(device->grab,
					  event->any.time,
					  device->x, device->y);
			return TRUE;
		}

		/* Not a clayland surface and we're not grabbing, so
		 * let clutter deliver the event. */
		if (cs == NULL)
			return FALSE;

		clutter_actor_transform_stage_point (event->any.source,
						     device->x,
						     device->y,
						     &sx, &sy);

		wl_input_device_set_pointer_focus(device,
						  &cs->surface,
						  event->any.time,
						  device->x,
						  device->y,
						  (int32_t) sx,
						  (int32_t) sy);
		wl_client_post_event(cs->surface.client,
				     &device->object,
				     WL_INPUT_DEVICE_MOTION,
				     event->any.time,
				     device->x,
				     device->y,
				     (int32_t) sx,
				     (int32_t) sy);
		return TRUE;

	case CLUTTER_ENTER:
		/* FIXME: Device is NULL on enter events? */
		g_debug("enter %p", event->any.source);
		return FALSE;

	case CLUTTER_LEAVE:
		g_debug("leave %p", event->any.source);
		return FALSE;

	case CLUTTER_BUTTON_PRESS:
	case CLUTTER_BUTTON_RELEASE:
		/* Not a clayland surface, let clutter deliver the event. */
		if (cs == NULL)
			return FALSE;

		state = event->type == CLUTTER_BUTTON_PRESS ? 1 : 0;
		button = event->button.button + 271;

		if (state && device->grab == NULL) {
			clutter_actor_raise_top (CLUTTER_ACTOR (cs));

			wl_input_device_start_grab(device,
						   &device->motion_grab,
						   button, event->any.time);
			wl_input_device_set_keyboard_focus(device,
							   &cs->surface,
							   event->any.time);
		}

		device->grab->interface->button(device->grab,
						event->any.time,
						event->button.button + 271,
						state);

		if (!state && device->grab && device->grab_button == button)
			wl_input_device_end_grab(device, event->any.time);

		return TRUE;
	}
}

static void
on_term_signal(int signal_number, void *data)
{
	clutter_main_quit();
}

static void
default_buffer_attach(struct wl_buffer *buffer, struct wl_surface *surface)
{
	ClaylandSurface *csurface =
		container_of(surface, ClaylandSurface, surface);
	ClaylandBuffer *cbuffer =
		container_of(buffer, ClaylandBuffer, buffer);
	/* do nothing; surface_attach (below) has this covered. */
}

static void
default_buffer_damage(struct wl_buffer *buffer,
                      struct wl_surface *surface,
                      int32_t x, int32_t y, int32_t width, int32_t height)
{
	ClaylandSurface *csurface =
		container_of(surface, ClaylandSurface, surface);
	ClaylandBuffer *cbuffer =
		container_of(buffer, ClaylandBuffer, buffer);

	/* damage event: TODO */
}

static void
client_destroy_buffer(struct wl_client *client, struct wl_buffer *buffer)
{
	wl_resource_destroy(&buffer->resource, client);
}

static const struct wl_buffer_interface default_buffer_interface = {
	client_destroy_buffer
};

CoglPixelFormat
_clayland_init_buffer(ClaylandBuffer *cbuffer,
                      ClaylandCompositor *compositor,
                      uint32_t id, int32_t width, int32_t height,
                      struct wl_visual *visual)
{
	g_return_val_if_fail(width >= 0 && height >= 0,
	                     COGL_PIXEL_FORMAT_ANY);

	cbuffer->buffer.compositor = &compositor->compositor;
	cbuffer->buffer.width = width;
	cbuffer->buffer.height = height;
	cbuffer->buffer.visual = visual;
	cbuffer->buffer.attach = default_buffer_attach;
	cbuffer->buffer.damage = default_buffer_damage;

	cbuffer->buffer.resource.object.id = id;
	cbuffer->buffer.resource.object.interface = &wl_buffer_interface;
	cbuffer->buffer.resource.object.implementation = (void (**)(void))
		&default_buffer_interface;

	if (visual == &compositor->compositor.premultiplied_argb_visual)
		return COGL_PIXEL_FORMAT_BGRA_8888_PRE;
	if (visual == &compositor->compositor.argb_visual)
		return COGL_PIXEL_FORMAT_BGRA_8888;
	if (visual == &compositor->compositor.rgb_visual)
		return COGL_PIXEL_FORMAT_BGR_888;

	/* unknown visual. */
	return COGL_PIXEL_FORMAT_ANY;
}

static void
surface_destroy(struct wl_client *client,
		struct wl_surface *surface)
{
	wl_resource_destroy(&surface->resource, client);
}

static void
surface_attach(struct wl_client *client,
	       struct wl_surface *surface, struct wl_buffer *buffer,
	       int32_t dx, int32_t dy)
{
	ClaylandSurface *csurface =
		container_of(surface, ClaylandSurface, surface);
	ClaylandBuffer *cbuffer =
		container_of(buffer, ClaylandBuffer, buffer);
	gfloat x, y;

	clutter_actor_get_position (CLUTTER_ACTOR (csurface), &x, &y);

	if (csurface->buffer != NULL)
		g_object_unref(csurface->buffer);
	csurface->buffer = g_object_ref(cbuffer);

	buffer->attach(buffer, surface); /* XXX: does nothing right now */
	clutter_texture_set_cogl_texture(&csurface->texture,
	                                 cbuffer->tex_handle);
	clutter_actor_set_position (CLUTTER_ACTOR(&csurface->texture),
				    x + dx, y + dy);
	clutter_actor_set_size (CLUTTER_ACTOR(&csurface->texture),
	                        buffer->width, buffer->height);
}

static void
surface_map_toplevel(struct wl_client *client,
	    struct wl_surface *surface)
{
	ClaylandSurface *csurface =
		container_of(surface, ClaylandSurface, surface);

	clutter_actor_show (CLUTTER_ACTOR(&csurface->texture));
	clutter_actor_set_reactive (CLUTTER_ACTOR (&csurface->texture), TRUE);
}

static void
surface_damage(struct wl_client *client,
	       struct wl_surface *surface,
	       int32_t x, int32_t y, int32_t width, int32_t height)
{
	ClaylandSurface *csurface =
		container_of(surface, ClaylandSurface, surface);
	ClaylandBuffer *cbuffer = csurface->buffer;

	if (cbuffer != NULL)
		cbuffer->buffer.damage(&cbuffer->buffer, surface,
		                       x, y, width, height);
	/* damage event: TODO */
}

const static struct wl_surface_interface surface_interface = {
	surface_destroy,
	surface_attach,
	surface_map_toplevel,
	surface_damage
};

static uint32_t
get_time(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void
destroy_surface(struct wl_resource *resource, struct wl_client *client)
{
	ClaylandSurface *surface =
		container_of(resource, ClaylandSurface, surface.resource);
	ClaylandCompositor *compositor = surface->compositor;
	struct wl_listener *l, *next;
	uint32_t time;

	time = get_time();
	wl_list_for_each_safe(l, next,
			      &surface->surface.destroy_listener_list, link)
		l->func(l, &surface->surface, time);

	g_object_unref(surface);
}

static void
compositor_create_surface(struct wl_client *client,
			  struct wl_compositor *compositor, uint32_t id)
{
	ClaylandCompositor *clayland =
		container_of(compositor, ClaylandCompositor, compositor);
	ClaylandSurface *surface;

	surface = g_object_new (clayland_surface_get_type(), NULL);

	surface->compositor = g_object_ref (clayland);
	clutter_container_add_actor(CLUTTER_CONTAINER (clayland->stage),
				    CLUTTER_ACTOR (surface));

	wl_list_init(&surface->surface.destroy_listener_list);
	surface->surface.resource.destroy = destroy_surface;

	surface->surface.resource.object.id = id;
	surface->surface.resource.object.interface = &wl_surface_interface;
	surface->surface.resource.object.implementation =
		(void (**)(void)) &surface_interface;
	surface->surface.client = client;

	/* Clutter actors inherit from GInitiallyUnowned, so the
	 * container now has the only reference.  We need to take one
	 * for the wayland resource as well. */
	g_object_ref (surface);
	wl_client_add_resource(client, &surface->surface.resource);
}

const static struct wl_compositor_interface compositor_interface = {
	compositor_create_surface,
};

static void
input_device_attach(struct wl_client *client,
		    struct wl_input_device *device_base,
		    uint32_t time,
		    struct wl_buffer *buffer, int32_t x, int32_t y)
{
	ClaylandInputDevice *device =
		container_of(device_base, ClaylandInputDevice, input_device);

	if (time < device->input_device.pointer_focus_time)
		return;
	if (device->input_device.pointer_focus == NULL)
		return;

	if (device->input_device.pointer_focus->client != client)
		return;

	/* FIXME: Actually update pointer image */
}

const static struct wl_input_device_interface input_device_interface = {
	input_device_attach,
};

static void
add_devices(ClaylandCompositor *compositor)
{
	ClutterDeviceManager *device_manager;
	ClaylandInputDevice *clayland_device;
	ClutterInputDevice *device;
	GSList *list, *l;

	clayland_device = g_new0 (ClaylandInputDevice, 1);

	wl_input_device_init(&clayland_device->input_device,
			     &compositor->compositor);

	clayland_device->input_device.object.interface =
		&wl_input_device_interface;
	clayland_device->input_device.object.implementation =
		(void (**)(void)) &input_device_interface;
	wl_display_add_object(compositor->display,
			      &clayland_device->input_device.object);
	wl_display_add_global(compositor->display,
			      &clayland_device->input_device.object,
			      NULL);

	clayland_device->input_device.motion_grab.interface =
		&motion_grab_interface;

	device_manager = clutter_device_manager_get_default ();
	list = clutter_device_manager_list_devices (device_manager);

	for (l = list; l; l = l->next) {
		g_debug("device %p", l->data);
		device = CLUTTER_INPUT_DEVICE (l->data);

		g_object_set_data (G_OBJECT (device),
				   "clayland", clayland_device);
	}
}

static void
post_drm_device(struct wl_client *client, struct wl_object *global)
{
	ClaylandCompositor *compositor =
	    container_of(global, ClaylandCompositor, drm_object);

	wl_client_post_event(client, global, WL_DRM_DEVICE,
	                     compositor->drm_path);
}

static void add_buffer_interfaces(ClaylandCompositor *compositor)
{
	const char *extensions;

	compositor->shm_object.interface = &wl_shm_interface;
	compositor->shm_object.implementation =
	    (void (**)(void)) &clayland_shm_interface;
	wl_display_add_object(compositor->display, &compositor->shm_object);
	wl_display_add_global(compositor->display, &compositor->shm_object, NULL);

	if (!compositor->drm_path) {
		g_warning("DRI2 connect failed, disabling DRM buffers");
		return;
	}

	extensions = eglQueryString(compositor->egl_display, EGL_EXTENSIONS);

	if(!extensions || !strstr(extensions, "EGL_MESA_drm_image"))
		return;

	compositor->create_image =
	    (PFNEGLCREATEIMAGEKHRPROC)
	        eglGetProcAddress("eglCreateImageKHR");
	compositor->destroy_image =
	    (PFNEGLDESTROYIMAGEKHRPROC)
	        eglGetProcAddress("eglDestroyImageKHR");
	compositor->image2tex =
	    (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)
	        eglGetProcAddress("glEGLImageTargetTexture2DOES");

	if (!(compositor->create_image) || !(compositor->destroy_image)
	    || !(compositor->image2tex))
		return;

	compositor->drm_object.interface = &wl_drm_interface;
	compositor->drm_object.implementation =
	    (void (**)(void)) &clayland_drm_interface;
	wl_display_add_object(compositor->display, &compositor->drm_object);
	wl_display_add_global(compositor->display, &compositor->drm_object,
	                      post_drm_device);
}

ClaylandCompositor *
clayland_compositor_create(ClutterActor *stage)
{
	ClaylandCompositor *compositor;

	g_return_val_if_fail(CLUTTER_IS_STAGE(stage), NULL);

	compositor = g_object_new (clayland_compositor_get_type(), NULL);

	if (!clutter_stage_is_default(CLUTTER_STAGE(stage)))
		compositor->stage = g_object_ref_sink(stage);
	else
		compositor->stage = stage;

	compositor->display = wl_display_create();
	if (compositor->display == NULL) {
		g_warning("failed to create display: %m");
		g_object_unref(compositor);
		return NULL;
	}

	compositor->loop = wl_display_get_event_loop(compositor->display);
	compositor->source = wl_glib_source_new(compositor->loop);
	g_source_attach(compositor->source, NULL);

	if (wl_display_add_socket(compositor->display, NULL)) {
		g_warning("failed to add socket: %m");
		wl_display_destroy (compositor->display);
		g_object_unref(compositor);
		return NULL;
	}

	if (wl_compositor_init(&compositor->compositor,
			       &compositor_interface,
			       compositor->display) < 0) {
		wl_display_destroy (compositor->display);
		g_object_unref(compositor);
		return NULL;
	}

	/* Can we figure out whether we're compiling against clutter
	 * x11 or not? */
	if (dri2_connect(compositor) < 0)
		g_warning("failed to connect to DRI2");

	compositor->egl_display = clutter_egl_display ();
	g_debug("egl display %p", compositor->egl_display);

	add_devices(compositor);
	add_buffer_interfaces(compositor);

	compositor->shell.object.interface = &wl_shell_interface;
	compositor->shell.object.implementation =
		(void (**)(void)) &shell_interface;
	wl_display_add_object(compositor->display, &compositor->shell.object);
	if (wl_display_add_global(compositor->display,
				  &compositor->shell.object, NULL)) {
		wl_display_destroy (compositor->display);
		g_object_unref(compositor);
		return NULL;
	}

	wl_event_loop_add_signal(compositor->loop,
				 SIGTERM, on_term_signal, compositor);
	wl_event_loop_add_signal(compositor->loop,
				 SIGINT, on_term_signal, compositor);

	return compositor;
}

int
main (int argc, char *argv[])
{
	ClutterActor *stage;
	ClutterColor  stage_color = { 0x61, 0x64, 0x8c, 0xff };
	ClaylandCompositor *compositor;
	GError       *error;

	error = NULL;

	clutter_init_with_args (&argc, &argv, NULL, NULL, NULL, &error);
	if (error) {
		g_warning ("Unable to initialise Clutter:\n%s",
			   error->message);
		g_error_free (error);

		return EXIT_FAILURE;
	}

	stage = clutter_stage_get_default ();
	clutter_actor_set_size (stage, 800, 600);
	clutter_actor_set_name (stage, "Clayland");

	clutter_stage_set_title (CLUTTER_STAGE (stage), "Clayland");
	clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
	clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);

	compositor = clayland_compositor_create(stage);
	if (!compositor)
		return EXIT_FAILURE;

	compositor->hand = clutter_texture_new_from_file ("redhand.png", &error);
	if (compositor->hand == NULL)
		g_error ("image load failed: %s", error->message);

	compositor->stage_width = clutter_actor_get_width (stage);
	compositor->stage_height = clutter_actor_get_height (stage);

	clutter_actor_set_reactive (compositor->hand, TRUE);
	clutter_actor_set_size (compositor->hand, 200, 213);
	clutter_actor_set_position (compositor->hand, 200, 200);
	clutter_actor_move_anchor_point_from_gravity (compositor->hand,
						      CLUTTER_GRAVITY_CENTER);

	/* Add to our group group */
	clutter_container_add_actor (CLUTTER_CONTAINER (stage), compositor->hand);
	/* Show everying */
	clutter_actor_show (stage);

	g_signal_connect (stage, "captured-event",
			  G_CALLBACK (event_cb),
			  compositor);

	clutter_main ();

	wl_display_destroy (compositor->display);
	g_object_unref (compositor);

	return EXIT_SUCCESS;
}
