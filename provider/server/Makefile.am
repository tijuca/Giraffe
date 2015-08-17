# -*- Makefile -*-

AM_CPPFLAGS = ${ZCPPFLAGS} $(DEBUGFLAGS) $(BOOST_CPPFLAGS) \
	-I${top_srcdir}/mapi4linux/include \
	-I${top_srcdir}/provider/libserver -I${top_srcdir}/provider/plugins \
	-I${top_srcdir}/provider/include -I${top_srcdir}/provider/common \
	-I${top_srcdir}/provider/client -I${top_builddir}/provider/soap \
	-I${top_srcdir}/provider/soap \
	-I${top_srcdir}/common -I${top_srcdir}/m4lcommon \
	-DPKGLIBDIR='"${pkglibdir}"' \
	$(MYSQL_INCLUDES) $(GSOAP_CFLAGS) $(SSL_CFLAGS) $(SSL_HAS_EVP_PKEY_CMP) $(EPOLL_CFLAGS) $(KRB5_CFLAGS)

sbin_PROGRAMS = zarafa-server
noinst_DATA = zarafa-server.ldx

zarafa_server_LDFLAGS = ${AM_LDFLAGS} -Wl,-E $(BOOST_FILESYSTEM_LDFLAGS)
zarafa_server_LDADD = \
	${top_builddir}/provider/libserver/libzarafa_server.la \
    ${GSOAP_LIBS} \
	${top_builddir}/provider/soap/libzarafa_soapserver.la \
	${top_builddir}/provider/common/libzarafacommon.la						\
	${top_builddir}/common/libzcp_common_util.la \
	${top_builddir}/common/libzcp_common_mapi.la \
	${top_builddir}/common/libzcp_common_ssl.la \
	$(PROG_LIBS)															\
	$(BOOST_FILESYSTEM_LIBS) $(BOOST_SYSTEM_LIBS) $(ICU_LIBS) $(DL_LIBS)	\
	$(MYSQL_LIBS) $(SSL_LIBS) $(PAM_LIBS) $(KRB5_LIBS) $(TCMALLOC_LIBS) -lrt

zarafa_server_SOURCES = ECServer.cpp  \
						ECSoapServerConnection.h ECSoapServerConnection.cpp \
						ECClientUpdate.h ECClientUpdate.cpp \
						ECThreadManager.h ECThreadManager.cpp

check-syntax:
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) \
		-fsyntax-only -fmessage-length=0 ${CHK_SOURCES} -Wall -Wformat=2

include ../../global.am
