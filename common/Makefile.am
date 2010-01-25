AM_CFLAGS = -g -O2 -Wall
INCLUDES = ${GKSUPKCOMMON_CFLAGS}

VALAC = valac
VALA_SRCS = gksu-environment.vala

VALA_PKGS = --pkg glib-2.0 --pkg gee-1.0

VALA_CFILES = $(VALA_SRCS:.vala=.c)
VALA_HFILES = $(VALA_SRCS:.vala=.h)

SUFFIXES = .vala .c .h

vala-build.stamp: $(VALA_SRCS)
	$(VALAC) -C -X "$(INCLUDES)" $(VALA_PKGS) $^ --basedir ./
	date +'%s' > $@

gksu-marshal.h: gksu-marshal.list
	($(GLIB_GENMARSHAL) --prefix=gksu_marshal gksu-marshal.list --header) > xgen-gmh \
	&& (cmp -s xgen-gmh gksu-marshal.h || cp xgen-gmh gksu-marshal.h) \
	&& rm -f xgen-gmh xgen-gmh~

gksu-marshal.c: gksu-marshal.list gksu-marshal.h
	($(GLIB_GENMARSHAL) --prefix=gksu_marshal gksu-marshal.list --body) > xgen-gmc \
	&& cp xgen-gmc gksu-marshal.c \
	&& rm -f xgen-gmc xgen-gmc~

noinst_LTLIBRARIES = libgksu-polkit-common.la
libgksu_polkit_common_la_SOURCES = \
	$(VALA_CFILES) \
	gksu-write-queue.c \
	gksu-write-queue.h \
	gksu-marshal.c \
	gksu-marshal.h

libgksu_polkit_common_la_LDFLAGS = ${GKSUPKLIB_CFLAGS}

BUILT_SOURCES = \
	vala-build.stamp \
	gksu-environment.c \
	gksu-environment.h \
	gksu-marshal.c \
	gksu-marshal.h

EXTRA_DIST = gksu-marshal.list

CLEANFILES = \
	${BUILT_SOURCES}