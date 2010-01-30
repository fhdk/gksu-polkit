AC_INIT(gksu-polkit, 0.0.1, gns@gnome.org)
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AC_CONFIG_SRCDIR(libgksu/gksu-process.c)
AC_CONFIG_MACRO_DIR(m4)

AM_MAINTAINER_MODE

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

IT_PROG_INTLTOOL

# i18n
GETTEXT_PACKAGE=gksu-polkit
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[Gettext package])
AM_GLIB_GNU_GETTEXT

# check for gtk-doc
GTK_DOC_CHECK(1.9)

lt_simple_link_test_code='int main(){return(0);}'
AC_LIBTOOL_LINKER_OPTION([whether the linker accepts the --version-script flag],
                   use_version_script, [-Wl,--version-script=libgksu/libgksu.ver])
AM_CONDITIONAL(USE_VERSION_SCRIPT, test x$use_version_script = xyes)

AC_PATH_PROG(DBUSBINDINGTOOL, dbus-binding-tool)
AC_SUBST(DBUSBINDINGTOOL)

GLIB_GENMARSHAL=`pkg-config glib-2.0 --variable=glib_genmarshal`
AC_SUBST(GLIB_GENMARSHAL)

dnl ---------------------------------------------------------------------------
dnl - Are we specifying a different dbus root ?
dnl ---------------------------------------------------------------------------

AC_ARG_WITH(dbus-sys,
            [AC_HELP_STRING([--with-dbus-sys=<dir>],
            [where D-BUS system.d directory is])])

AC_ARG_WITH(dbus-services,
            [AC_HELP_STRING([--with-dbus-services=<dir>],
            [where D-BUS services directory is])])

if ! test -z "$with_dbus_sys" ; then
        DBUS_SYS_DIR="$with_dbus_sys"
else
        DBUS_SYS_DIR="/etc/dbus-1/system.d"
fi
AC_SUBST(DBUS_SYS_DIR)

PKG_CHECK_MODULES(GKSUPKMECH, [gobject-2.0 >= 2.0.0, dbus-glib-1, polkit-gobject-1])

PKG_CHECK_MODULES(GKSUPKLIB, [gobject-2.0 >= 2.0.0, dbus-glib-1, gdk-x11-2.0, libstartup-notification-1.0])

PKG_CHECK_MODULES(GKSUPKCOMMON, [gobject-2.0 >= 2.0.0, glib-2.0, gee-1.0 >= 0.5])

PKG_CHECK_MODULES(GKSUPK, [glib-2.0 >= 2.14 gtk+-2.0, gobject-2.0 >= 2.0.0])

AC_CONFIG_FILES([
        Makefile
        common/Makefile
        data/Makefile
        mechanism/Makefile
        libgksu/Makefile
        libgksu/libgksu-polkit-1.pc
        gksu/Makefile
        po/Makefile.in
        doc/Makefile
        doc/reference/Makefile
        ])
AC_OUTPUT
