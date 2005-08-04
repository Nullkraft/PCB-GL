## $Id: Makefile.am,v 1.2 2005-08-04 03:23:56 danmc Exp $
##

## Note:  We put this script in a different directory from the Pcb
## app-defaults file because certain platforms like cygwin don't have 
## case sensitive file systems.

bin_SCRIPTS=	pcb pcb-print

EXTRA_DIST= pcb.in 

DISTCLEANFILES= pcb

# create wrapper script for running pcb after installation
pcb:	$(srcdir)/pcb.in Makefile
	sed -e 's;@BINDIR@;${bindir};g' -e 's;@PKGDATADIR@;${pkgdatadir};g' \
		$(srcdir)/pcb.in > $@
	chmod 755 $@


