AC_PREREQ(2.60)
m4_define([zarafa_version], esyscmd(cat version.in | tr -d '\n'))
AC_INIT([Zarafa], [zarafa_version], [zarafa-dev@zarafa.com])
AC_CONFIG_SRCDIR([provider/server/ECServer.cpp])
AC_CONFIG_HEADERS([common/config.h])
AC_CONFIG_AUX_DIR([autoconf])
AC_CONFIG_MACRO_DIR([autoconf])
# because webaccess has too deep dirs for default tar-v7, we need something "better"
AM_INIT_AUTOMAKE([1.10 foreign tar-ustar])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# used in the pkgconfig file
ZARAFA_VERSION=zarafa_version
AC_SUBST(ZARAFA_VERSION)

ECCLIENT_LIBRARY_VERSION=1:0:0
AC_SUBST(ECCLIENT_LIBRARY_VERSION)

# Checks for programs.
AC_DISABLE_STATIC	dnl must be before ac_prog_libtool
AC_PROG_LIBTOOL
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_LEX
AC_PROG_YACC
AC_PATH_PROG(PROG_XMLTO, xmlto)	# not an error if not found, just no manual pages
AC_PATH_PROG(PROG_ANT, ant)		# not an error if not found, no 'make dist' (waxt package)
AM_PROG_CC_C_O

# Lex / Yacc version
if test "x$LEX" = "x" -o "x$LEX" = "x:"; then
    AC_MSG_ERROR([Lexer not found. Please install flex])
elif test "x$LEX" = "xflex"; then
   LEX_DEFINE=-DWITH_FLEX
   LEX_FLAGS="-l"
else
   LEX_DEFINE=-DWITH_LEX
   LEX_FLAGS=
fi
AC_SUBST(LEX_DEFINE)
AC_SUBST(LEX_FLAGS)

if test "x$YACC" = "x" -o "x$YACC" = "x:"; then
    AC_MSG_ERROR([Yacc not found. Please install bison])
elif test "$YACC" = "bison -y"; then
   BISON_DEFINE=-DWITH_BISON
   YACC_LIB=
else
   BISON_DEFINE=-DWITH_YACC
   YACC_LIB="-ly"
fi
AC_SUBST(BISON_DEFINE)
AC_SUBST(YACC_LIB)


#
# construct default CXXFLAGS
#

# Everything is C++
AC_LANG_PUSH(C++)
# save clean settings
CXXFLAGS_system=$CXXFLAGS
CPPFLAGS_system=$CPPFLAGS
LDFLAGS_system=$LDFLAGS
LIBS_system=$LIBS

# check build OS
BUILDOS=""
case $host in
	*-*-openbsd*)
		BUILDOS="-DOPENBSD"
		;;
	*-*-linux*)
		;;
esac

# default to debug mode
# disable write-strings: too much complains about "" being 'cast' to char* (since const char* is not in the declarations) on g++-4.2 and higher
ZCXXFLAGS="$ZCXXFLAGS -g -rdynamic -Wall -Wno-write-strings $BUILDOS -DHAVE_CONFIG_H"
if ! test "$want_release" = "yes"; then
# when using -pedantic, -Wno-long-long is required
# only add -pedantic in non-release mode, since a header in php5 contains a , too much in an enum, which triggers a compile error.
  ZCXXFLAGS="$ZCXXFLAGS -pedantic -Wno-long-long"
fi

# programs that use files >2gb need 64bit file support
LFS_CFLAGS=`getconf LFS_CFLAGS`
ZCXXFLAGS="$ZCXXFLAGS $LFS_CFLAGS"

# disable variadic-macros: too much warning: anonymous variadic macros were introduced in C99, only on new compilers
AC_MSG_CHECKING([whether C++ compiler understands -Wno-variadic-macros option])
if ( echo 'int main() {}' | $CXX -Wno-variadic-macros -E - >/dev/null 2>&1 ); then
   AC_MSG_RESULT([yes])
   ZCXXFLAGS="$ZCXXFLAGS -Wno-variadic-macros"
else
   AC_MSG_RESULT([no])
fi

# Everything uses pthread
# check lib, but do not set in link list, but use -pthread instead (gnu c/c++ only?)
AC_CHECK_LIB([pthread], [pthread_create], [ZCXXFLAGS="$ZCXXFLAGS -pthread"], AC_MSG_ERROR([required library libpthread missing or unusable]))

#
# enable/disable options
#

AC_ARG_ENABLE(release, AC_HELP_STRING([--enable-release],[enable release mode (compile flags -O2 and no DEBUG defines)]),
  [want_release=${enableval}],[want_release=no])
if test "$want_release" = "yes"; then
    ZCXXFLAGS="$ZCXXFLAGS -O2 -fno-strict-aliasing -DNDEBUG -DFORCE_CHARSET_CONVERSION"
else
# -fPIC must be present on x86_64 when -O2 isn't used
    ZCXXFLAGS="$ZCXXFLAGS -fPIC"
