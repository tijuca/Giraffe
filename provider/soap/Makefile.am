# -*- Makefile -*-

AM_CPPFLAGS = ${ZCPPFLAGS} -I${top_srcdir}/mapi4linux/include \
              -I${top_srcdir}/provider/include ${GSOAP_CFLAGS}
AM_CXXFLAGS = ${ZCXXFLAGS} -Wno-vla -Wno-sign-compare ${DEBUGCFLAGS}

lib_LTLIBRARIES = libzarafa_soapserver.la libzarafa_soapclient.la
#noinst_DATA = libzarafa_soapserver.ldd libzarafa_soapclient.ldd

nodist_libzarafa_soapserver_la_SOURCES = soapC.cpp soapServer.cpp
libzarafa_soapserver_la_LIBADD = ${GSOAP_LIBS}
libzarafa_soapserver_la_LDFLAGS = ${AM_LDFLAGS} \
	-Wl,--version-script=${top_builddir}/default.sym
EXTRA_libzarafa_soapserver_la_DEPENDENCIES = ${top_builddir}/default.sym

nodist_libzarafa_soapclient_la_SOURCES = soapC.cpp soapClient.cpp
libzarafa_soapclient_la_LIBADD = ${GSOAP_LIBS}
libzarafa_soapclient_la_LDFLAGS = ${AM_LDFLAGS} \
	-Wl,--version-script=${top_builddir}/default.sym
EXTRA_libzarafa_soapclient_la_DEPENDENCIES = ${top_builddir}/default.sym

SOAPHEADER = ${top_srcdir}/provider/include/ZarafaProto.h
SOAP_INC = ${top_srcdir}/provider/include/
SOAP_NS = ZarafaCmd.nsmap ZarafaCmd.wsdl

SOAP_H_FILES = soapH.h soapStub.h
SOAP_CPP_CLIENT = soapClient.cpp soapC.cpp
SOAP_LIB_FILES = soapClientLib.cpp soapServerLib.cpp soapZarafaCmdObject.h
SOAP_CPP_SRC=$(SOAP_CPP_CLIENT) soapServer.cpp $(SOAP_H_FILES) $(SOAP_NS) $(SOAP_LIB_FILES)

$(SOAP_CPP_SRC) : ns.xsd
ns.xsd: $(SOAPHEADER)
	$(GSOAP_SOAPCPP2) -x -I$(SOAP_INC) $<

#libsoapserver_a_LDADD=$(SOAP_CPP_LIB)
BUILT_SOURCES=ns.xsd $(SOAP_CPP_SRC)
CLEANFILES=$(SOAP_CPP_SRC) *.req.xml *.res.xml *.wsdl *.nsmap *.xsd *Proxy\.h *~

check-syntax:
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) \
		-fsyntax-only -fmessage-length=0 ${CHK_SOURCES} -Wall -Wformat=2

include ../../global.am
