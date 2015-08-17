# -*- Makefile -*-

AM_CPPFLAGS = ${ZCPPFLAGS} $(DEBUGFLAGS) \
	-I${top_srcdir}/common -I${top_srcdir}/m4lcommon

noinst_PROGRAMS = zarafa-cfgchecker
noinst_DATA = zarafa-cfgchecker.ldx

zarafa_cfgchecker_LDADD = \
	${top_builddir}/common/libzcp_common_util.la \
	${top_builddir}/common/libzcp_common_mapi.la \
	$(PROG_LIBS) ${top_builddir}/mapi4linux/src/libmapi.la

zarafa_cfgchecker_SOURCES = \
	zarafa-cfg.cpp \
	ECConfigCheck.cpp ECConfigCheck.h \
	LDAPConfigCheck.cpp LDAPConfigCheck.h \
	UnixConfigCheck.cpp UnixConfigCheck.h \
	ServerConfigCheck.cpp ServerConfigCheck.h \
	GatewayConfigCheck.cpp GatewayConfigCheck.h \
	IcalConfigCheck.cpp IcalConfigCheck.h \
	MonitorConfigCheck.cpp MonitorConfigCheck.h \
	DAgentConfigCheck.cpp DAgentConfigCheck.h \
	SpoolerConfigCheck.cpp SpoolerConfigCheck.h 

check-syntax:
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) \
		-fsyntax-only -fmessage-length=0 ${CHK_SOURCES} -Wall -Wformat=2 $(zarafa_cfg_SOURCES)

include ../../global.am
