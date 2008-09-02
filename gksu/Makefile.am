AM_CFLAGS = -g -O2 -Wall
INCLUDES = @GKSUPK_CFLAGS@ -I../libgksu/ -I../common/

bin_PROGRAMS = gksu-polkit

gksu_polkit_LDFLAGS = @GKSUPK_LIBS@
gksu_polkit_LDADD = ../libgksu/libgksu-polkit.la ../common/libgksu-polkit-common.la
gksu_polkit_SOURCES = gksu-polkit.c