fi

AC_ARG_ENABLE(unicode, AC_HELP_STRING([--enable-unicode],[enable unicode mode (compile flags -DUNICODE)]),
  [want_unicode=${enableval}],[want_unicode=yes])
if test "$want_unicode" = "yes"; then
    ZCXXFLAGS="$ZCXXFLAGS -DUNICODE"
fi

AC_ARG_ENABLE(tcmalloc, AC_HELP_STRING([--enable-tcmalloc],[enable use of tcmalloc lib]),
  [want_tcmalloc=${enableval}],[want_tcmalloc=no])

AC_ARG_ENABLE(sparsehash, AC_HELP_STRING([--enable-sparsehash],[enable use of sparsehash]),
  [want_sparsehash=${enableval}],[want_sparsehash=no])

AC_MSG_CHECKING([specified Linux distribution name])
AC_ARG_WITH(distro, AC_HELP_STRING([--with-distro],[set distribution name, used for sysconfig and initscripts only. available: rhel, alt, suse or sles]),
  [with_distro=${withval}],[with_distro=rhel])
if test "$with_distro" != "rhel" -a "$with_distro" != "suse" -a "$with_distro" != "sles" -a "$with_distro" != "alt" -a "$with_distro" != "cbs4"; then
   AC_MSG_RESULT([unknown distro, using rhel instead])
   with_distro=rhel
else
   AC_MSG_RESULT([using $with_distro])
fi
DISTRO=$with_distro
AC_SUBST(DISTRO)

AC_ARG_ENABLE(oss, AC_HELP_STRING([--enable-oss],[enable opensource mode]),
  [want_oss=${enableval}],[want_oss=no])
AC_MSG_CHECKING([non-oss distro files])
if test "$want_oss" = "no" -a ! -d `dirname $0`/licensed; then
   AC_MSG_RESULT([not found])
   want_oss="yes"
else
   AC_MSG_RESULT([ok])
fi
AM_CONDITIONAL([OSS_ONLY], [test "$want_oss" = yes])

AC_ARG_ENABLE(debug, AC_HELP_STRING([--enable-debug],[enable debug mode (Adds debug code)]),
  [want_debug=${enableval}],[want_debug=no])
if test "$want_debug" = "yes"; then
    DEBUGFLAGS="-DDEBUG"
    DEBUG=1
else
    DEBUGFLAGS=
    DEBUG=0
fi
AC_SUBST(DEBUGFLAGS)

# profiling
AC_ARG_ENABLE(profiling,
  AC_HELP_STRING([--enable-profiling], [enable profiling (-pg)]),
    [ if test "${enableval}" = "yes"; then
		ZCXXFLAGS="$ZCXXFLAGS -pg"
		ZLDFLAGS="$LDFLAGS -pg"
	  fi
    ])

# test offline possibility
AC_DEFUN([OFFLINE_CONFIG],[
  AC_ARG_ENABLE(offline, AC_HELP_STRING([--enable-offline],[Compile zarafa-server as the offline server]),
  [
  	ZCXXFLAGS="$ZCXXFLAGS -DHAVE_OFFLINE_SUPPORT"
  ])
])
if ! test "$want_oss" = "yes"; then
   OFFLINE_CONFIG
fi

# MAPI config files
AC_ARG_WITH(mapi-configdir,
  AC_HELP_STRING([--with-mapi-configdir=PATH],[path to install the libmapi config (provider) files (default /etc/mapi)]),
  [MAPICONFIGDIR=${withval}],[MAPICONFIGDIR=/etc/mapi])
AC_SUBST(MAPICONFIGDIR)

# user create scripts location
AC_ARG_WITH(userscript-prefix,
  AC_HELP_STRING([--with-userscript-prefix=PATH],[path to install the userscripts in (default /etc/zarafa/userscripts)]),
  [USERSCRIPTDIR=${withval}],[USERSCRIPTDIR=/etc/zarafa/userscripts])
AC_SUBST(USERSCRIPTDIR)

# quota warning templates location
AC_ARG_WITH(quotatemplate-prefix,
  AC_HELP_STRING([--with-quotatemplate-prefix=PATH],[path to install the quota warning templates in (default /etc/zarafa/quotamail)]),
  [QUOTATEMPLATEDIR=${withval}],[QUOTATEMPLATEDIR=/etc/zarafa/quotamail])
AC_SUBST(QUOTATEMPLATEDIR)

# search scripts location
AC_ARG_WITH(searchscripts-prefix,
  AC_HELP_STRING([--with-searchscripts-prefix=PATH],[path to install the search scripts in (default /etc/zarafa/searchscripts)]),
  [SEARCHSCRIPTSDIR=${withval}],[SEARCHSCRIPTSDIR=/etc/zarafa/searchscripts])
AC_SUBST(SEARCHSCRIPTSDIR)

#
# find required headers and functions
#

AC_HEADER_STDC

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_C_RESTRICT
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

