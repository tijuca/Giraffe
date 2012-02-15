pythonexeclib_LTLIBRARIES = _MAPICore.la _inetmapi.la _icalmapi.la _licenseclient.la
noinst_LTLIBRARIES = _libcommon.la _archiver.la _libfreebusy.la _RecurrenceState.la

if ! OSS_ONLY
pythonexeclib_LTLIBRARIES += _licensereq.la
endif

# disable warnings since code is mostly generated
AM_CPPFLAGS = $(DEBUGFLAGS) -DLINUX -I$(top_srcdir)/mapi4linux/include -I$(top_srcdir)/provider/common -I${top_srcdir}/common $(PYTHON_INCLUDES) -I${top_srcdir}/swig -w -DSWIG_PYTHON_OUTPUT_TUPLE

_MAPICore_la_CXXFLAGS = -I${top_srcdir}/zarafa-libsync
_MAPICore_la_LDFLAGS = -module -avoid-version
_MAPICore_la_LIBADD = ${top_builddir}/mapi4linux/src/libmapi.la \
						${top_builddir}/common/libcommon_mapi.la \
						${top_builddir}/common/libcommon_util.la \
						${top_builddir}/zarafa-libsync/libzarafasync.la \
						$(PROG_LIBS) $(PYTHON_LIBS)

_inetmapi_la_CXXFLAGS = -I${top_srcdir}/inetmapi
_inetmapi_la_LDFLAGS = -module -avoid-version
_inetmapi_la_LIBADD = ${top_builddir}/mapi4linux/src/libmapi.la \
						${top_builddir}/common/libcommon_mapi.la \
						${top_builddir}/common/libcommon_util.la \
						${top_builddir}/inetmapi/libinetmapi.la \
						$(PROG_LIBS) $(PYTHON_LIBS)

_icalmapi_la_CXXFLAGS = -I${top_srcdir}/libicalmapi -I${top_srcdir}/libfreebusy
_icalmapi_la_LDFLAGS = -module -avoid-version
_icalmapi_la_LIBADD = ${top_builddir}/mapi4linux/src/libmapi.la \
						${top_builddir}/libicalmapi/libicalmapi.la \
						${top_builddir}/common/libcommon_mapi.la \
						${top_builddir}/common/libcommon_util.la \
						${top_builddir}/libfreebusy/libfreebusy.la \
						$(PROG_LIBS) $(PYTHON_LIBS)

_archiver_la_CXXFLAGS = -I${top_srcdir}/ECtools/zarafa-archiver
_archiver_la_LDFLAGS = -module -avoid-version -rpath '$(libdir)'
_archiver_la_LIBADD = ${top_builddir}/ECtools/zarafa-archiver/libarchiver.la \
						$(PROG_LIBS) $(PYTHON_LIBS)

_licenseclient_la_CXXFLAGS = -I${top_srcdir}/provider/common -I${top_builddir}/provider/soap -I${top_srcdir}/provider/soap
_licenseclient_la_LDFLAGS = -module -avoid-version
_licenseclient_la_LIBADD = ${top_builddir}/provider/common/libzarafacommon.la \
						${top_builddir}/provider/soap/libgsoap.la \
						${top_builddir}/provider/soap/libsoapclient.la \
						${top_builddir}/common/libcommon_mapi.la \
						${top_builddir}/common/libcommon_util.la \
						${top_builddir}/common/libcommon_ssl.la \
						${top_builddir}/mapi4linux/src/libmapi.la \
						$(PROG_LIBS) $(PYTHON_LIBS) $(SSL_LIBS)

if ! OSS_ONLY
_licensereq_la_CXXFLAGS = -I${top_srcdir}/provider/common -I${top_builddir}/provider/soap -I${top_srcdir}/provider/soap -I${top_srcdir}/liblicense
_licensereq_la_LDFLAGS = -module -avoid-version
_licensereq_la_LIBADD = ${top_builddir}/liblicense/liblicense.la \
						${top_builddir}/common/libcommon_mapi.la \
						${top_builddir}/common/libcommon_util.la \
						$(PROG_LIBS) $(PYTHON_LIBS) $(SSL_LIBS)
endif

