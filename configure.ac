AC_INIT(gksu-polkit, 0.0.1, gns@gnome.org)
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AC_CONFIG_SRCDIR(libgksu/gksu-process.c)

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

##################################################
# Check for gtk-doc.
##################################################

AC_ARG_WITH(html-dir, [  --with-html-dir=PATH path to installed docs ])

if test "x$with_html_dir" = "x" ; then
  HTML_DIR='${datadir}/gtk-doc/html'
else
  HTML_DIR=$with_html_dir
fi

AC_SUBST(HTML_DIR)

gtk_doc_min_version=1.0
AC_MSG_CHECKING([gtk-doc version >= $gtk_doc_min_version])
if pkg-config --atleast-version=$gtk_doc_min_version gtk-doc; then
  AC_MSG_RESULT(yes)
  GTKDOC=true
else
  AC_MSG_RESULT(no)
  GTKDOC=false
fi

dnl Let people disable the gtk-doc stuff.
AC_ARG_ENABLE(gtk-doc, [  --enable-gtk-doc  Use gtk-doc to build documentation [default=yes]], enable_gtk_doc="$enableval", enable_gtk_doc=yes)

AM_CONDITIONAL(ENABLE_GTK_DOC, test x$enable_gtk_doc = xyes)
AM_CONDITIONAL(GTK_DOC_USE_LIBTOOL, test x$use_libtool = xyes)
# end of gtk-doc check

lt_simple_link_test_code='int main(){return(0);}'
AC_LIBTOOL_LINKER_OPTION([whether the linker accepts the --version-script flag],
                   use_version_script, [-Wl,--version-script=libgksu/libgksu.ver])
AM_CONDITIONAL(USE_VERSION_SCRIPT, test x$use_version_script = xyes)

AC_PATH_PROG(DBUSBINDINGTOOL, dbus-binding-tool)
AC_SUBST(DBUSBINDINGTOOL)

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
        DBUS_SYS_DIR="$sysconfdir/dbus-1/system.d"
fi
AC_SUBST(DBUS_SYS_DIR)

PKG_CHECK_MODULES(GKSUPKMECH, [gobject-2.0 >= 2.0.0, dbus-glib-1])

PKG_CHECK_MODULES(GKSUPKLIB, [gobject-2.0 >= 2.0.0, dbus-glib-1])

PKG_CHECK_MODULES(GKSUPKCOMMON, [gobject-2.0 >= 2.0.0, glib-2.0, gee-1.0])

PKG_CHECK_MODULES(GKSUPK, [gobject-2.0 >= 2.0.0])

AC_CONFIG_FILES([
        Makefile
        common/Makefile
        data/Makefile
        mechanism/Makefile
        libgksu/Makefile
        libgksu/libgksu-polkit-1.pc
        gksu/Makefile
        po/Makefile.in
        ])
AC_OUTPUT