# zarafa-stats requires ncurses(w) for --top
# check wide version before non-wide version
AC_DEFUN([LIBNCURSES_WITH_CONFIG],[
  AC_ARG_WITH(ncurses-config, AC_HELP_STRING([--with-ncurses-config=PATH],[path to the ncurses(w)5-config script, wide version is preferred]),
    [NCURSES_CONFIG=${withval}],[NCURSES_CONFIG=ncursesw5-config])

  AC_MSG_CHECKING([for ncurses-config presence])
  NCURSES_CONFIG=`which $NCURSES_CONFIG 2>/dev/null`
  if ! test -x "$NCURSES_CONFIG"; then
    # test non-wide version after wide (or forced version)
    NCURSES_CONFIG=`which ncurses5-config 2>/dev/null`
  fi
  if ! test -x "$NCURSES_CONFIG"; then
    AC_MSG_RESULT([not found])
	# does not indicate if ncurses is available
    NCURSES_FLAGS=
    NCURSES_LIBS=-lncursesw
  else
    AC_MSG_RESULT([$NCURSES_CONFIG])

    NCURSES_FLAGS=`$NCURSES_CONFIG --cflags`
    NCURSES_LIBS=`$NCURSES_CONFIG --libs`
  fi

  CXXFLAGS="$CXXFLAGS $NCURSES_FLAGS"
  LIBS=$NCURSES_LIBS

  AC_CHECK_FUNCS([initscr], [AC_DEFINE([HAVE_NCURSES], [1], [ncurses library available])], [
  # possibly wide version failed, force check normal version
    NCURSES_LIBS=-lncurses
    LIBS=$NCURSES_LIBS
	AC_CHECK_FUNCS([cbreak], [AC_DEFINE([HAVE_NCURSES], [1], [ncurses library available])])
  ])

  LIBS=$libs_keep
  CFLAGS=$cflags_keep
  AC_SUBST(NCURSES_FLAGS)
  AC_SUBST(NCURSES_LIBS)
])
LIBNCURSES_WITH_CONFIG

AC_CHECK_HEADERS([memory.h stddef.h stdlib.h string.h sys/timeb.h wchar.h syslog.h sys/un.h sys/ucred.h sys/stat.h openssl/ssl.h zlib.h])

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_MKTIME
AC_FUNC_REALLOC
AC_FUNC_STRFTIME
AC_FUNC_STRTOD
AC_FUNC_VPRINTF
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([random gettimeofday ftime localtime_r timegm gmtime gmtime_r memchr memset select socket strchr strerror strrchr strstr strtol strtoul strtoll strtoull sscanf strtof_l strtod_l wctomb mbtowc dcgettext getpeereid gethostbyname_r getpwnam_r vsyslog snprintf sprintf_l vsnprintf_l])


#
# find required/optional libraries
#

# checks for swig (optional if files are already generated in make dist) and python
SWIG_CHECK_SWIG
SWIG_CHECK_PYTHON
if test "$want_unicode" = "yes"; then
   SWIG_FLAGS="-DUNICODE"
fi
AC_SUBST(SWIG_FLAGS)

# Checks for PHP through php-config and tests the truth of php-config results
PHP_WITH_PHP_CONFIG
PHP_CHECK_INCLUDES

# Checks for libxml2
AC_DEFUN([LIBXML_WITH_XML2_CONFIG],[
  AC_ARG_WITH(xml2-config, AC_HELP_STRING([--with-xml2-config=PATH],[path to the xml2-config script of libxml2]),
    [XML2_CONFIG=${withval}],[XML2_CONFIG=xml2-config])

  AC_MSG_CHECKING([for xml2-config presence])
  XML2_CONFIG=`which $XML2_CONFIG 2>/dev/null`
  if ! test -x "$XML2_CONFIG"; then
    AC_MSG_ERROR([cannot execute $XML2_CONFIG])
  else
    AC_MSG_RESULT([$XML2_CONFIG])

    XML2_CFLAGS=`$XML2_CONFIG --cflags`
    XML2_LIBS=`$XML2_CONFIG --libs`

    AC_SUBST(XML2_CFLAGS)
    AC_SUBST(XML2_LIBS)

    dnl check correctness of lib
    CXXFLAGS="$CXXFLAGS $XML2_CFLAGS"
    LIBS=$XML2_LIBS
    AC_MSG_CHECKING([acquired XML2 settings])
    AC_LINK_IFELSE([
      AC_LANG_SOURCE([
#include <libxml/xmlwriter.h>
int main() {
   	xmlNewTextWriterMemory(NULL, 0);
}
      ]) ], [ AC_MSG_RESULT([ok]) ], [ AC_MSG_ERROR([broken])
    ])
    LIBS=$LIBS_system
    CXXFLAGS=$CXXFLAGS_system
  fi
])
LIBXML_WITH_XML2_CONFIG
AM_CONDITIONAL([WITH_XML2], [test "$XML2_CONFIG" != "" -a -x "`which $XML2_CONFIG 2>/dev/null`"])

