# -*- Makefile -*-

SUBDIRS = . archiver/include/zarafa backup-plus presence search

AM_CPPFLAGS = ${ZCPPFLAGS} ${DEBUGFLAGS} ${BOOST_CPPFLAGS} ${SSL_CFLAGS} \
	-I${top_srcdir}/mapi4linux/include \
	-I${top_srcdir}/common -I${top_srcdir}/m4lcommon


bin_PROGRAMS = zarafa-fsck zarafa-passwd zarafa-stats
dist_bin_SCRIPTS = utils/zarafa-set-oof
sbin_PROGRAMS = zarafa-admin zarafa-archiver zarafa-monitor
dist_sbin_SCRIPTS = utils/zarafa-mailbox-permissions
noinst_PROGRAMS = zarafa-cfgchecker
lib_LTLIBRARIES = libzarafa-archiver.la libzarafa-archiver-core.la
noinst_DATA = \
	libzarafa-archiver.ldd libzarafa-archiver-core.ldd \
	zarafa-archiver.ldx zarafa-cfgchecker.ldx \
	zarafa-monitor.ldx zarafa-passwd.ldx zarafa-stats.ldx


zarafa_admin_SOURCES = \
	admin/zarafa-admin.cpp
zarafa_admin_CPPFLAGS = \
	${AM_CPPFLAGS} -I${top_srcdir}/ECtools/archiver
zarafa_admin_LDADD = \
	${top_builddir}/mapi4linux/src/libmapi.la \
	${top_builddir}/common/libzcp_common_mapi.la \
	${top_builddir}/common/libzcp_common_util.la \
	${top_builddir}/common/libzcp_common_ssl.la \
	libzarafa-archiver.la ${PROG_LIBS} ${SSL_LIBS}

zarafa_archiver_SOURCES = \
	archiver/main.cpp
zarafa_archiver_CPPFLAGS = \
	${AM_CPPFLAGS} -I${top_srcdir}/provider/client \
	-I${top_srcdir}/provider/include -I${top_srcdir}/provider/soap \
	-I${top_builddir}/provider/soap -I${top_srcdir}/ECtools/archiver \
	${GSOAP_CFLAGS} ${MYSQL_INCLUDES}
zarafa_archiver_LDADD = \
	${top_builddir}/mapi4linux/src/libmapi.la \
	${top_builddir}/common/libzcp_common_mapi.la \
	${top_builddir}/common/libzcp_common_util.la \
	libzarafa-archiver.la libzarafa-archiver-core.la \
	${top_builddir}/common/libzcp_common_service.la ${PROG_LIBS}


zarafa_cfgchecker_SOURCES = \
	cfgchecker/zarafa-cfg.cpp \
	cfgchecker/ECConfigCheck.cpp \
	cfgchecker/ECConfigCheck.h \
	cfgchecker/LDAPConfigCheck.cpp \
	cfgchecker/LDAPConfigCheck.h \
	cfgchecker/UnixConfigCheck.cpp \
	cfgchecker/UnixConfigCheck.h \
	cfgchecker/ServerConfigCheck.cpp \
	cfgchecker/ServerConfigCheck.h \
	cfgchecker/GatewayConfigCheck.cpp \
	cfgchecker/GatewayConfigCheck.h \
	cfgchecker/IcalConfigCheck.cpp \
	cfgchecker/IcalConfigCheck.h \
	cfgchecker/MonitorConfigCheck.cpp \
	cfgchecker/MonitorConfigCheck.h \
	cfgchecker/DAgentConfigCheck.cpp \
	cfgchecker/DAgentConfigCheck.h \
	cfgchecker/SpoolerConfigCheck.cpp \
	cfgchecker/SpoolerConfigCheck.h 
zarafa_cfgchecker_LDADD = \
	${top_builddir}/common/libzcp_common_util.la \
	${top_builddir}/common/libzcp_common_mapi.la \
	${top_builddir}/mapi4linux/src/libmapi.la ${PROG_LIBS}


zarafa_fsck_SOURCES = \
	fsck/zarafa-fsck.cpp \
	fsck/zarafa-fsck-main.cpp \
	fsck/zarafa-fsck-calendar.cpp \
	fsck/zarafa-fsck-contact.cpp \
	fsck/zarafa-fsck-task.cpp \
	fsck/zarafa-fsck.h
zarafa_fsck_CPPFLAGS = \
	${AM_CPPFLAGS} -I${top_srcdir}/libicalmapi
zarafa_fsck_LDADD = \
	${top_builddir}/mapi4linux/src/libmapi.la \
	${top_builddir}/common/libzcp_common_mapi.la \
	${top_builddir}/common/libzcp_common_util.la \
	${PROG_LIBS}


zarafa_monitor_SOURCES = \
	monitor/zarafa-monitor.cpp \
	monitor/ECMonitorDefs.h \
	monitor/ECQuotaMonitor.h \
	monitor/ECQuotaMonitor.cpp
zarafa_monitor_LDADD = \
	${top_builddir}/mapi4linux/src/libmapi.la \
	${top_builddir}/common/libzcp_common_mapi.la \
	${top_builddir}/common/libzcp_common_util.la \
	${PROG_LIBS}


