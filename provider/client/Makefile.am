# -*- Makefile -*-

pkglib_LTLIBRARIES = libzarafaclient.la
noinst_DATA = libzarafaclient.ldd

sysconf_configdir = ${MAPICONFIGDIR}
dist_sysconf_config_DATA = zarafa.inf

sampleconfdir = ${docdir}/example-config
sampleconf_DATA = zarafa.inf

AM_CPPFLAGS = ${ZCPPFLAGS} $(DEBUGFLAGS) $(BOOST_CPPFLAGS)	\
	$(GSOAP_CFLAGS) $(SSL_CFLAGS)				\
	-I${top_srcdir}/provider/include			\
	-I${top_srcdir}/provider/common				\
	-I${top_builddir}/provider/soap				\
	-I${top_srcdir}/provider/soap				\
	-I${top_srcdir}/mapi4linux/include			\
	-I${top_srcdir}/libfreebusy					\
	-I${top_srcdir}/common -I${top_srcdir}/m4lcommon \
	-I${top_srcdir}/zarafa-libsync

libzarafaclient_la_LDFLAGS = ${AM_LDFLAGS} -avoid-version -module
libzarafaclient_la_LIBADD = $(SSL_LIBS)	\
    ${GSOAP_LIBS} \
	${top_builddir}/provider/soap/libzarafa_soapclient.la \
	${top_builddir}/provider/common/libzarafacommon.la	\
	${top_builddir}/common/libzcp_common_util.la \
	${top_builddir}/common/libzcp_common_mapi.la \
	${top_builddir}/common/libzcp_common_ssl.la \
	${top_builddir}/libfreebusy/libfreebusy.la			\
	${top_builddir}/mapi4linux/src/libmapi.la			\
	${top_builddir}/zarafa-libsync/libzarafasync.la		\
	-lstdc++ $(UUID_LIBS) $(ICU_LIBS)

libzarafaclient_la_SOURCES = ECABLogon.cpp ECABProvider.cpp											\
	ECAttach.cpp ECMAPIContainer.cpp ECMAPIFolder.cpp ECMAPIProp.cpp								\
	ECMAPITable.cpp ECMessage.cpp ECMsgStore.cpp													\
	ECMSProvider.cpp ECNamedProp.cpp ECNotifyClient.cpp ECNotifyMaster.cpp							\
	SessionGroupData.cpp ECSessionGroupManager.cpp ECXPLogon.cpp ECXPProvider.cpp epclient.cpp \
	Mem.cpp WSTransport.cpp																			\
	ECPropertyEntry.cpp ECGenericProp.cpp ECParentStorage.cpp										\
	ECABProp.cpp ECABContainer.cpp ECDistList.cpp ECMailUser.cpp									\
	WSMAPIPropStorage.cpp WSMAPIFolderOps.cpp WSTableView.cpp										\
	WSABPropStorage.cpp WSUtil.cpp											\
	WSABTableView.cpp WSStoreTableView.cpp															\
	DLLGlobal.h ECABLogon.h ECABProvider.h ECAttach.h ECMAPIContainer.h								\
	ECMAPIFolder.h ECMAPIProp.h ECMAPITable.h ECMessage.h											\
	ECMsgStore.h ECMSProvider.h ECNamedProp.h ECNotifyClient.h SessionGroupData.h ECSessionGroupManager.h	\
	ECXPLogon.h ECXPProvider.h EntryPoint.h Mem.h resource.h WSTransport.h					\
	IECPropStorage.h ECPropertyEntry.h ECGenericProp.h ECParentStorage.h							\
	ECABProp.h ECABContainer.h ECDistList.h ECMailUser.h											\
	WSMAPIPropStorage.h WSMAPIFolderOps.h WSTableView.h												\
	WSABPropStorage.h ECNotifyMaster.h WSUtil.h									\
	WSABTableView.h WSStoreTableView.h																\
	ClientUtil.h ClientUtil.cpp																		\
	ECExchangeModifyTable.cpp ECExchangeModifyTable.h												\
	ECExchangeExportChanges.cpp ECExchangeExportChanges.h											\
	ECExchangeImportContentsChanges.cpp ECExchangeImportContentsChanges.h							\
	ECExchangeImportHierarchyChanges.cpp ECExchangeImportHierarchyChanges.h							\
	ECICS.h 																		\
	ECMSProviderSwitch.cpp ECMSProviderSwitch.h	ECMSProviderOffline.h ECMSProviderOffline.cpp		\
	ECABProviderSwitch.cpp ECABProviderSwitch.h	ECABProviderOffline.h ECABProviderOffline.cpp		\
	ProviderUtil.h ProviderUtil.cpp																	\
	ECExportAddressbookChanges.h ECExportAddressbookChanges.cpp										\
	ECMAPIFolderPublic.h ECMAPIFolderPublic.cpp														\
	ECMemTablePublic.h ECMemTablePublic.cpp															\
	ECMsgStorePublic.h ECMsgStorePublic.cpp															\
	ECChangeAdvisor.h ECChangeAdvisor.cpp											\
	ECDisplayTable.h ECDisplayTable.cpp	\
	ECArchiveAwareMsgStore.h ECArchiveAwareMsgStore.cpp	\
	ECArchiveAwareMessage.h ECArchiveAwareMessage.cpp	\
	ECArchiveAwareAttach.h ECArchiveAwareAttach.cpp	\
	WSMessageStreamExporter.h WSMessageStreamExporter.cpp \
	WSSerializedMessage.h WSSerializedMessage.cpp \
	WSMessageStreamImporter.h WSMessageStreamImporter.cpp \
	ECMessageStreamImporterIStreamAdapter.h ECMessageStreamImporterIStreamAdapter.cpp

check-syntax:
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) \
		-fsyntax-only -fmessage-length=0 ${CHK_SOURCES} -Wall -Wformat=2

include ../../global.am