# Checks for MySQL 4.1 or better
AC_DEFUN([MYSQL_WITH_MYSQL_CONFIG],[
  AC_ARG_WITH(mysql-config, AC_HELP_STRING([--with-mysql-config=PATH], [path to the mysql-config script of mysql 4.1]),
    [MYSQL_CONFIG=${withval}],[MYSQL_CONFIG=mysql_config])

  AC_ARG_ENABLE(embedded-mysql, AC_HELP_STRING([--enable-embedded-mysql],[Compile zarafa-server with the embedded MySQL server]),
  [
	EMBEDDED=1
	CXXFLAGS="$CXXFLAGS -DEMBEDDED_MYSQL"
  ],[
	EMBEDDED=0
  ])

  MYSQL_INCLUDES=`$MYSQL_CONFIG --include`
  if test "$EMBEDDED" -eq 1; then
    MYSQL_LIBS=`$MYSQL_CONFIG --libmysqld-libs`
  else
    MYSQL_LIBS=`$MYSQL_CONFIG --libs`
  fi
  
  MYSQL_VERSION=`$MYSQL_CONFIG --version`

  if test -z "$MYSQL_VERSION"; then
    AC_MSG_ERROR(Cannot find mysql_config. Please use --with-mysql-config=PATH)
  fi

  AC_SUBST(MYSQL_INCLUDES)
  AC_SUBST(MYSQL_LIBS)
  AC_SUBST(MYSQL_VERSION)

  AC_MSG_CHECKING([for MySQL includes])
  AC_MSG_RESULT($MYSQL_INCLUDES)
  AC_MSG_CHECKING([for MySQL libs])
  AC_MSG_RESULT($MYSQL_LIBS)
])
MYSQL_WITH_MYSQL_CONFIG

# test for precense of mysql 5.0.7 function
CXXFLAGS="$CXXFLAGS $MYSQL_INCLUDES"
LIBS="$LIBS $MYSQL_LIBS"
AC_CHECK_FUNCS([mysql_set_character_set])
LIBS=$LIBS_system
CXXFLAGS=$CXXFLAGS_system

GSOAP_CFLAGS="-DWITH_OPENSSL -DWITH_GZIP"
AC_SUBST(GSOAP_CFLAGS)
# fixme first libz test
GSOAP_LIBS="-lz"
AC_SUBST(GSOAP_LIBS)

# check user given vmime location
AC_ARG_WITH(vmime-prefix,
  AC_HELP_STRING([--with-vmime-prefix=PATH],[path to the vmime include files, e.g. /usr/local/include]),
  [
  VMIME_PREFIX="$withval"
  CXXFLAGS="$CXXFLAGS -I$VMIME_PREFIX"
  CPPFLAGS="$CPPFLAGS -I$VMIME_PREFIX"
  VMIME_LIBS="-L$VMIME_PREFIX -R$VMIME_PREFIX "
  ],
  [
  VMIME_PREFIX="/usr/include"
  ])

# Checks for libvmime
AC_CHECK_HEADERS([vmime/utility/smartPtrInt.hpp],
  [
	dnl assume defaults
	VMIME_CFLAGS=-I$VMIME_PREFIX
	VMIME_LIBS="$VMIME_LIBS -lvmime"
  ],[
    dnl	header not found on system, try with pkgconfig (watch the [] quotes)
	PKG_CHECK_MODULES(VMIME, vmime = 0.9.0)
  ])
AC_SUBST(VMIME_CFLAGS)
AC_SUBST(VMIME_LIBS)
CXXFLAGS=$CXXFLAGS_system
CPPFLAGS=$CPPFLAGS_system

PKG_CHECK_MODULES([KYOTOCABINET], [kyotocabinet])
AC_SUBST(KYOTOCABINET_CFLAGS)
AC_SUBST(KYOTOCABINET_LIBS)

dnl TODO: add checks for library on system
AC_ARG_WITH(ical-prefix,
  AC_HELP_STRING([--with-ical-prefix=PATH],[path to the libical include files, e.g. /usr/local/include]),
  [ICAL_PREFIX=${withval}],[ICAL_PREFIX=/usr/include])

if test "x$ICAL_PREFIX" != "x"; then
   CXXFLAGS="$CXXFLAGS -I$ICAL_PREFIX"
fi
AC_CHECK_HEADERS([libical/ical.h],
  [ ICAL_CFLAGS=-I$ICAL_PREFIX
	ICAL_LIBS="-lical -licalss" ],
  [WITH_ICAL=no])
AC_SUBST(ICAL_CFLAGS)
AC_SUBST(ICAL_LIBS)
CXXFLAGS=$CXXFLAGS_system

# Check for CLucene availability
AC_ARG_WITH(clucene-lib-prefix,
  AC_HELP_STRING([--with-clucene-lib-prefix=PATH],[path to the clucene config file, e.g. /usr/lib/]),
  [CLUCENE_LIB_PREFIX=${withval}],[CLUCENE_LIB_PREFIX=/usr/lib])
