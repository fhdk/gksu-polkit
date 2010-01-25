#!/bin/sh
# Run this to generate all the initial makefiles, etc.

rm -rf autom4te.cache
aclocal -I m4 || exit $?
libtoolize --copy --force || exit $?
gtkdocize --copy --flavour no-tmpl || exit $?
intltoolize --copy --force --automake
automake --copy --add-missing || exit $?
autoconf || exit $?

./configure $@
