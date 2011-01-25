lib_LTLIBRARIES = libclayland.la
noinst_PROGRAMS = clayland

include_HEADERS =				\
	clayland.h

libclayland_la_CFLAGS =				\
	$(CLAYLAND_CFLAGS)			\
	$(GCC_CFLAGS)

libclayland_la_LIBADD = $(CLAYLAND_LIBS)

libclayland_la_SOURCES =			\
	clayland-private.h			\
	clayland.c				\
	clayland-buffer.c			\
	clayland-drm.c				\
	clayland-shm.c				\
	clayland-surface.c			\
	input.c					\
	shell.c					\
	wayland-source.c			\
	dri2.c


clayland_CFLAGS = $(EXAMPLE_CFLAGS)

clayland_LDADD =				\
	$(top_builddir)/libclayland.la		\
	$(EXAMPLE_LIBS)

clayland_SOURCES = example.c

ACLOCAL_AMFLAGS = -I m4
