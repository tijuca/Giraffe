lib_LTLIBRARIES = libarchiver.la

AM_CPPFLAGS = $(DEBUGFLAGS) $(BOOST_CPPFLAGS) -DLINUX -DARCHIVER_EXTRA \
	-I${top_srcdir}/mapi4linux/include \
	-I${top_srcdir}/common \
	$(MYSQL_INCLUDES)



libarchiver_la_LIBADD = ${top_builddir}/mapi4linux/src/libmapi.la \
	${top_builddir}/common/libcommon_mapi.la \
	${top_builddir}/common/libcommon_util.la \
	$(PROG_LIBS) $(SSL_LIBS) $(MYSQL_LIBS)

libarchiver_la_SOURCES = \
	archiver-session.cpp archiver-session.h archiver-session_fwd.h \
	archiver-common.cpp archiver-common.h \
	archivemanage.cpp archivemanage.h \
	archiver.cpp archiver.h \
	archivectrl.cpp archivectrl.h \
	archivestatecollector.cpp archivestatecollector.h \
	archivestateupdater.cpp archivestateupdater.h archivestateupdater_fwd.h \
	helpers/archivehelper.cpp helpers/archivehelper.h \
	helpers/storehelper.cpp helpers/storehelper.h \
	helpers/mapiprophelper.cpp helpers/mapiprophelper.h \
	logger.cpp logger.h \
	operations/operations.cpp operations/operations.h operations/operations_fwd.h \
	operations/copier.cpp operations/copier.h \
	operations/deleter.cpp operations/deleter.h \
	operations/stubber.cpp operations/stubber.h \
	operations/transaction.cpp operations/transaction.h operations/transaction_fwd.h \
	operations/instanceidmapper.cpp operations/instanceidmapper.h operations/instanceidmapper_fwd.h \
	operations/postsaveaction.h \
	operations/postsaveiidupdater.cpp operations/postsaveiidupdater.h \
	ECDatabaseMySQL.cpp ECDatabaseMySQL.h

bin_PROGRAMS = zarafa-archiver

zarafa_archiver_LDADD = ${top_builddir}/common/libcommon_util.la \
	libarchiver.la \
	$(PROG_LIBS)

zarafa_archiver_SOURCES = main.cpp


check-syntax:
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) \
		-fsyntax-only -fmessage-length=0 ${CHK_SOURCES} -Wall -Wformat=2