# noinst_ and rpath will generate the _libcommon.so file
_libcommon_la_LDFLAGS = -module -avoid-version -rpath '$(libdir)'
_libcommon_la_LIBADD = ${top_builddir}/mapi4linux/src/libmapi.la \
					   ${top_builddir}/common/libcommon_mapi.la \
					   ${top_builddir}/common/libcommon_util.la \
				  	   $(PROG_LIBS) $(PYTHON_LIBS)

_RecurrenceState_la_LDFLAGS = -module -avoid-version -rpath '$(libdir)'
_RecurrenceState_la_LIBADD = ${top_builddir}/mapi4linux/src/libmapi.la \
	${top_builddir}/common/libcommon_mapi.la \
	${top_builddir}/common/libcommon_util.la \
	$(PROG_LIBS) $(PYTHON_LIBS)

_libfreebusy_la_CXXFLAGS = -I${top_srcdir}/libfreebusy
_libfreebusy_la_LDFLAGS = -module -avoid-version 
_libfreebusy_la_LIBADD = ${top_builddir}/mapi4linux/src/libmapi.la \
						 ${top_builddir}/libfreebusy/libfreebusy.la \
						 ${top_builddir}/common/libcommon_mapi.la \
                         ${top_builddir}/common/libcommon_util.la \
                         $(PROG_LIBS) $(PYTHON_LIBS)


BUILT_SOURCES = mapi_wrap.cxx \
				mapi_wrap.h \
				inetmapi_wrap.cxx \
				icalmapi_wrap.cxx \
				archiver_wrap.cxx \
				libcommon_wrap.cxx \
				licenseclient_wrap.cxx \
				libfreebusy_wrap.cxx \
				RecurrenceState_wrap.cxx

if ! OSS_ONLY
BUILT_SOURCES += licensereq_wrap.cxx
endif

CLEANFILES = $(BUILT_SOURCES) \
			 MAPICore.py \
			 inetmapi.py \
			 icalmapi.py \
			 archiver.py \
			 libcommon.py \
			 licenseclient.py \
			 libfreebusy.py

if ! OSS_ONLY
CLEANFILES += licensereq.py
endif

# wait for full output of swig
MAPICore.py: $(BUILT_SOURCES)
inetmapi.py: $(BUILT_SOURCES)
icalmapi.py: $(BUILT_SOURCES)
archiver.py: $(BUILT_SOURCES)
libcommon.py: $(BUILT_SOURCES)
licenseclient.py: $(BUILT_SOURCES)
libfreebusy.py: $(BUILT_SOURCES)
RecurrenceState.py: $(BUILT_SOURCES)

if ! OSS_ONLY
licensereq.py: $(BUILT_SOURCES)
endif

_MAPICore_la_SOURCES = conversion.h conversion.cpp mapi_wrap.cxx mapi_wrap.h scl.h ../director_util.h ../director_util.cpp
_inetmapi_la_SOURCES = conversion.h conversion.cpp inetmapi_wrap.cxx 
_icalmapi_la_SOURCES = conversion.h conversion.cpp icalmapi_wrap.cxx
_archiver_la_SOURCES = archiver_wrap.cxx archiver_conv.h archiver_conv.cpp
_libcommon_la_SOURCES = conversion.h conversion.cpp libcommon_wrap.cxx
_RecurrenceState_la_SOURCES = conversion.h conversion.cpp RecurrenceState_wrap.cxx
_licenseclient_la_SOURCES = licenseclient_wrap.cxx licenseclient_conv.cpp licenseclient_conv.h
_libfreebusy_la_SOURCES = conversion.h conversion.cpp libfreebusy_wrap.cxx libfreebusy_conv.h libfreebusy_conv.cpp

if ! OSS_ONLY
_licensereq_la_SOURCES = licensereq_wrap.cxx
endif

EXTRA_DIST = typemap_python.i helpers_python.i setup.py $(BUILT_SOURCES) MAPICore.py inetmapi.py icalmapi.py licenseclient.py MAPI/Defs.py  MAPI/__init__.py  MAPI/Struct.py  MAPI/Tags.py  MAPI/Time.py MAPI/Util/__init__.py MAPI/Util/Generators.py MAPI/Util/Iterators.py MAPI/Util/AddressBook.py archiver_python.i
datadir = $(pythonexeclibdir)
data_DATA = MAPICore.py inetmapi.py icalmapi.py licenseclient.py
noinst_datadir = libcommon.py RecurrenceState.py

