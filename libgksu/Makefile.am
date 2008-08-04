AM_CFLAGS = -g -O2 -Wall
INCLUDES = ${GKSUPKLIB_CFLAGS}

lib_LTLIBRARIES = libgksu-polkit.la
libgksu_polkit_la_SOURCES = gksu-process.c gksu-process.h

# 0.0.0 -> major.minor.micro
# major -> breaks backward compatibility (changes to existing ABI)
# minor -> keeps compatibility (additions to the API)
# micro -> no change to the API/ABI
libgksu_polkit_la_LDFLAGS = -version-info 0:0:0 -Wl,-O1 ${GKSUPKLIB_LIBS}
if USE_VERSION_SCRIPT
libgksu_polkit_la_LDFLAGS += -Wl,--version-script=libgksu.ver
endif

include_HEADERS = gksu-process.h
includedir = ${prefix}/include/${PACKAGE}

pkgconfigdir = ${libdir}/pkgconfig
pkgconfig_DATA = libgksu-polkit-1.pc

EXTRA_DIST = libgksu.ver
