# -*- Makefile -*-

AM_CPPFLAGS = ${ZCPPFLAGS} ${DEBUGFLAGS} ${GSOAP_CFLAGS} \
	${BOOST_CPPFLAGS} \
	-I${top_srcdir}/mapi4linux/include -I${top_srcdir}/provider/include \
	-I${top_builddir}/provider/soap -I${top_srcdir}/provider/soap \
	-I${top_srcdir}/common -I${top_srcdir}/m4lcommon

noinst_LTLIBRARIES = libzarafacommon.la
#noinst_DATA = libzarafacommon.ldd

libzarafacommon_la_SOURCES = SOAPUtils.cpp SOAPUtils.h \
	SOAPAlloc.h \
	SOAPHelpers.cpp SOAPHelpers.h \
	SOAPDebug.cpp SOAPDebug.h \
	SOAPSock.cpp SOAPSock.h \
	ZarafaUtil.cpp ZarafaUtil.h \
	ZarafaUser.h ZarafaUser.cpp \
	ECLicenseClient.h ECLicenseClient.cpp \
	ECSearchClient.h ECSearchClient.cpp \
	ECOfflineState.h ECOfflineState.cpp \
	StreamTypes.h ZarafaVersions.h
libzarafacommon_la_LIBADD = \
	../../common/libzcp_common_util.la ../../common/libzcp_common_mapi.la \
	../soap/libzarafa_soapclient.la ../../common/libzcp_common_ssl.la \
	${GSOAP_LIBS} ${SSL_LIBS}
libzarafacommon_la_LDFLAGS = ${AM_LDFLAGS} \
	-Wl,--version-script=${top_builddir}/default.sym
EXTRA_libzarafacommon_la_DEPENDENCIES = ${top_builddir}/default.sym

check-syntax:
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) \
		-fsyntax-only -fmessage-length=0 ${CHK_SOURCES} -Wall -Wformat=2

include ../../global.am
