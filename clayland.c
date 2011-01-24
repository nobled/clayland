#include <string.h>
#include <wayland-server.h>
#include <clutter/clutter.h>
#include <clutter/egl/clutter-egl.h>

#include <sys/time.h>
#include <math.h>
#include <errno.h>
#include <glib.h>

#include "clayland-private.h"

G_DEFINE_TYPE (ClaylandCompositor, clayland_compositor, G_TYPE_OBJECT);

static void
clayland_compositor_finalize (GObject *object)
{
	ClaylandCompositor *compositor = CLAYLAND_COMPOSITOR(object);

	g_debug("finalizing compositor %p of type '%s'", object,
	        G_OBJECT_TYPE_NAME(object));

	if (compositor->source != NULL) {
		g_source_destroy(compositor->source);
		g_source_unref(compositor->source);
	}
	if (g_signal_handler_is_connected(compositor->stage,
		            compositor->event_handler_id))
		g_signal_handler_disconnect(compositor->stage,
		                compositor->event_handler_id);
	if (!clutter_stage_is_default(CLUTTER_STAGE(compositor->stage)))
		g_object_unref(compositor->stage);
	if (compositor->display != NULL)
		wl_display_destroy (compositor->display);
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
	g_debug("initializing compositor %p of type '%s'", compositor,
	        G_OBJECT_TYPE_NAME(compositor));

	compositor->display = NULL;
	compositor->source = NULL;
	compositor->event_handler_id = 0;
	compositor->stage = NULL;
	compositor->drm_path = NULL;
	compositor->drm_fd = -1;
}


typedef struct ClaylandInputDevice {
	struct wl_input_device input_device;
	ClutterInputDevice *clutter_device;
} ClaylandInputDevice;

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
	ClutterActor *stage = surface->compositor->stage;
	struct wl_listener *l, *next;
	uint32_t time;

	g_debug("destroying surface %p of type '%s'", surface,
	        G_OBJECT_TYPE_NAME(surface));

	time = get_time();
	wl_list_for_each_safe(l, next,
			      &surface->surface.destroy_listener_list, link)
		l->func(l, &surface->surface, time);

	clutter_container_remove_actor (CLUTTER_CONTAINER (stage),
					CLUTTER_ACTOR (surface));
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
		(void (**)(void)) &clayland_surface_interface;
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

static gboolean
has_egl_extension(EGLDisplay dpy, size_t target_len, const char *target)
{
	const char *extensions, *ext, *next;
	size_t len;

	extensions = eglQueryString(dpy, EGL_EXTENSIONS);
	ext = extensions;
	while (ext) {
		next = strchr(ext, ' ');
		if (next) {
			len = (size_t)(next - ext);
			next++; /* point to the next extension string */
			if (len != target_len)
				goto no_match;
		} else {
		/* if 'ext' is the last extension string,
		   include the target's null terminator in the comparison */
			len = target_len+1;
		}

		if (strncmp(target, ext, len) == 0)
			break;
no_match:
		ext = next;
	};

	return ext != NULL;
}

static void
post_drm_device(struct wl_client *client, struct wl_object *global)
{
	ClaylandCompositor *compositor =
	    container_of(global, ClaylandCompositor, drm_object);

	g_debug("Advertising DRM device: %s", compositor->drm_path);

	wl_client_post_event(client, global, WL_DRM_DEVICE,
	                     compositor->drm_path);
}

static void add_buffer_interfaces(ClaylandCompositor *compositor)
{
	compositor->shm_object.interface = &wl_shm_interface;
	compositor->shm_object.implementation =
	    (void (**)(void)) &clayland_shm_interface;
	wl_display_add_object(compositor->display, &compositor->shm_object);
	wl_display_add_global(compositor->display, &compositor->shm_object, NULL);

	if (!compositor->drm_path) {
		g_warning("DRI2 connect failed, disabling DRM buffers");
		return;
	}

	if (!has_egl_extension(compositor->egl_display,
	                       18, "EGL_MESA_drm_image"))
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
		g_object_unref(compositor);
		return NULL;
	}

	if (wl_compositor_init(&compositor->compositor,
			       &compositor_interface,
			       compositor->display) < 0) {
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
		(void (**)(void)) &clayland_shell_interface;
	wl_display_add_object(compositor->display, &compositor->shell.object);
	if (wl_display_add_global(compositor->display,
				  &compositor->shell.object, NULL)) {
		g_object_unref(compositor);
		return NULL;
	}

	compositor->event_handler_id =
	    g_signal_connect_object (stage, "captured-event",
			  G_CALLBACK (event_cb),
			  compositor, 0);

	wl_event_loop_add_signal(compositor->loop,
				 SIGTERM, on_term_signal, compositor);
	wl_event_loop_add_signal(compositor->loop,
				 SIGINT, on_term_signal, compositor);

	return compositor;
}

