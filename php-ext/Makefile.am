lib_LTLIBRARIES = mapi.la

libdir=$(PHP_EXTENSION_DIR)

AM_CPPFLAGS = $(DEBUGFLAGS) $(BOOST_CPPFLAGS) -DLINUX \
	-I$(top_srcdir)/mapi4linux/include \
	-I$(top_srcdir)/libfreebusy \
	-I$(top_srcdir)/inetmapi \
	-I${top_srcdir}/common \
	$(PHP_INCLUDES)

mapi_la_LDFLAGS = -module -avoid-version \
	$(PHP_LDFLAGS)

mapi_la_LIBADD = \
	${top_builddir}/mapi4linux/src/libmapi.la \
	${top_builddir}/common/libcommon_mapi.la \
	${top_builddir}/common/libcommon_util.la \
	${top_builddir}/libfreebusy/libfreebusy.la \
	${top_builddir}/inetmapi/libinetmapi.la \
	$(PROG_LIBS) -lpthread

# it seems linking libraries with -pthread will not contain -lpthread.
# but because php is not linked against -lpthread, the library will not be loaded,
# and be missing at runtime.
# therefore, we explicitly link mapi.so with -lpthread :(

dist_data_DATA = include/mapi/mapicode.php include/mapi/mapidefs.php include/mapi/mapitags.php include/mapi/mapi.util.php include/mapi/class.recurrence.php include/mapi/class.meetingrequest.php include/mapi/class.freebusypublish.php include/mapi/class.baseexception.php include/mapi/class.mapiexception.php include/mapi/mapiguid.php include/mapi/class.baserecurrence.php include/mapi/class.taskrequest.php include/mapi/class.taskrecurrence.php
datadir = ${datarootdir}/php/mapi

sysconfdir = ${PHP_SYSCONF_DIR}
dist_sysconf_DATA = zarafa.ini

mapi_la_SOURCES = main.cpp util.cpp \
	phpconfig.h main.h util.h \
	ECImportContentsChangesProxy.cpp ECImportContentsChangesProxy.h \
	ECImportHierarchyChangesProxy.cpp ECImportHierarchyChangesProxy.h \
	typeconversion.cpp typeconversion.h \
	globals.h

check-syntax:
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) \
		-fsyntax-only -fmessage-length=0 ${CHK_SOURCES} -Wall -Wformat=2 -Wno-strict-aliasing
