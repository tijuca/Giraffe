# -*- Makefile -*-

AM_CPPFLAGS = ${ZCPPFLAGS} $(DEBUGFLAGS) $(BOOST_CPPFLAGS) \
	-I${top_srcdir}/mapi4linux/include \
	-I${top_srcdir}/common -I${top_srcdir}/m4lcommon \
	-I${top_srcdir}/libicalmapi

bin_PROGRAMS = zarafa-fsck
noinst_DATA = zarafa-fsck.ldx

zarafa_fsck_LDADD = ${top_builddir}/mapi4linux/src/libmapi.la \
	${top_builddir}/common/libzcp_common_mapi.la \
	${top_builddir}/common/libzcp_common_util.la \
	$(PROG_LIBS)

zarafa_fsck_SOURCES = zarafa-fsck.cpp zarafa-fsck-main.cpp zarafa-fsck-calendar.cpp zarafa-fsck-contact.cpp zarafa-fsck-task.cpp zarafa-fsck.h

check-syntax:
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) \
		-fsyntax-only -fmessage-length=0 ${CHK_SOURCES} -Wall -Wformat=2 $(zarafa_fsck_SOURCES)

include ../../global.am
