AM_CFLAGS = -Wall
INCLUDES = @GKSUPK_CFLAGS@

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

gksu_server_LDFLAGS = @GKSUPK_LIBS@
gksu_server_SOURCES = $(VALA_CFILES)

BUILT_SOURCES = \
	vala-build.stamp

vala-clean:
	rm -f vala-build.stamp $(VALA_CFILES) $(VALA_HFILES)