AC_ARG_WITH(clucene-include-prefix,
  AC_HELP_STRING([--with-clucene-include-prefix=PATH],[path to the clucene include files, e.g. /usr/include/]),
  [CLUCENE_INCLUDE_PREFIX=${withval}],[CLUCENE_INCLUDE_PREFIX=/usr/include])
have_clucene=no
# preprocessor doesn't check in /usr/lib{,64}/ for include files
if test "x$CLUCENE_LIB_PREFIX" != "x"; then
   CXXFLAGS="$CXXFLAGS -I$CLUCENE_LIB_PREFIX"
   CPPFLAGS="$CPPFLAGS -I$CLUCENE_LIB_PREFIX"
fi
if test "x$CLUCENE_INCLUDE_PREFIX" != "x"; then
   CXXFLAGS="$CXXFLAGS -I$CLUCENE_INCLUDE_PREFIX"
   CPPFLAGS="$CPPFLAGS -I$CLUCENE_INCLUDE_PREFIX"
fi
# force add pthread in lucene test
CXXFLAGS="$CXXFLAGS -pthread"
if test -e "${CLUCENE_LIB_PREFIX}/libclucene.a"; then
# lucene::index::IndexReader::open(const char *) in c++ style
AC_CHECK_LIB(clucene, [_ZN6lucene5index11IndexReader4openEPKc],
  [ CLUCENE_LIBS="${CLUCENE_LIB_PREFIX}/libclucene.a"
    have_clucene=static
  ])
else
AC_CHECK_LIB(clucene, [_ZN6lucene5index11IndexReader4openEPKc],
  [ CLUCENE_LIBS="-lclucene"
    have_clucene=dynamic
  ])
fi
AC_MSG_CHECKING([library type for clucene])
AC_MSG_RESULT([$have_clucene])
AC_CHECK_HEADERS([CLucene.h],
  [ CLUCENE_CFLAGS="-DLUCENE_DISABLE_MEMTRACKING" ],
  [ have_clucene=no ])
if test "x$CLUCENE_LIB_PREFIX" != "x"; then
   CLUCENE_CFLAGS="$CLUCENE_CFLAGS -I$CLUCENE_LIB_PREFIX"
fi
if test "x$CLUCENE_INCLUDE_PREFIX" != "x"; then
   CLUCENE_CFLAGS="$CLUCENE_CFLAGS -I$CLUCENE_INCLUDE_PREFIX"
fi
AM_CONDITIONAL([WITH_CLUCENE], [test "$have_clucene" != "no"])
AC_SUBST(CLUCENE_CFLAGS)
AC_SUBST(CLUCENE_LIBS)
CXXFLAGS=$CXXFLAGS_system
CPPFLAGS=$CPPFLAGS_system

# Check for google perftools tcmalloc availability
if test "$want_tcmalloc" = "yes"; then
    AC_ARG_WITH(tcmalloc-prefix,
      AC_HELP_STRING([--with-tcmalloc-prefix=PATH],[path to tcmalloc lib, e.g. /usr/lib/]),
      [TCMALLOC_PREFIX=${withval}],[TCMALLOC_PREFIX=/usr/lib])
    AC_CHECK_LIB(tcmalloc_minimal, [open],
      [ TCMALLOC_LIBS="${TCMALLOC_PREFIX}/libtcmalloc_minimal.a" ],
      [ WITH_TCMALLOC=no ])
    AC_SUBST(TCMALLOC_LIBS)
else
	TCMALLOC_LIBS=""
fi

# Check for sparsehash availability
if test "$want_sparsehash" = "yes"; then
	AC_CHECK_HEADERS([google/sparse_hash_map], AC_DEFINE(HAVE_SPARSEHASH, [], [Google sparsehash available]), [])
fi

#
# Check for user plugin required libraries
#

AC_CHECK_HEADERS([ldap.h],
	[ LIBS=-lldap
	  AC_MSG_CHECKING([for -lldap without LDAP_DEPRECATED])
	  AC_LINK_IFELSE([ AC_LANG_SOURCE([#include <ldap.h>
int main() {
    LDAP *l = ldap_init("localhost",389);
}]) ],
		[ LDAP_LIBS=-lldap
		  LDAP_FLAGS=
		  want_ldap=yes
		  AC_MSG_RESULT([yes]) ],
		[ AC_MSG_RESULT([no])
		  CXXFLAGS="$CXXFLAGS -DLDAP_DEPRECATED=1"
		  AC_MSG_CHECKING([for -lldap with LDAP_DEPRECATED])
		  AC_LINK_IFELSE([
			AC_LANG_SOURCE([#include <ldap.h>
int main() {
    LDAP *l = ldap_init("localhost",389);
}]) ],
			[ LDAP_LIBS=-lldap
			  LDAP_FLAGS=-DLDAP_DEPRECATED=1
			  want_ldap=yes
			  AC_MSG_RESULT([yes]) ],
			[ LDAP_LIBS=
			  LDAP_CXXFLAGS=
			  want_ldap=no
			  AC_MSG_RESULT([no]) ]
			)
		  CXXFLAGS=$CXXFLAGS_system
		])
	], AC_MSG_RESULT([no]) )
