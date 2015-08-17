# -*- Makefile -*-

AM_CPPFLAGS = ${ZCPPFLAGS} $(DEBUGFLAGS) $(BOOST_CPPFLAGS) \
	-DARCHIVER_EXTRA \
	-I${top_srcdir}/mapi4linux/include \
	-I${top_srcdir}/provider/client \
	-I${top_srcdir}/provider/include \
	-I${top_srcdir}/provider/soap \
	-I${top_builddir}/provider/soap \
    $(GSOAP_CFLAGS) \
	-I${top_srcdir}/common -I${top_srcdir}/m4lcommon \
	$(MYSQL_INCLUDES)

sbin_PROGRAMS = zarafa-archiver
lib_LTLIBRARIES = libzarafa-archiver.la libzarafa-archiver-core.la
noinst_DATA = libzarafa-archiver.ldd libzarafa-archiver-core.ldd \
	zarafa-archiver.ldx

libzarafa_archiver_la_LIBADD = ${top_builddir}/mapi4linux/src/libmapi.la \
	${top_builddir}/common/libzcp_common_mapi.la \
	${top_builddir}/common/libzcp_common_util.la

libzarafa_archiver_la_SOURCES = \
	ArchiverSession.cpp ArchiverSession.h ArchiverSessionPtr.h \
	archiver-common.cpp archiver-common.h ArchiveControl.h \
	ArchiveManageImpl.cpp ArchiveManageImpl.h ArchiveManage.h\
	ArchiveStateCollector.cpp ArchiveStateCollector.h \
	ArchiveStateUpdater.cpp ArchiveStateUpdater.h archivestateupdater_fwd.h \
	helpers/ArchiveHelper.cpp helpers/ArchiveHelper.h \
	helpers/StoreHelper.cpp helpers/StoreHelper.h \
	helpers/MAPIPropHelper.cpp helpers/MAPIPropHelper.h \
	ECArchiverLogger.cpp ECArchiverLogger.h

libzarafa_archiver_la_LDFLAGS = ${AM_LDFLAGS} -no-undefined \
	-Wl,--version-script=${top_builddir}/default.sym
EXTRA_libzarafa_archiver_la_DEPENDENCIES = ${top_builddir}/default.sym


libzarafa_archiver_core_la_LIBADD = ${top_builddir}/mapi4linux/src/libmapi.la \
	${top_builddir}/common/libzcp_common_mapi.la \
	${top_builddir}/common/libzcp_common_service.la \
	libzarafa-archiver.la \
	$(PROG_LIBS) $(SSL_LIBS) $(MYSQL_LIBS)

libzarafa_archiver_core_la_SOURCES = \
	Archiver.cpp Archiver.h AutoMAPI.h\
	ArchiverImpl.cpp ArchiverImpl.h\
	operations/instanceidmapper.cpp operations/instanceidmapper.h operations/instanceidmapper_fwd.h \
	operations/operations.cpp operations/operations.h operations/operations_fwd.h \
	operations/copier.cpp operations/copier.h \
	operations/deleter.cpp operations/deleter.h \
	operations/stubber.cpp operations/stubber.h \
	operations/transaction.cpp operations/transaction.h operations/transaction_fwd.h \
	operations/postsaveaction.h \
	operations/postsaveiidupdater.cpp operations/postsaveiidupdater.h \
	ArchiveControlImpl.cpp ArchiveControlImpl.h \
	ECDatabase.h ECDatabase.cpp

libzarafa_archiver_core_la_LDFLAGS = ${AM_LDFLAGS} -no-undefined \
	-Wl,--version-script=${top_builddir}/default.sym
EXTRA_libzarafa_archiver_core_la_DEPENDENCIES = ${top_builddir}/default.sym

zarafa_archiver_LDADD = ${top_builddir}/mapi4linux/src/libmapi.la \
	${top_builddir}/common/libzcp_common_mapi.la \
	${top_builddir}/common/libzcp_common_util.la \
	libzarafa-archiver.la libzarafa-archiver-core.la \
	$(PROG_LIBS) ${top_builddir}/common/libzcp_common_service.la

zarafa_archiver_SOURCES = main.cpp



check-syntax:
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) \
		-fsyntax-only -fmessage-length=0 ${CHK_SOURCES} -Wall -Wformat=2

install-exec-hook:
	${MKDIR_P} ${DESTDIR}${pkglibdir}
	ln -fs ${libdir}/libzarafa-archiver.so ${DESTDIR}${pkglibdir}/libarchiver.so
	ln -fs ${libdir}/libzarafa-archiver-core.so ${DESTDIR}${pkglibdir}/libarchiver-core.so

include ../../global.am
