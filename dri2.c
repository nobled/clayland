#include <stdlib.h>
#include <xcb/xcb.h>
#include <xcb/dri2.h>
#include <clutter/x11/clutter-x11.h>
#include <X11/Xlib-xcb.h>
#include <fcntl.h>

#include "clayland.h"
#include "clayland-config.h"

int
dri2_connect(ClaylandCompositor *compositor)
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

	compositor->drm_path = g_strdup(path);
	if (compositor->drm_path == NULL) {
		(void) close(fd);
		return -1;
	}

	compositor->drm_fd = fd;
	compositor->xconn = conn;
	compositor->root_window = root;

	return 0;
}

int
dri2_authenticate(ClaylandCompositor *compositor, uint32_t magic)
{
	xcb_connection_t *conn;
	xcb_dri2_authenticate_reply_t *authenticate;
	xcb_dri2_authenticate_cookie_t authenticate_cookie;
	Window root;

	conn = compositor->xconn;
	root = compositor->root_window;

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
