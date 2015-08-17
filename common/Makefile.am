# -*- Makefile -*-

AM_CPPFLAGS = ${ZCPPFLAGS} ${DEBUGFLAGS} ${BOOST_CPPFLAGS} \
	-I${top_srcdir}/m4lcommon \
	-I${top_srcdir}/mapi4linux/include -I${top_srcdir}/provider/common \
	-I${top_srcdir}/libfreebusy ${MYSQL_INCLUDES}

pkgincludedir = ${includedir}/zarafa
pkgconfdir = ${libdir}/pkgconfig
pkgconf_DATA = zarafa.pc

lib_LTLIBRARIES = \
	libzcp_common_util.la libzcp_common_mapi.la \
	libzcp_common_ssl.la libzcp_common_service.la
noinst_DATA = \
	libzcp_common_util.ldd libzcp_common_mapi.ldd \
	libzcp_common_ssl.ldd libzcp_common_service.ldd

#
# libzcp_common_util
#
nobase_pkginclude_HEADERS = \
	ConsoleTable.h ECACL.h ECCache.h ECConfig.h ECConfigImpl.h \
	ECFifoBuffer.h ECGetText.h ECIConv.h ECLogger.h \
	ECScheduler.h ECSerializer.h ECThreadPool.h StatsClient.h \
	SymmetricCrypt.h TimeUtil.h TmpPath.h Trace.h UnixUtil.h \
	base64.h fileutil.h md5.h my_getopt.h \
	platform.linux.h platform.h pthreadutil.h \
	stringutil.h threadutil.h ustringutil.h utf8.h zcdefs.h \
	charset/convert.h charset/traits.h charset/utf32string.h \
	charset/utf8string.h charset/utf16string.h charset/convstring.h \
	charset/localeutil.h \
	ustringutil/utfutil.h \
	utf8/checked.h utf8/core.h utf8/unchecked.h
libzcp_common_util_la_SOURCES = \
	ConsoleTable.cpp ECACL.cpp ECCache.cpp ECConfig.cpp ECConfigImpl.cpp \
	ECFifoBuffer.cpp ECGetText.cpp ECIConv.cpp ECLogger.cpp \
	ECScheduler.cpp ECSerializer.cpp ECThreadPool.cpp StatsClient.cpp \
	SymmetricCrypt.cpp TimeUtil.cpp TmpPath.cpp Trace.cpp UnixUtil.cpp \
	base64.cpp fileutil.cpp md5.cpp my_getopt.cpp \
	platform.cpp platform.linux.cpp \
	stringutil.cpp threadutil.cpp ustringutil.cpp \
	charset/convert.cpp charset/convstring.cpp charset/traits.cpp \
	charset/localeutil.cpp
libzcp_common_util_la_LIBADD = \
	-lpthread -lz ${BOOST_FILESYSTEM_LIBS} ${UUID_LIBS}
libzcp_common_util_la_LDFLAGS = ${AM_LDFLAGS} -no-undefined \
	-Wl,--version-script=${top_builddir}/default.sym
EXTRA_libzcp_common_util_la_DEPENDENCIES = ${top_builddir}/default.sym

if HAVE_ICU
libzcp_common_util_la_SOURCES += ustringutil/utfutil.cpp
libzcp_common_util_la_LIBADD += ${ICU_LIBS}
endif

#
# libzcp_common_mapi
#
nobase_pkginclude_HEADERS += \
	ECABEntryID.h ECDebug.h ECDebugPrint.h ECGuid.h \
	ECFifoStream.h ECKeyTable.h ECMemStream.h ECUnknown.h \
	HtmlEntity.h HtmlToTextParser.h IStreamAdapter.h \
	MAPIErrors.h ZarafaCode.h codepage.h rtfutil.h
libzcp_common_mapi_la_SOURCES = \
	ECABEntryID.cpp ECDebug.cpp ECDebugPrint.cpp ECGuid.cpp \
	ECFifoStream.cpp ECKeyTable.cpp ECMemStream.cpp ECUnknown.cpp \
	HtmlEntity.cpp HtmlToTextParser.cpp IStreamAdapter.cpp \
	MAPIErrors.cpp ZarafaCode.cpp codepage.cpp rtfutil.cpp
libzcp_common_mapi_la_LIBADD = libzcp_common_util.la -lpthread
libzcp_common_mapi_la_LDFLAGS = ${AM_LDFLAGS} -no-undefined \
	-Wl,--version-script=${top_builddir}/default.sym
EXTRA_libzcp_common_mapi_la_DEPENDENCIES = ${top_builddir}/default.sym

#
# libzcp_common_ssl
#
nobase_pkginclude_HEADERS += ECChannel.h ECChannelClient.h SSLUtil.h
libzcp_common_ssl_la_SOURCES = ECChannel.cpp ECChannelClient.cpp SSLUtil.cpp
libzcp_common_ssl_la_LIBADD = libzcp_common_util.la -lpthread ${SSL_LIBS}
libzcp_common_ssl_la_LDFLAGS = ${AM_LDFLAGS} -no-undefined \
	-Wl,--version-script=${top_builddir}/default.sym
EXTRA_libzcp_common_ssl_la_DEPENDENCIES = ${top_builddir}/default.sym

#
# libzcp_common_service
#
nobase_pkginclude_HEADERS += ECDatabaseMySQL.h
libzcp_common_service_la_SOURCES = ECDatabaseMySQL.cpp
libzcp_common_service_la_LIBADD = libzcp_common_util.la -lpthread ${MYSQL_LIBS}
libzcp_common_service_la_LDFLAGS = ${AM_LDFLAGS} -no-undefined \
	-Wl,--version-script=${top_builddir}/default.sym
EXTRA_libzcp_common_service_la_DEPENDENCIES = ${top_builddir}/default.sym


# Create historic symlinks (e.g. older snapshots of zstatsd use it) for build
install-exec-hook:
	${MKDIR_P} ${DESTDIR}${pkglibdir}
	ln -fs ${libdir}/libzcp_common_util.so ${DESTDIR}${pkglibdir}/libcommon_util.so
	ln -fs ${libdir}/libzcp_common_mapi.so ${DESTDIR}${pkglibdir}/libcommon_mapi.so
	ln -fs ${libdir}/libzcp_common_ssl.so ${DESTDIR}${pkglibdir}/libcommon_ssl.so
	ln -fs ${libdir}/libzcp_common_service.so ${DESTDIR}${pkglibdir}/libcommon_service.so

check-syntax:
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) \
		-fsyntax-only -fmessage-length=0 ${CHK_SOURCES} -Wall -Wformat=2

include ../global.am
