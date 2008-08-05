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

noinst_LTLIBRARIES = libgksu-polkit-common.la
libgksu_polkit_common_la_SOURCES = $(VALA_CFILES)
libgksu_polkit_common_la_LDFLAGS = ${GKSUPKLIB_CFLAGS}

BUILT_SOURCES = \
	vala-build.stamp

vala-clean:
	rm -f vala-build.stamp $(VALA_CFILES) $(VALA_HFILES)
