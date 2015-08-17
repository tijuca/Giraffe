# -*- Makefile -*-

AM_CPPFLAGS = ${ZCPPFLAGS} ${DEBUGFLAGS} ${BOOST_CPPFLAGS} \
	-I${top_srcdir}/mapi4linux/include -I${top_srcdir}/provider/common \
	-I${top_srcdir}/common -I${top_srcdir}/m4lcommon

lib_LTLIBRARIES = libfreebusy.la
noinst_DATA = libfreebusy.ldd

libfreebusy_la_includedir = $(includedir)/libfreebusy

libfreebusy_la_include_HEADERS = ECEnumFBBlock.h ECFBBlockList.h		\
	ECFreeBusyData.h ECFreeBusySupport.h ECFreeBusyUpdate.h freebusy.h	\
	freebusyguid.h freebusytags.h freebusyutil.h PublishFreeBusy.h recurrence.h

libfreebusy_la_SOURCES = ECEnumFBBlock.cpp ECFBBlockList.cpp		\
	ECFreeBusyData.cpp ECFreeBusySupport.cpp ECFreeBusyUpdate.cpp	\
	freebusyutil.cpp PublishFreeBusy.cpp recurrence.cpp
libfreebusy_la_LIBADD = \
	${top_builddir}/common/libzcp_common_util.la \
	${top_builddir}/common/libzcp_common_mapi.la \
	${top_builddir}/mapi4linux/src/libmapi.la
libfreebusy_la_LDFLAGS = ${AM_LDFLAGS} -no-undefined \
	-Wl,--version-script=${top_builddir}/default.sym
EXTRA_libfreebusy_la_DEPENDENCIES = ${top_builddir}/default.sym

check-syntax:
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) \
		-fsyntax-only -fmessage-length=0 ${CHK_SOURCES} -Wall -Wformat=2

include ../global.am
