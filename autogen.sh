#! /bin/sh
#
# $Id: autogen.sh,v 1.2 2005-03-10 22:32:44 danmc Exp $
#
# Run the various GNU autotools to bootstrap the build
# system.  Should only need to be done once.

# for now avoid using bash as not everyone has that installed
CONFIG_SHELL=/bin/sh
export CONFIG_SHELL

aclocal $ACLOCAL_FLAGS
autoheader
automake -a -c --foreign
autoconf
./configure $@
