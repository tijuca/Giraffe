# -*- Makefile -*-

AM_CPPFLAGS = ${ZCPPFLAGS} ${DEBUGFLAGS} ${BOOST_CPPFLAGS} \
	-I${top_srcdir}/mapi4linux/include	\
	-I${top_srcdir}/provider/common 	\
	-I${top_srcdir}/common -I${top_srcdir}/m4lcommon

lib_LTLIBRARIES = libzarafasync.la
noinst_DATA = libzarafasync.ldd

libzarafasync_la_LIBADD = \
	${top_builddir}/common/libzcp_common_mapi.la \
	${top_builddir}/mapi4linux/src/libmapi.la

libzarafasync_la_includedir = $(includedir)/libzarafasync

libzarafasync_la_include_HEADERS = ECLibSync.h ECSync.h ECSyncLog.h	\
	ECSyncUtil.h ECSyncContext.h ECOfflineABImporter.h ECSyncSettings.h ECResyncSet.h

libzarafasync_la_SOURCES = ECSync.cpp ECSyncLog.cpp ECSyncUtil.cpp	\
	ECSyncContext.cpp ECOfflineABImporter.cpp ECSyncSettings.cpp ECResyncSet.cpp
libzarafasync_la_LDFLAGS = ${AM_LDFLAGS} -no-undefined \
	-Wl,--version-script=${top_builddir}/default.sym
EXTRA_libzarafasync_la_DEPENDENCIES = ${top_builddir}/default.sym

check-syntax:
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) \
		-fsyntax-only -fmessage-length=0 ${CHK_SOURCES} -Wall -Wformat=2

include ../global.am
