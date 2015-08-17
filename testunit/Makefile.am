# -*- Makefile -*-

AM_CPPFLAGS = ${ZCPPFLAGS} -I${top_srcdir}/common -I${top_srcdir}/m4lcommon \
              -I${top_srcdir}/mapi4linux/include -I${top_srcdir}/inetmapi \
              -DSOURCEDIR=\"${abs_srcdir}\"

# Always build tests to ensure they line up with the API.
noinst_PROGRAMS = imtomapi ${check_PROGRAMS}
# Programs to run during `make check`.
# imtomapi is left out for now since it requires a running zarafa-server.
check_PROGRAMS =
TESTS = ${check_PROGRAMS}

imtomapi_SOURCES = imtomapi.cpp
imtomapi_LDADD = ../inetmapi/libinetmapi.la ../mapi4linux/src/libmapi.la \
	../common/libzcp_common_util.la ../common/libzcp_common_mapi.la
