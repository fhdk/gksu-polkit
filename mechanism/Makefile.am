# Dbus service file; copied from Epiphany's Makefile.am
servicedir = $(datadir)/dbus-1/services
service_in_files = org.gnome.Gksu.service.in
service_DATA = $(service_in_files:.service.in=.service)

# Rule to make the service file with bindir expanded
$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@bindir\@|$(bindir)|" $< > $@

dbusconfdir = $(DBUS_SYS_DIR)
dbusconf_DATA = gksu-polkit.conf

AM_CFLAGS = -Wall
INCLUDES = @GKSUPKMECH_CFLAGS@

VALAC = valac
VALA_SRCS = gksu-process.vala

VALA_PKGS = --pkg dbus-glib-1

VALA_CFILES = $(VALA_SRCS:.vala=.c)
VALA_HFILES = $(VALA_SRCS:.vala=.h)

SUFFIXES = .vala .c .h

vala-build.stamp: $(VALA_SRCS)
	$(VALAC) -C -X "$(INCLUDES)" $(VALA_PKGS) $^ --basedir ./
	date +'%s' > $@

gksu-server-service-glue.h: dbus-gksu-server.xml
	$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=gksu_server $^

bin_PROGRAMS = gksu-server

gksu_server_LDFLAGS = @GKSUPKMECH_LIBS@
gksu_server_SOURCES = \
	main.c \
	gksu-server.c \
	gksu-server.h \
	gksu-server-service-glue.h \
	$(VALA_CFILES)

BUILT_SOURCES = \
	gksu-server-service-glue.h \
	vala-build.stamp

vala-clean:
	rm -f vala-build.stamp $(VALA_CFILES) $(VALA_HFILES)

EXTRA_DIST = \
	$(service_in_files)             \
	$(service_DATA)

DISTCLEANFILES = \
	$(service_DATA)

CLEANFILES = \
	gksu-server-service-glue.h
