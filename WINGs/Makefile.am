## automake input file for WINGs

AUTOMAKE_OPTIONS =

SUBDIRS = WINGs . po Documentation Resources

libWINGs_la_LDFLAGS = -version-info @WINGS_VERSION@
libWUtil_la_LDFLAGS = -version-info @WUTIL_VERSION@

bin_SCRIPTS = get-wings-flags get-wutil-flags

lib_LTLIBRARIES = libWUtil.la libWINGs.la


LDADD= libWUtil.la libWINGs.la $(top_builddir)/wrlib/libwraster.la @INTLIBS@
libWINGs_la_LIBADD = libWUtil.la $(top_builddir)/wrlib/libwraster.la @XLIBS@ @XFTLIBS@ @FCLIBS@ @LIBM@
libWUtil_la_LIBADD = @LIBBSD@

EXTRA_DIST = BUGS make-rgb Examples Extras Tests get-wings-flags.in get-wutil-flags.in


# wbutton.c 
libWINGs_la_SOURCES = 	\
	configuration.c \
	dragcommon.c \
	dragdestination.c \
	dragsource.c \
	rgb.h \
	selection.c \
	wappresource.c \
	wballoon.c \
	wbox.c \
	wbrowser.c \
	wbutton.c \
	wcolor.c \
	wcolorpanel.c \
	wcolorwell.c \
	wconfig.h \
	wevent.c \
	wfilepanel.c \
	wframe.c \
	wfont.c \
	wfontpanel.c \
	widgets.c \
	winputmethod.c \
	wlabel.c \
	wlist.c \
	wmenuitem.c \
	wmisc.c \
	wpanel.c \
	wpixmap.c \
	wpopupbutton.c \
	wprogressindicator.c \
	wruler.c \
	wscroller.c \
	wscrollview.c \
	wslider.c \
	wsplitview.c \
	wtabview.c \
	wtext.c \
	wtextfield.c \
	wview.c \
	wwindow.c

libWUtil_la_SOURCES = 	\
	array.c \
	bagtree.c \
	data.c \
	error.c \
	findfile.c \
	handlers.c \
	hashtable.c \
	memory.c \
	menuparser.c \
	menuparser.h \
	menuparser_macros.c \
	misc.c \
	notification.c \
	proplist.c \
	string.c \
	tree.c \
	userdefaults.c \
	usleep.c \
	wapplication.c \
	wconfig.h \
	wutil.c


AM_CFLAGS =

AM_CPPFLAGS = -DLOCALEDIR=\"$(NLSDIR)\" -DRESOURCE_PATH=\"$(datadir)/WINGs\" \
	 -I$(top_srcdir)/WINGs/WINGs -I$(top_srcdir)/wrlib -I$(top_srcdir)/src \
	@XFTFLAGS@ @HEADER_SEARCH_PATH@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = WINGs.pc

DISTCLEANFILES = $(pkgconfig_DATA) get-wings-flags get-wutil-flags

WINGs.pc: Makefile
	@echo "Generating $@"
	@echo 'Name: WINGs' > $@
	@echo 'Description: Small widget set with the NeXTStep(TM) look and feel' >> $@
	@echo 'Version: $(VERSION)' >> $@
	@echo 'Requires: wrlib' >> $@
	@echo 'Libs: $(lib_search_path) -lWINGs' >> $@
	@echo 'Libs.private: $(XFTLIBS) $(XLIBS) -lm $(INTLIBS)' >> $@
	@echo 'Cflags: $(inc_search_path)' >> $@

get-wings-flags: get-wings-flags.in Makefile
	@echo "Generating $@"
	@$(SED) -e 's#$${inc_search_path}#$(inc_search_path)#;' \
	        -e 's#$${lib_search_path}#$(lib_search_path)#;' \
	        -e 's#$${GFXLIBS}#$(GFXLIBS)#;' \
	        -e 's#$${XFTLIBS}#$(XFTLIBS)#;' \
	        -e 's#$${INTLIBS}#$(INTLIBS)#;' \
	        -e 's#$${XLIBS}#$(XLIBS)#;' < $(abs_srcdir)/get-wings-flags.in > $@
	@chmod 755 $@

get-wutil-flags: get-wutil-flags.in Makefile
	@echo "Generating $@"
	@$(SED) -e 's#$${includedir}#$(includedir)#;' \
	        -e 's#$${libdir}#$(libdir)#;' \
	        -e 's#$${INTLIBS}#$(INTLIBS)#;' < $(abs_srcdir)/get-wutil-flags.in > $@
	@chmod 755 $@

