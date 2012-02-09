lib_LTLIBRARIES = libfreebusy.la

AM_CPPFLAGS = $(DEBUGFLAGS) -DLINUX $(BOOST_CPPFLAGS) \
	-I${top_srcdir}/mapi4linux/include -I${top_srcdir}/provider/common \
	-I${top_srcdir}/common

libfreebusy_la_includedir = $(includedir)/libfreebusy

libfreebusy_la_LDFLAGS = -static

libfreebusy_la_include_HEADERS = ECEnumFBBlock.h ECFBBlockList.h		\
	ECFreeBusyData.h ECFreeBusySupport.h ECFreeBusyUpdate.h freebusy.h	\
	freebusyguid.h freebusytags.h freebusyutil.h PublishFreeBusy.h recurrence.h

libfreebusy_la_SOURCES = ECEnumFBBlock.cpp ECFBBlockList.cpp		\
	ECFreeBusyData.cpp ECFreeBusySupport.cpp ECFreeBusyUpdate.cpp	\
	freebusyutil.cpp PublishFreeBusy.cpp recurrence.cpp

check-syntax:
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) \
		-fsyntax-only -fmessage-length=0 ${CHK_SOURCES} -Wall -Wformat=2
