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

#include <stdlib.h>
#include <xcb/xcb.h>
#include <xcb/dri2.h>
#include <clutter/x11/clutter-x11.h>
#include <X11/Xlib-xcb.h>
#include <fcntl.h>

#include "clayland-drm.h"
#include "clayland-config.h"

int
dri2_x11_connect(ClaylandDRMCompositor *drm)
{
	Display *dpy;
	xcb_connection_t *conn;
	Window root;
	xcb_dri2_query_version_reply_t *dri2_query;
	xcb_dri2_query_version_cookie_t dri2_query_cookie;
	xcb_dri2_connect_reply_t *connect;
	xcb_dri2_connect_cookie_t connect_cookie;
	xcb_generic_error_t *error;
	char path[512];
	int fd;

	dpy = clutter_x11_get_default_display ();
	conn = XGetXCBConnection(dpy);
	root = clutter_x11_get_root_window ();

	xcb_prefetch_extension_data (conn, &xcb_dri2_id);

	dri2_query_cookie =
		xcb_dri2_query_version (conn,
					XCB_DRI2_MAJOR_VERSION,
					XCB_DRI2_MINOR_VERSION);

	connect_cookie = xcb_dri2_connect_unchecked (conn,
						     root,
						     XCB_DRI2_DRIVER_TYPE_DRI);

	dri2_query =
		xcb_dri2_query_version_reply (conn,
					      dri2_query_cookie, &error);
	if (dri2_query == NULL || error != NULL) {
		fprintf(stderr, "DRI2: failed to query version\n");
		free(dri2_query);
		free(error);
		return -1;
	}

	fprintf(stderr, "DRI2: %u.%u\n",
		dri2_query->major_version, dri2_query->minor_version);

	connect = xcb_dri2_connect_reply (conn,
					  connect_cookie, NULL);
	if (connect == NULL ||
	    connect->driver_name_length + connect->device_name_length == 0) {
		fprintf(stderr, "DRI2: failed to connect, DRI2 version: %u.%u\n",
			dri2_query->major_version, dri2_query->minor_version);
		free(dri2_query);
		return -1;
	}
	free(dri2_query);

#ifdef XCB_DRI2_CONNECT_DEVICE_NAME_BROKEN
	{
		char *driver_name, *device_name;

		driver_name = xcb_dri2_connect_driver_name (connect);
		device_name = driver_name +
			((connect->driver_name_length + 3) & ~3);
		snprintf(path, sizeof path, "%.*s",
			 xcb_dri2_connect_device_name_length (connect),
			 device_name);
	}
#else
	snprintf(path, sizeof path, "%.*s",
		 xcb_dri2_connect_device_name_length (connect),
		 xcb_dri2_connect_device_name (connect));
#endif
	free(connect);

	fd = open(path, O_RDWR);
	if (fd < 0) {
		fprintf(stderr,
			"DRI2: could not open %s (%m)\n", path);
		return -1;
	}

	drm->drm_path = g_strdup(path);
	if (drm->drm_path == NULL) {
		(void) close(fd);
		return -1;
	}

	drm->drm_fd = fd;
	drm->xconn = conn;
	drm->root_xwindow = root;

	return 0;
}

int
dri2_x11_authenticate(ClaylandDRMCompositor *drm, uint32_t magic)
{
	xcb_connection_t *conn;
	xcb_dri2_authenticate_reply_t *authenticate;
	xcb_dri2_authenticate_cookie_t authenticate_cookie;
	Window root;

	conn = drm->xconn;
	root = drm->root_xwindow;

	authenticate_cookie =
		xcb_dri2_authenticate_unchecked(conn, root, magic);
	authenticate =
		xcb_dri2_authenticate_reply(conn,
					    authenticate_cookie, NULL);
	if (authenticate == NULL || !authenticate->authenticated) {
		fprintf(stderr, "DRI2: failed to authenticate\n");
		free(authenticate);
		return -1;
	}

	free(authenticate);

	return 0;
}
