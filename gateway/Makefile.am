# -*- Makefile -*-

AM_CPPFLAGS = ${ZCPPFLAGS} ${DEBUGFLAGS} \
	${BOOST_CPPFLAGS} ${CRYPTO_CFLAGS} ${SSL_CFLAGS} \
	-I${top_srcdir}/mapi4linux/include \
	-I${top_srcdir}/inetmapi -I${top_srcdir}/provider/common \
	-I${top_srcdir}/common -I${top_srcdir}/m4lcommon

sbin_PROGRAMS = zarafa-gateway
noinst_DATA = zarafa-gateway.ldx

zarafa_gateway_LDADD = ${top_builddir}/inetmapi/libinetmapi.la \
	${top_builddir}/mapi4linux/src/libmapi.la \
	${top_builddir}/common/libzcp_common_mapi.la \
	${top_builddir}/common/libzcp_common_util.la \
	${top_builddir}/common/libzcp_common_ssl.la \
	${PROG_LIBS} ${CRYPTO_LIBS} ${SSL_LIBS} ${XML2_LIBS} ${icu_uc_LIBS}

zarafa_gateway_SOURCES = Gateway.cpp POP3.cpp POP3.h IMAP.cpp IMAP.h ClientProto.h

check-syntax:
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) \
		-fsyntax-only -fmessage-length=0 ${CHK_SOURCES} -Wall -Wformat=2

include ../global.am