if ! OSS_ONLY
EXTRA_DIST += licensereq.py
data_DATA += licensereq.py
endif

mapi_wrap.h: mapi_wrap.cxx
mapi_wrap.cxx: ../mapi.i ../mapiutil.i ../mapidefs.i ../mapicode.i ../mapix.i ../mapinotifsink.i ../mapiutil.i ../typemap.i ../helpers.i ../edkmdb.i ../IECServiceAdmin.i ../ecdefs.i typemap_python.i helpers_python.i
	if test "x$(SWIG_EXEC)" = "x"; then echo "swig is required for 'make dist'"; exit 1; fi
	$(SWIG_EXEC) -I${top_srcdir}/swig -I${top_srcdir}/zarafa-libsync $(SWIG_FLAGS) -Wall -python -c++ -threads -o ${top_builddir}/swig/python/mapi_wrap.cxx ${top_srcdir}/swig/mapi.i

inetmapi_wrap.cxx: ../typemap.i ../inetmapi.i typemap_python.i helpers_python.i
	$(SWIG_EXEC) -I${top_srcdir}/swig -Wall -python -c++ -threads -o ${top_builddir}/swig/python/inetmapi_wrap.cxx ${top_srcdir}/swig/inetmapi.i

icalmapi_wrap.cxx: ../typemap.i ../icalmapi.i typemap_python.i helpers_python.i
	$(SWIG_EXEC) -I${top_srcdir}/libicalmapi -I${top_srcdir}/libfreebusy -I${top_srcdir/swig} -Wall -python -c++ -threads -o ${top_builddir}/swig/python/icalmapi_wrap.cxx ${top_srcdir}/swig/icalmapi.i

archiver_wrap.cxx: ../archiver.i archiver_python.i
	$(SWIG_EXEC) -I${top_srcdir}/ECtools/zarafa-archiver -Wall -python -c++ -threads -o ${top_builddir}/swig/python/archiver_wrap.cxx ${top_srcdir}/swig/archiver.i

libcommon_wrap.cxx: ../libcommon.i
	$(SWIG_EXEC) -I${top_srcdir}/common -I${top_srcdir/swig} -Wall -python -c++ -threads -o ${top_builddir}/swig/python/libcommon_wrap.cxx ${top_srcdir}/swig/libcommon.i

RecurrenceState_wrap.cxx: ../RecurrenceState.i
	$(SWIG_EXEC) -I${top_srcdir}/common -I${top_srcdir/swig} -Wall -python -c++ -threads -o ${top_builddir}/swig/python/RecurrenceState_wrap.cxx ${top_srcdir}/swig/RecurrenceState.i

licenseclient_wrap.cxx: ../licenseclient.i
	$(SWIG_EXEC) -I${top_srcdir}/provider/common -I${top_srcdir}/common -I${top_srcdir/swig} -Wall -python -c++ -threads -o ${top_builddir}/swig/python/licenseclient_wrap.cxx ${top_srcdir}/swig/licenseclient.i

# make dist seems to want to create this file regardless of the OSS_ONLY flag.
licensereq_wrap.cxx: ../licensereq.i
	$(SWIG_EXEC) -I${top_srcdir}/liblicense -I${top_srcdir/swig} -Wall -python -c++ -threads -o ${top_builddir}/swig/python/licensereq_wrap.cxx ${top_srcdir}/swig/licensereq.i

libfreebusy_wrap.cxx: ../libfreebusy.i
	$(SWIG_EXEC) -I${top_srcdir}/libfreebusy -I${top_srcdir/swig}  -Wall -python -c++ -threads -o ${top_builddir}/swig/python/libfreebusy_wrap.cxx ${top_srcdir}/swig/libfreebusy.i

check-syntax:
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) \
		-fsyntax-only -fmessage-length=0 ${CHK_SOURCES} -Wall -Wformat=2

.NOTPARALLEL: install-exec-local
install-exec-local:
	test -n "$(DESTDIR)" && pyroot="--root=$(DESTDIR)"; \
	test "$(DISTRO)" = "suse" -o "$(DISTRO)" = "sles" && pyrpm="--record-rpm=INSTALLED_FILES"; \
	cd ${top_srcdir}/swig/python/ && python setup.py install $${pyroot} --prefix="$(prefix)" --install-purelib $(pythonexeclibdir) $${pyrpm}
