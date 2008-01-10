## -*- makefile -*-
## $Id: Makefile.am,v 1.9 2008-01-10 18:57:34 petercjclifton Exp $
##
## Top level automake file for PCB

DIRS=		win32 src data lib newlib example tools tutorial README_FILES intl po
SUBDIRS=	${DIRS} @DOC@
DIST_SUBDIRS=	${DIRS} doc
EXTRA_DIST=	config.rpath m4/ChangeLog autogen.sh globalconst.h README.cvs README.win32 icon-theme-installer

DISTCLEANFILES=	configure.lineno

ACLOCAL_AMFLAGS = -I m4
DISTCHECK_CONFIGURE_FLAGS = --disable-update-mime-database --disable-update-desktop-database

