#include <stdlib.h>
#include <xcb/xcb.h>
#include <xcb/dri2.h>
#include <clutter/x11/clutter-x11.h>
#include <X11/Xlib-xcb.h>

int
dri2_connect(void)
{
	Display *dpy;
	xcb_connection_t *conn;
	xcb_dri2_query_version_reply_t *dri2_query;
	xcb_dri2_query_version_cookie_t dri2_query_cookie;
	xcb_generic_error_t *error;

	dpy = clutter_x11_get_default_display ();
	conn = XGetXCBConnection(dpy);

	xcb_prefetch_extension_data (conn, &xcb_dri2_id);

	dri2_query_cookie =
		xcb_dri2_query_version (conn,
					XCB_DRI2_MAJOR_VERSION,
					XCB_DRI2_MINOR_VERSION);

	dri2_query =
		xcb_dri2_query_version_reply (conn,
					      dri2_query_cookie, &error);
	if (dri2_query == NULL || error != NULL) {
		fprintf(stderr, "DRI2: failed to query version\n");
		free(error);
		return -1;
	}

	fprintf(stderr, "DRI2: %d.%d\n",
		dri2_query->major_version, dri2_query->minor_version);

	free(dri2_query);

	return 0;
}

int
dri2_authenticate(uint32_t magic)
{
	Display *dpy;
	xcb_connection_t *conn;
	xcb_dri2_authenticate_reply_t *authenticate;
	xcb_dri2_authenticate_cookie_t authenticate_cookie;
	Window root;

	dpy = clutter_x11_get_default_display ();
	conn = XGetXCBConnection (dpy);
	root = clutter_x11_get_root_window ();

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
