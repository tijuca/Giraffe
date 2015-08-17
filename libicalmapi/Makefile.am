# -*- Makefile -*-

lib_LTLIBRARIES = libicalmapi.la
noinst_DATA = libicalmapi.ldd

AM_CPPFLAGS = ${ZCPPFLAGS} $(DEBUGFLAGS) $(BOOST_CPPFLAGS) \
	$(ICAL_CFLAGS) \
	-I$(top_srcdir)/mapi4linux/include -I$(top_srcdir)/provider/common \
	-I${top_srcdir}/common -I${top_srcdir}/m4lcommon \
	-I$(top_srcdir)/libfreebusy

libicalmapi_la_LDFLAGS = ${AM_LDFLAGS} \
	-version-info ${ECCLIENT_LIBRARY_VERSION}
	-Wl,--version-script=${top_builddir}/default.sym
EXTRA_libicalmapi_la_DEPENDENCIES = ${top_builddir}/default.sym

libicalmapi_la_LIBADD = \
	${top_builddir}/libfreebusy/libfreebusy.la \
	${top_builddir}/mapi4linux/src/libmapi.la \
	${top_builddir}/common/libzcp_common_mapi.la \
	${top_builddir}/common/libzcp_common_util.la \
	$(ICAL_LIBS)

libicalmapidir = $(includedir)/icalmapi
libicalmapi_HEADERS = icalitem.h nameids.h icaluid.h \
	MAPIToICal.h ICalToMAPI.h \
	vtimezone.h valarm.h vconverter.h vevent.h vtodo.h \
	icalrecurrence.h icalmapi.h vfreebusy.h

libicalmapi_la_SOURCES = icalinit.cpp \
	nameids.cpp \
	icaluid.cpp \
	MAPIToICal.cpp \
	ICalToMAPI.cpp \
	vtimezone.cpp \
	valarm.cpp \
	vconverter.cpp \
	vevent.cpp \
	vtodo.cpp \
	icalrecurrence.cpp \
	vfreebusy.cpp


check-syntax:
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) \
		-fsyntax-only -fmessage-length=0 ${CHK_SOURCES} -Wall -Wformat=2

include ../global.am