zarafa_passwd_SOURCES = \
	passwd/zarafa-passwd.cpp
zarafa_passwd_LDADD = \
	${top_builddir}/mapi4linux/src/libmapi.la \
	${top_builddir}/common/libzcp_common_mapi.la \
	${top_builddir}/common/libzcp_common_util.la \
	${PROG_LIBS}


zarafa_stats_SOURCES = \
	stats/zarafa-stats.cpp
zarafa_stats_CPPFLAGS = \
	${AM_CPPFLAGS} ${NCURSES_FLAGS} -I${top_srcdir}/libfreebusy
# ncurses needs to come first, because ICU_LIBS erroneously has -L/usr/lib64
# which overrides the ncurses lib selection >:-(
zarafa_stats_LDADD = \
	${NCURSES_LIBS} \
	${top_builddir}/mapi4linux/src/libmapi.la \
	${top_builddir}/common/libzcp_common_mapi.la \
	${top_builddir}/common/libzcp_common_util.la \
	${PROG_LIBS}


libzarafa_archiver_la_SOURCES = \
	archiver/ArchiverSession.cpp \
	archiver/ArchiverSession.h \
	archiver/ArchiverSessionPtr.h \
	archiver/archiver-common.cpp \
	archiver/ArchiveManageImpl.cpp \
	archiver/ArchiveManageImpl.h \
	archiver/ArchiveManage.h \
	archiver/ArchiveStateCollector.cpp \
	archiver/ArchiveStateCollector.h \
	archiver/ArchiveStateUpdater.cpp \
	archiver/ArchiveStateUpdater.h \
	archiver/archivestateupdater_fwd.h \
	archiver/helpers/ArchiveHelper.cpp \
	archiver/helpers/ArchiveHelper.h \
	archiver/helpers/StoreHelper.cpp \
	archiver/helpers/StoreHelper.h \
	archiver/helpers/MAPIPropHelper.cpp \
	archiver/helpers/MAPIPropHelper.h \
	archiver/ECArchiverLogger.cpp \
	archiver/ECArchiverLogger.h
libzarafa_archiver_la_CPPFLAGS = ${zarafa_archiver_CPPFLAGS}
libzarafa_archiver_la_LDFLAGS = ${AM_LDFLAGS} -no-undefined \
	-Wl,--version-script=${top_builddir}/default.sym
libzarafa_archiver_la_LIBADD = ${top_builddir}/mapi4linux/src/libmapi.la \
	${top_builddir}/common/libzcp_common_mapi.la \
	${top_builddir}/common/libzcp_common_util.la
EXTRA_libzarafa_archiver_la_DEPENDENCIES = ${top_builddir}/default.sym


libzarafa_archiver_core_la_SOURCES = \
	archiver/Archiver.cpp \
	archiver/Archiver.h \
	archiver/AutoMAPI.h \
	archiver/ArchiverImpl.cpp \
	archiver/ArchiverImpl.h \
	archiver/operations/instanceidmapper.cpp \
	archiver/operations/instanceidmapper.h \
	archiver/operations/instanceidmapper_fwd.h \
	archiver/operations/operations.cpp \
	archiver/operations/operations.h \
	archiver/operations/operations_fwd.h \
	archiver/operations/copier.cpp \
	archiver/operations/copier.h \
	archiver/operations/deleter.cpp \
	archiver/operations/deleter.h \
	archiver/operations/stubber.cpp \
	archiver/operations/stubber.h \
	archiver/operations/transaction.cpp \
	archiver/operations/transaction.h \
	archiver/operations/transaction_fwd.h \
	archiver/operations/postsaveaction.h \
	archiver/operations/postsaveiidupdater.cpp \
	archiver/operations/postsaveiidupdater.h \
	archiver/ArchiveControlImpl.cpp \
	archiver/ArchiveControlImpl.h \
	archiver/ECDatabase.h \
	archiver/ECDatabase.cpp
libzarafa_archiver_core_la_CPPFLAGS = ${zarafa_archiver_CPPFLAGS}
libzarafa_archiver_core_la_LDFLAGS = \
	${AM_LDFLAGS} -no-undefined \
	-Wl,--version-script=${top_builddir}/default.sym
libzarafa_archiver_core_la_LIBADD = \
	${top_builddir}/mapi4linux/src/libmapi.la \
	${top_builddir}/common/libzcp_common_mapi.la \
	${top_builddir}/common/libzcp_common_service.la \
	libzarafa-archiver.la \
	${PROG_LIBS} ${SSL_LIBS} ${MYSQL_LIBS}
EXTRA_libzarafa_archiver_core_la_DEPENDENCIES = \
	${top_builddir}/default.sym


install-exec-hook:
	${MKDIR_P} ${DESTDIR}${pkglibdir}
	ln -fs ${libdir}/libzarafa-archiver.so ${DESTDIR}${pkglibdir}/libarchiver.so
	ln -fs ${libdir}/libzarafa-archiver-core.so ${DESTDIR}${pkglibdir}/libarchiver-core.so


include ../global.am