LIBS=$LIBS_system

AC_SUBST(LDAP_LIBS)
AC_SUBST(LDAP_FLAGS)
AM_CONDITIONAL([WITH_LDAP], [test "$want_ldap" = yes])

LIBS=$LDAP_LIBS
CXXFLAGS="$CXXFLAGS $LDAP_FLAGS"
AC_CHECK_FUNCS([ldap_start_tls_s])
AC_CHECK_FUNCS([ldap_create_page_control])
LIBS=$LIBS_system
CXXFLAGS=$CXXFLAGS_system

AC_CHECK_HEADERS([shadow.h], [want_unix=yes], [want_unix=no])
AM_CONDITIONAL([WITH_UNIX], [test "$want_unix" = yes])

AC_MSG_CHECKING([for iconv])
AC_LINK_IFELSE([ AC_LANG_SOURCE([
#include <stdlib.h>
#include <iconv.h>
int main() {
iconv_t i; i = iconv_open("", "");
}]) ],
	[ AC_MSG_RESULT([yes]) ],
	[ AC_MSG_RESULT([no])
	  AC_MSG_CHECKING([for iconv in -liconv])
	  LIBS="-liconv"
	  AC_LINK_IFELSE([ AC_LANG_SOURCE([
#include <stdlib.h>
#include <iconv.h>
int main() {
iconv_t i; i = iconv_open("", "");
}]) ],
		[ AC_MSG_RESULT([yes])
		  ICONV_LIBS=$LIBS ],
		[ AC_MSG_RESULT([no])
		  AC_MSG_ERROR([required library libiconv missing or unusable]) ]) ])
AC_SUBST(ICONV_LIBS)
LIBS=$LIBS_system

AC_MSG_CHECKING([for crypt])
AC_LINK_IFELSE([ AC_LANG_SOURCE([int main() {
  char crypt();  crypt();
}]) ],
	[ AC_MSG_RESULT([yes]) ],
	[ AC_MSG_RESULT([no])
	  AC_CHECK_LIB([crypt], [crypt],, AC_MSG_ERROR([required library libcrypt missing or unusable])) ])
CRYPT_LIBS=$LIBS
LIBS=$LIBS_system
AC_SUBST(CRYPT_LIBS)

# Only specific links with uuid and dl, clients only with crypto and ssl for ssl connections
SSL_LIBS=
# TODO: version of ssl check??
# ssl before crypto, because of dependancies
AC_CHECK_LIB([ssl], [SSL_accept], [SSL_LIBS="$SSL_LIBS -lssl"], AC_MSG_ERROR([required library libssl missing or unusable]))
AC_CHECK_LIB([crypto], [BIO_accept], [SSL_LIBS="$SSL_LIBS -lcrypto"], AC_MSG_ERROR([required library libcrypto missing or unusable]))
AC_SUBST(SSL_LIBS)
LIBS=$LIBS_system

# OpenSSL check for new EVP_PKEY_cmp() function
LIBS="$LIBS $SSL_LIBS"
AC_CHECK_FUNCS([EVP_PKEY_cmp])
LIBS=$LIBS_system

# Check for pam library (optional)
PAM_LIBS=
AC_CHECK_LIB([pam], [pam_start],
		[PAM_LIBS="-lpam"]
		AC_DEFINE(HAVE_PAM, [1], [pam authentication available]),
		AC_MSG_RESULT([library libpam missing or unusable]))
AC_SUBST(PAM_LIBS)
AM_CONDITIONAL([WITH_PAM], [test "$PAM_LIBS" != ""])

