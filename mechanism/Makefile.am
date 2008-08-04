# Dbus service file; copied from Epiphany's Makefile.am
servicedir = $(datadir)/dbus-1/services
service_in_files = org.gnome.Gksu.service.in
service_DATA = $(service_in_files:.service.in=.service)

# Rule to make the service file with bindir expanded
$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@bindir\@|$(bindir)|" $< > $@

AM_CFLAGS = -Wall
INCLUDES = @GKSUPKMECH_CFLAGS@

VALAC = valac
VALA_SRCS = \
	gksu-server.vala

VALA_PKGS = 

VALA_CFILES = $(VALA_SRCS:.vala=.c)
VALA_HFILES = $(VALA_SRCS:.vala=.h)

SUFFIXES = .vala .c .h

vala-build.stamp: $(VALA_SRCS)
	$(VALAC) -C -X "$(INCLUDES)" $(VALA_PKGS) $^ --basedir ./
	date +'%s' > $@

bin_PROGRAMS = gksu-server

gksu_server_LDFLAGS = @GKSUPKMECH_LIBS@
gksu_server_SOURCES = $(VALA_CFILES)

BUILT_SOURCES = \
	vala-build.stamp

vala-clean:
	rm -f vala-build.stamp $(VALA_CFILES) $(VALA_HFILES)

EXTRA_DIST = \
	$(service_in_files)             \
	$(service_DATA)

DISTCLEANFILES = \
	$(service_DATA)

