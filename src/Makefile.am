lib_LTLIBRARIES = libclayland.la

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
	clayland-shm-buffer.c			\
	clayland-surface.c			\
	input.c					\
	output.c				\
	shell.c					\
	wayland-source.c			\
	clayland-drm.h				\
	clayland-drm-compositor.c		\
	clayland-drm-buffer.c			\
	$(dri2_sources)

if HAVE_X11
dri2_sources = dri2-x11.c
endif

