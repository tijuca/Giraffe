# -*- Makefile -*-

AM_CPPFLAGS = ${ZCPPFLAGS} $(DEBUGFLAGS) $(BOOST_CPPFLAGS) \
	-I${top_srcdir}/mapi4linux/include \
	-I${top_srcdir}/common -I${top_srcdir}/m4lcommon \
	-I${top_srcdir}/ECtools/zarafa-archiver

sbin_PROGRAMS = zarafa-admin
noinst_DATA = zarafa-admin.ldx

zarafa_admin_LDADD = ${top_builddir}/mapi4linux/src/libmapi.la \
	${top_builddir}/common/libzcp_common_mapi.la \
	${top_builddir}/common/libzcp_common_util.la \
	${top_builddir}/common/libzcp_common_ssl.la \
	${top_builddir}/ECtools/zarafa-archiver/libzarafa-archiver.la \
	$(PROG_LIBS) $(SSL_LIBS)

zarafa_admin_SOURCES = zarafa-admin.cpp

check-syntax:
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) \
		-fsyntax-only -fmessage-length=0 ${CHK_SOURCES} -Wall -Wformat=2

include ../../global.am
