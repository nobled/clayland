noinst_PROGRAMS = clayland

AM_CPPFLAGS = $(CLAYLAND_CFLAGS)

clayland_LDADD =				\
	$(CLAYLAND_LIBS)

clayland_SOURCES =				\
	clayland.h				\
	clayland-private.h			\
	clayland.c				\
	clayland-buffer.c			\
	clayland-drm.c				\
	clayland-shm.c				\
	clayland-surface.c			\
	input.c					\
	shell.c					\
	example.c				\
	wayland-source.c			\
	dri2.c

ACLOCAL_AMFLAGS = -I m4