# Check for kerberos library (optional)
AC_DEFUN([LIBKRB5_WITH_KRB5_CONFIG],[
  AC_ARG_WITH(krb5-config, AC_HELP_STRING([--with-krb5-config=PATH],[path to the krb5-config script of libkrb5]),
    [KRB5_CONFIG=${withval}],[KRB5_CONFIG=krb5-config])

  AC_MSG_CHECKING([for krb5-config presence])
  KRB5_CONFIG=`which $KRB5_CONFIG 2>/dev/null`
  if ! test -x "$KRB5_CONFIG"; then
    AC_MSG_RESULT([not found])
  else
    AC_MSG_RESULT([$KRB5_CONFIG])

    KRB5_CFLAGS=`$KRB5_CONFIG --cflags`
    KRB5_LIBS=`$KRB5_CONFIG --libs gssapi`

	dnl test if found krb5 is good enough version
	CXXFLAGS="$CXXFLAGS $KRB5_CFLAGS"
	LIBS=$KRB5_LIBS

	use_krb=2
	AC_CHECK_FUNCS([gss_acquire_cred], [AC_DEFINE(HAVE_GSSAPI, [1], [Kerberos Single Signon Authentication enabled]) ], [ use_krb=$((use_krb-1)) ])
	AC_CHECK_FUNCS([krb5_free_unparsed_name], [ AC_DEFINE(HAVE_KRB5, [1], [Kerberos Password Authentication enabled]) ], [ use_krb=$((use_krb-1)) ])
	AC_MSG_CHECKING([for krb5 usability])
	if test $use_krb -eq 0; then
		KRB5_CFLAGS=
		KRB5_LIBS=
		AC_MSG_RESULT([no])
	else
		AC_MSG_RESULT([yes])
	fi

	LIBS=$libs_keep
	CFLAGS=$cflags_keep
	AC_SUBST(KRB5_CFLAGS)
	AC_SUBST(KRB5_LIBS)
  fi
])
LIBKRB5_WITH_KRB5_CONFIG
AM_CONDITIONAL([WITH_KRB5], [test "$KRB5_LIBS" != ""])

# libcurl for report
if ! test "$want_oss" = "yes"; then
AC_CHECK_LIB([curl], [curl_easy_perform], [CURL_LIBS="-lcurl"], AC_MSG_ERROR([required library libcurl missing or unusable]))
AC_SUBST(CURL_LIBS)
fi # oss only

AC_CHECK_FUNCS([uuid_create], UUID_LIBS="",	dnl uuid_create is in libc on FreeBSD
  AC_CHECK_LIB([uuid], [uuid_create],
    UUID_LIBS="-luuid"
    AC_DEFINE(HAVE_UUID_CREATE),	dnl uuid_create is in libuuid on OpenBSD, also define such in config.h
    AC_CHECK_LIB([uuid], [uuid_generate], UUID_LIBS="-luuid", dnl uuid_generate in uuid on linux
      AC_MSG_ERROR([required library libuuid missing or unusable])
    )
  )
)
AC_SUBST(UUID_LIBS)

AC_CHECK_FUNCS([dlopen], DL_LIBS="", dnl dlopen is in libc on FreeBSD/OpenBSD
  AC_CHECK_LIB([dl], [dlopen], DL_LIBS="-ldl", dnl use -ldl on linux
    AC_MSG_ERROR([required library libdl missing or unusable])
  )
)
AC_SUBST(DL_LIBS)

AC_CHECK_LIB([z], [gzopen], GZ_LIBS="-lz",
  AC_MSG_ERROR([required library libz missing or unusable])
)
AC_SUBST(GZ_LIBS)

AC_CHECK_FUNCS([gettext], INTL_LIBS="", dnl gettext in libc
  AC_CHECK_LIB([intl], [gettext], INTL_LIBS="-lintl", dnl in -lintl
    AC_MSG_ERROR([required library libintl missing or unusable])
  )
)
AC_SUBST(INTL_LIBS)

# check epoll support
AC_ARG_ENABLE(epoll, AC_HELP_STRING([--enable-epoll], [enable building epoll socket handling]), [want_epoll=${enableval}], [want_epoll=no])
AM_CONDITIONAL(WITH_EPOLL, test "$want_epoll" = "yes")
if test "$want_epoll" = "yes"; then
AC_CHECK_FUNCS([epoll_create])
EPOLL_CFLAGS=-DUSE_EPOLL
else
EPOLL_CFLAGS=
fi
AC_SUBST(EPOLL_CFLAGS)

# Boost
BOOST_REQUIRE([1.35])
BOOST_SMART_PTR
BOOST_FILESYSTEM([mt])
BOOST_DATE_TIME([mt])

AC_ARG_ENABLE(icu, AC_HELP_STRING([--enable-icu],[enable icu support]), [want_icu=${enableval}],[want_icu=auto])
if test "$want_icu" = "no"; then
	have_icu=0
elif test "$want_icu" = "auto"; then
	AC_CHECK_ICU([3.4],
		[ have_icu=1 
		  AC_DEFINE([HAVE_ICU], [1], [Defined if the requested minimum ICU version is satisfied]) ],
		[ have_icu=0 ])
elif test "$want_icu" = "yes"; then
	AC_CHECK_ICU([3.4],
		[ have_icu=1 
		  AC_DEFINE([HAVE_ICU], [1], [Defined if the requested minimum ICU version is satisfied]) ])
fi
AM_CONDITIONAL([HAVE_ICU], [ test "$have_icu" -eq 1 ])

# all libs for programs, because functions are in static common stuff
PROG_LIBS="$UUID_LIBS $INTL_LIBS $ICONV_LIBS $GZ_LIBS $ICU_LIBS"
AC_SUBST(PROG_LIBS)

# done testing C++ requirements
AC_LANG_POP
# set out compileflags
CXXFLAGS="$CXXFLAGS $ZCXXFLAGS"
LDFLAGS="$LDFLAGS $ZLDFLAGS"

