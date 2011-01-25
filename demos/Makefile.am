noinst_PROGRAMS = clayland

AM_CPPFLAGS =					\
	-I$(top_srcdir)/src

clayland_CFLAGS = $(EXAMPLE_CFLAGS)

clayland_LDADD =				\
	$(top_builddir)/src/libclayland.la	\
	$(EXAMPLE_LIBS)

clayland_SOURCES = example.c
