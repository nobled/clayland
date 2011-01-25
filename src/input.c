
#include "clayland-private.h"

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
	else
		device = NULL;

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
		g_assert(device != NULL);

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
		g_assert(device != NULL);

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
		g_assert(device != NULL);

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

void
_clayland_add_devices(ClaylandCompositor *compositor)
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

	compositor->event_handler_id =
	    g_signal_connect_object (compositor->container, "captured-event",
			  G_CALLBACK (event_cb),
			  compositor, 0);
}

