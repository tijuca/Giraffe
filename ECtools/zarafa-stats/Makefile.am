bin_PROGRAMS = zarafa-stats

if ! OSS_ONLY
dist_noinst_DATA = zarafa-ssm zarafa-ssm.default zarafa-ssm.cron
endif

AM_CPPFLAGS = $(DEBUGFLAGS) $(BOOST_CPPFLAGS) -DLINUX \
	-I${top_srcdir}/mapi4linux/include \
	-I${top_srcdir}/libfreebusy	\
	-I${top_srcdir}/common

zarafa_stats_LDADD = \
	${top_builddir}/mapi4linux/src/libmapi.la \
	${top_builddir}/common/libcommon_mapi.la \
	${top_builddir}/common/libcommon_util.la \
	$(PROG_LIBS) -lncurses

zarafa_stats_SOURCES = zarafa-stats.cpp

install-data-hook:
if ! OSS_ONLY
	mkdir -p $(DESTDIR)/$(sysconfdir)/default
	install -m 644 ${top_srcdir}/ECtools/zarafa-stats/zarafa-ssm.default $(DESTDIR)/$(sysconfdir)/default/zarafa-ssm
	mkdir -p $(DESTDIR)/$(sysconfdir)/cron.d
	install -m 644 ${top_srcdir}/ECtools/zarafa-stats/zarafa-ssm.cron $(DESTDIR)/$(sysconfdir)/cron.d/zarafa-ssm
	mkdir -p $(DESTDIR)/$(bindir)
	install -m 755 ${top_srcdir}/ECtools/zarafa-stats/zarafa-ssm $(DESTDIR)/$(bindir)
endif

check-syntax:
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) \
		-fsyntax-only -fmessage-length=0 ${CHK_SOURCES} -Wall -Wformat=2
