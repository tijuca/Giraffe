lib_LTLIBRARIES = libcommon_util.la libcommon_mapi.la libcommon_ssl.la libcommon_service.la

# keep subdirs for these headers
charset_HEADERS = charset/convert.h charset/traits.h charset/utf32string.h charset/utf8string.h charset/utf16string.h charset/convstring.h charset/localeutil.h
charsetdir = $(includedir)/zarafa/charset
utf8_HEADERS = utf8/checked.h utf8/core.h utf8/unchecked.h
utf8dir = $(includedir)/zarafa/utf8
ustringutil_HEADERS = ustringutil/utfutil.h
ustringutildir = $(includedir)/zarafa/ustringutil
mapiptr_HEADERS = mapi_ptr/mapi_array_ptr.h mapi_ptr/mapi_memory_ptr.h mapi_ptr/mapi_object_ptr.h mapi_ptr/mapi_rowset_ptr.h
mapiptrdir = $(includedir)/zarafa/mapi_ptr

libcommon_mapi_la_include_HEADERS = rtfutil.h mapiext.h ECGuid.h ECTags.h ECDefs.h IECSecurity.h IECServiceAdmin.h IECSpooler.h		\
				  IECUnknown.h IECExportChanges.h IECExportAddressbookChanges.h IECStatsCollector.h IECImportAddressbookChanges.h	\
				  ECUnknown.h ECMemStream.h ECMemTable.h ECKeyTable.h Util.h ECDebug.h namedprops.h mapiguidext.h edkguid.h			\
				  IMAPIOffline.h IECMultiStoreTable.h restrictionutil.h favoritesutil.h ZarafaCode.h platform.linux.h md5.h			\
				  base64.h ECLogger.h ECConfig.h ECConfigImpl.h EMSAbTag.h my_getopt.h codepage.h ECScheduler.h stringutil.h		\
				  Trace.h UnixUtil.h ECIConv.h ustringutil.h SymmetricCrypt.h SSLUtil.h ECChannel.h platform.h CommonUtil.h			\
				  mapicontact.h IECLicense.h ECABEntryID.h RecurrenceState.h ECFifoBuffer.h ECSerializer.h IECSingleInstance.h		\
				  IECChangeAdvisor.h IECChangeAdviseSink.h IECImportContentsChanges.h IECTestProtocol.h auto_free.h ECCache.h		\
				  TimeUtil.h ECGetText.h ECInvariantChecker.h tstring.h mapi_ptr.h utf8.h ECRestriction.h threadutil.h				\
				  HtmlToTextParser.h HtmlEntity.h userutil.h ECFeatures.h ECFeatureList.h ECDebugPrint.h ECInterfaceDefs.h			\
				  HrException.h ECIterators.h ECACL.h pthreadutil.h ECThreadPool.h MAPINotifSink.h \
				  ECDatabaseMySQL.h fileutil.h IStreamAdapter.h IECExchangeModifyTable.h \
				  ConsoleTable.h MAPIConsoleTable.h ECFifoStream.h ECMAPIDebug.h boost_compat.h \
				  $(charset_HEADERS) $(utf8_HEADERS) $(ustringutil_HEADERS) $(mapiptr_HEADERS)

libcommon_mapi_la_includedir = $(includedir)/zarafa

data_DATA = zarafa.pc
datadir = $(libdir)/pkgconfig

AM_CPPFLAGS = $(DEBUGFLAGS) $(BOOST_CPPFLAGS) -DLINUX \
	-I${top_srcdir}/mapi4linux/include -I${top_srcdir}/provider/common \
	-I${top_srcdir}/libfreebusy $(MYSQL_INCLUDES)

libcommon_mapi_la_LDFLAGS = -static
libcommon_mapi_la_SOURCES = CommonUtil.cpp				\
			rtfutil.cpp									\
			codepage.cpp								\
			ECGuid.cpp									\
			ECUnknown.cpp								\
			ECMemStream.cpp								\
			ECFifoStream.cpp							\
			ECMemTable.cpp								\
			ECKeyTable.cpp								\
			Util.cpp 									\
			ECDebug.cpp									\
			favoritesutil.cpp							\
			ZarafaCode.cpp								\
			HtmlToTextParser.cpp						\
			HtmlEntity.cpp								\
			ECABEntryID.cpp								\
			ECDebugPrint.cpp							\
			ECIterators.cpp								\
			MAPINotifSink.cpp							\
			IStreamAdapter.cpp							\
			MAPIConsoleTable.cpp						\
			ECMAPIDebug.cpp

libcommon_util_la_LDFLAGS = -static
libcommon_util_la_SOURCES = platform.cpp			\
			platform.linux.cpp						\
			md5.cpp									\
			base64.cpp								\
			ECLogger.cpp							\
			ECConfig.cpp							\
			ECConfigImpl.cpp						\
			ECFifoBuffer.cpp						\
			ECSerializer.cpp						\
			ECCache.cpp								\
			my_getopt.cpp							\
			ECScheduler.cpp							\
			stringutil.cpp							\
			Trace.cpp								\
			UnixUtil.cpp							\
			ECIConv.cpp								\
			ustringutil.cpp							\
			SymmetricCrypt.cpp						\
			charset/convert.cpp						\
			charset/convstring.cpp					\
			charset/traits.cpp						\
			charset/localeutil.cpp					\
			TimeUtil.cpp							\
			RecurrenceState.cpp						\
			ECGetText.cpp							\
			threadutil.cpp							\
			ECRestriction.cpp						\
			userutil.cpp							\
			ECFeatures.cpp							\
			ECACL.cpp								\
			fileutil.cpp							\
			ECThreadPool.cpp						\
			ConsoleTable.cpp

if HAVE_ICU
libcommon_util_la_SOURCES += ustringutil/utfutil.cpp
endif

libcommon_ssl_la_LDFLAGS = -static
libcommon_ssl_la_SOURCES = 							\
			SSLUtil.cpp								\
			ECChannel.cpp

libcommon_service_la_LDFLAGS = -static
libcommon_service_la_SOURCES = ECDatabaseMySQL.cpp

# remove double installed headers
install-data-hook:
	for f in $(charset_HEADERS) $(utf8_HEADERS) $(ustringutil_HEADERS) $(mapiptr_HEADERS); do rm ${DESTDIR}/$(includedir)/zarafa/`basename $$f`; done

check-syntax:
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) \
		-fsyntax-only -fmessage-length=0 ${CHK_SOURCES} -Wall -Wformat=2
