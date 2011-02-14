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

		g_object_set_data (G_OBJECT (device), "clayland",
				   &clayland_device->input_device);
	}
}

