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

#include "clayland-private.h"


G_DEFINE_TYPE (ClaylandOutput, clayland_output, G_TYPE_OBJECT);

static void
clayland_output_finalize (GObject *object)
{
	ClaylandOutput *output = CLAYLAND_OUTPUT(object);

	g_debug("finalizing output %p of type '%s'", object,
	        G_OBJECT_TYPE_NAME(object));

	if (g_signal_handler_is_connected(output->container,
		            output->event_handler_id))
		g_signal_handler_disconnect(output->container,
		                output->event_handler_id);
	if (g_signal_handler_is_connected(output->container,
		            output->delete_handler_id))
		g_signal_handler_disconnect(output->container,
		                output->delete_handler_id);
	if (!clutter_stage_is_default(CLUTTER_STAGE(output->container)))
		g_object_unref(output->container);

	G_OBJECT_CLASS (clayland_output_parent_class)->finalize (object);
}

static void
clayland_output_class_init (ClaylandOutputClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = clayland_output_finalize;
}

static void
clayland_output_init (ClaylandOutput *output)
{
	output->container = NULL;
	output->event_handler_id = 0;
	output->delete_handler_id = 0;
}

static gboolean
delete_cb(ClutterStage *container, ClutterEvent *event, gpointer data)
{
	ClaylandCompositor *compositor = CLAYLAND_COMPOSITOR (data);

	if (compositor->output->container != CLUTTER_ACTOR(container))
		return FALSE;

	if (!clutter_stage_is_default(CLUTTER_STAGE(container)))
		clutter_actor_destroy(CLUTTER_ACTOR(container));

	g_object_unref(compositor->output);
	compositor->output = NULL;
	/* TODO: once there's multiple-output support, only quit
	   when the number of outputs reaches zero */
	clutter_main_quit();
	return TRUE;
}

static gboolean
event_cb (ClutterActor *container, ClutterEvent *event, gpointer data)
{
	const struct wl_grab_interface *interface;
	struct wl_input_device *device;
	ClutterInputDevice *clutter_device;
	ClaylandSurface *cs;
	gfloat sx, sy;
	uint32_t state, button, key;

	clutter_device = clutter_event_get_device (event);
	if (clutter_device) {
		device = g_object_get_data (G_OBJECT(clutter_device),
					   "clayland");
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
post_geometry(struct wl_client *client, struct wl_object *global)
{
	ClaylandOutput *output =
		container_of(global, ClaylandOutput, output);
	gfloat width, height;

	clutter_actor_get_size(output->container, &width, &height);

	/* XXX: Should we even try to use the x and y parameters here? */
	/* XXX: The container should be able to change size, we should send
	        an event like this any time it does, not just at connect */
	wl_client_post_event(client, global, WL_OUTPUT_GEOMETRY,
	                     0, 0, (int)width, (int)height);
}

void
clayland_compositor_add_output(ClaylandCompositor *compositor,
                               ClutterContainer *container)
{
	ClaylandOutput *output;
	GConnectFlags flags = 0;

	g_return_if_fail(CLAYLAND_IS_COMPOSITOR(compositor) &&
	                 CLUTTER_IS_CONTAINER(container) &&
	                 CLUTTER_IS_ACTOR(container));

	/* TODO: allow multiple outputs. */
	if (compositor->output != NULL)
		return;

	output = g_object_new(clayland_output_get_type(), NULL);

	g_debug("creating output %p of type '%s'", output,
	        G_OBJECT_TYPE_NAME(output));

	if (!clutter_stage_is_default(CLUTTER_STAGE(container)))
		output->container = g_object_ref_sink(container);
	else
		output->container = CLUTTER_ACTOR(container);

	output->event_handler_id =
	    g_signal_connect_object (container, "captured-event",
			  G_CALLBACK (event_cb),
			  NULL, flags);
	if (CLUTTER_IS_STAGE(container))
		output->delete_handler_id =
		    g_signal_connect_object (container, "delete-event",
				  G_CALLBACK (delete_cb),
				  compositor, flags);

	output->output.interface = &wl_output_interface;

	wl_display_add_object(compositor->display, &output->output);
	wl_display_add_global(compositor->display,
	                      &output->output, post_geometry);

	compositor->output = output;
}

