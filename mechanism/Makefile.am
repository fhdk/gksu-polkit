# Dbus service file; copied from Epiphany's Makefile.am
servicedir = $(datadir)/dbus-1/system-services
service_in_files = org.gnome.Gksu.service.in
service_DATA = $(service_in_files:.service.in=.service)

# Rule to make the service file with bindir expanded
$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@sbindir\@|$(sbindir)|" $< > $@

dbusconfdir = $(DBUS_SYS_DIR)
dbusconf_DATA = gksu-polkit.conf

AM_CFLAGS = -Wall
INCLUDES = $(GKSUPKCOMMON_CFLAGS) $(GKSUPKMECH_CFLAGS) -I../common/

gksu-server-service-glue.h: dbus-gksu-server.xml
	$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=gksu_server $^

sbin_PROGRAMS = gksu-server

gksu_server_LDFLAGS = $(GKSUPKCOMMON_LIBS) $(GKSUPKMECH_LIBS)
gksu_server_LDADD = ../common/libgksu-polkit-common.la
gksu_server_SOURCES = \
	main.c \
	gksu-server.c \
	gksu-server.h \
	gksu-controller.c \
	gksu-controller.h \
	gksu-error.h \
	gksu-server-service-glue.h

BUILT_SOURCES = \
	gksu-server-service-glue.h

EXTRA_DIST = \
	$(service_in_files)             \
	$(service_DATA)

DISTCLEANFILES = \
	$(service_DATA)

CLEANFILES = \
	gksu-server-service-glue.h