#
# Output files
#
AC_CONFIG_FILES([Makefile
		mapi4linux/Makefile
		mapi4linux/include/Makefile
		mapi4linux/src/Makefile
		php-ext/Makefile
		provider/Makefile
		provider/client/Makefile
		provider/contacts/Makefile
		provider/common/Makefile
		provider/include/Makefile
		provider/libserver/Makefile
		provider/plugins/Makefile
		provider/server/Makefile
		provider/soap/Makefile
		provider/tools/Makefile
		provider/tools/soapcpp2/Makefile
		inetmapi/Makefile
		spooler/Makefile
		spooler/python/Makefile
		spooler/python/plugins/Makefile
		common/Makefile
		ECtools/Makefile
		ECtools/zarafa-admin/Makefile
		ECtools/zarafa-passwd/Makefile
		ECtools/zarafa-monitor/Makefile
		ECtools/zarafa-fsck/Makefile
		ECtools/zarafa-cfgchecker/Makefile
		ECtools/zarafa-stats/Makefile
		ECtools/zarafa-archiver/Makefile
		ECtools/zarafa-search/Makefile
		ECtools/zarafa-utils/Makefile
		installer/Makefile
		installer/linux/Makefile
		installer/userscripts/Makefile
		installer/userscripts/createcompany
		installer/userscripts/createcompany.d/Makefile
		installer/userscripts/creategroup
		installer/userscripts/creategroup.d/Makefile
		installer/userscripts/createuser
		installer/userscripts/createuser.d/Makefile
		installer/userscripts/deletecompany
		installer/userscripts/deletecompany.d/Makefile
		installer/userscripts/deletegroup
		installer/userscripts/deletegroup.d/Makefile
		installer/userscripts/deleteuser
		installer/userscripts/deleteuser.d/Makefile
		installer/quotamail/Makefile
		installer/searchscripts/Makefile
		po/Makefile
		doc/Makefile
		gateway/Makefile
		libfreebusy/Makefile
		libicalmapi/Makefile
		caldav/Makefile
		swig/Makefile
		swig/python/Makefile
		zarafa-libsync/Makefile

		common/zarafa.pc
		version
		specialbuild
])
dnl non-oss files
if test -d `dirname $0`/licensed; then
AC_CONFIG_FILES([
		liblicense/Makefile
		licensed/Makefile
		ECtools/zarafa-backup/Makefile
		ECtools/zarafa-backup/helpers/Makefile
		ECtools/zarafa-report/Makefile
		ECtools/zarafa-msr/Makefile
		ECtools/zarafa-ocf/Makefile
		ECtools/za-extra/Makefile
])
fi

AC_OUTPUT

AC_MSG_RESULT([
Install prefix	:	$prefix
Mapiconfig dir	:	$MAPICONFIGDIR
Userscript dir	:	$USERSCRIPTDIR
Quota dir	:	$QUOTATEMPLATEDIR
Search scripts dir	:	$SEARCHSCRIPTSDIR

Build host	:	$host
Compile flags	:	$CXXFLAGS
Debug flags	:	$DEBUGFLAGS
PHP version	:	$PHP_VERSION
PHP ext dir	:	$PHP_EXTENSION_DIR
PHP config dir	:	$PHP_SYSCONF_DIR
PHP flags	:	$PHP_INCLUDES
PHP libs	:	$PHP_LIBS
gSOAP flags	:	$GSOAP_CFLAGS
gSOAP libs	:	$GSOAP_LIBS
OpenSSL libs	:	$SSL_LIBS
MySQL flags	:	$MYSQL_INCLUDES
MySQL libs	:	$MYSQL_LIBS
VMime flags	:	$VMIME_CFLAGS
VMime libs	:	$VMIME_LIBS
XML2 flags	:	$XML2_CFLAGS
XML2 libs	:	$XML2_LIBS
ICal flags	:	$ICAL_CFLAGS
ICal libs	:	$ICAL_LIBS
clucene flags	:	$CLUCENE_CFLAGS
clucene libs	:	$CLUCENE_LIBS
kyotocabinet libs	:	$KYOTOCABINET_LIBS
ncurses libs	:	$NCURSES_LIBS
Clients libs	:	$PROG_LIBS
xmlto	 	:	$PROG_XMLTO
LDAP support	:	$want_ldap
LDAP flags	:	$LDAP_FLAGS
LDAP libs	:	$LDAP_LIBS
Curl libs	:	$CURL_LIBS
Pam libs	:	$PAM_LIBS
Krb5 libs	:	$KRB5_LIBS
TCMalloc lib	:	$TCMALLOC_LIBS
Boost		:
 Flags		:	$BOOST_CPPFLAGS
 FS ldflags	:	$BOOST_FILESYSTEM_LDFLAGS
 FS libs	:	$BOOST_FILESYSTEM_LIBS
ICU libs	:	$ICU_LIBS
Swig 		:	$want_swig
Python module	:	$want_python
])
