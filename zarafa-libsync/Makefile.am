lib_LTLIBRARIES = libzarafasync.la

AM_CPPFLAGS = $(DEBUGFLAGS) -DLINUX	$(BOOST_CPPFLAGS)	\
	-I${top_srcdir}/mapi4linux/include	\
	-I${top_srcdir}/provider/common 	\
	-I${top_srcdir}/common

libzarafasync_la_LIBADD = \
	${top_builddir}/mapi4linux/src/libmapi.la \
	${top_builddir}/common/libcommon_mapi.la

libzarafasync_la_includedir = $(includedir)/libzarafasync

libzarafasync_la_include_HEADERS = ECLibSync.h ECSync.h ECSyncLog.h	\
	ECSyncUtil.h ECSyncContext.h ECOfflineABImporter.h ECSyncSettings.h ECResyncSet.h

libzarafasync_la_SOURCES = ECSync.cpp ECSyncLog.cpp ECSyncUtil.cpp	\
	ECSyncContext.cpp ECOfflineABImporter.cpp ECSyncSettings.cpp ECResyncSet.cpp

check-syntax:
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) \
		-fsyntax-only -fmessage-length=0 ${CHK_SOURCES} -Wall -Wformat=2
