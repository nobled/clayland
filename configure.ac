AC_INIT(clayland, 0.1)
AM_INIT_AUTOMAKE([foreign dist-bzip2])
AC_PROG_CC
AC_PROG_LIBTOOL
AC_CONFIG_MACRO_DIR([m4])
AM_SILENT_RULES([yes])

PKG_PROG_PKG_CONFIG()

PKG_CHECK_MODULES(CLAYLAND, [wayland-server clutter-egl-1.0 libdrm >= 2.4.17 x11-xcb xcb-dri2])

if test "x$GCC" = "xyes"; then
	GCC_CFLAGS="-Wall -g -Wstrict-prototypes -Wmissing-prototypes -fvisibility=hidden"
fi
AC_SUBST(GCC_CFLAGS)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
