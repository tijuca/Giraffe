AC_PREREQ(2.60)
m4_define([zarafa_version], esyscmd(tr -d '\n' <version.in))
AC_INIT([Zarafa], [zarafa_version], [zarafa-dev@zarafa.com])
AC_CONFIG_SRCDIR([provider/server/ECServer.cpp])
AC_CONFIG_HEADERS([common/config.h])
AC_CONFIG_AUX_DIR([autoconf])
AC_CONFIG_MACRO_DIR([autoconf])
# because webaccess has too deep dirs for default tar-v7, we need something "better"
AM_INIT_AUTOMAKE([1.10 foreign tar-ustar subdir-objects])
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

abs_top_srcdir=`cd $srcdir; pwd`
abs_top_builddir=`pwd`

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

AC_PROG_YACC
if test x"$YACC" = "xyacc"; then
  AC_CHECK_PROG([YACC_EXISTS], [yacc], [yes], [no])
  if test x"$YACC_EXISTS" != xyes; then
    AC_MSG_ERROR([[bison/byacc/yacc not found.
          Please install bison]])
  fi
fi

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
AC_CANONICAL_HOST
BUILDOS=""
case "$host_os" in
	openbsd*)
		BUILDOS="-DOPENBSD"
		;;
	linux*)
		BUILDOS="-DLINUX"
		;;
esac

>conftest.cpp
ZPHPCXXFLAGS=""
if $CXX -Wno-narrowing -c conftest.cpp; then
	ZPHPCXXFLAGS="-Wno-narrowing"
fi
rm -f conftest.*
AC_SUBST([ZPHPCXXFLAGS])

# default to debug mode
# disable write-strings: too much complains about "" being 'cast' to char* (since const char* is not in the declarations) on g++-4.2 and higher
ZCPPFLAGS="$BUILDOS -DHAVE_CONFIG_H"
ZCXXFLAGS="-Wall -Wmissing-declarations -Wno-write-strings"
if ! test "$want_release" = "yes"; then
# when using -pedantic, -Wno-long-long is required
# only add -pedantic in non-release mode, since a header in php5 contains a , too much in an enum, which triggers a compile error.
  ZCXXFLAGS="$ZCXXFLAGS -pedantic -Wno-long-long"
fi

# programs that use files >2gb need 64bit file support
LFS_CFLAGS=`getconf LFS_CFLAGS`
ZCPPFLAGS="$ZCPPFLAGS $LFS_CFLAGS"

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
    ZCPPFLAGS="$ZCPPFLAGS -DNDEBUG -DFORCE_CHARSET_CONVERSION"
    ZCXXFLAGS="$ZCXXFLAGS -O2 -fno-strict-aliasing"
else
# -fPIC must be present on x86_64 when -O2 isn't used
    ZCXXFLAGS="$ZCXXFLAGS -fPIC"
fi

AC_ARG_ENABLE(unicode, AC_HELP_STRING([--enable-unicode],[enable unicode mode (compile flags -DUNICODE)]),
  [want_unicode=${enableval}],[want_unicode=yes])
if test "$want_unicode" = "yes"; then
    ZCPPFLAGS="$ZCPPFLAGS -DUNICODE"
fi

AC_ARG_ENABLE(tcmalloc, AC_HELP_STRING([--enable-tcmalloc],[enable use of tcmalloc minimal lib]),
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

AC_ARG_ENABLE(coverage,
  AC_HELP_STRING([--enable-coverage], [enable coverage (--coverage)]),
    [ if test "${enableval}" = "yes"; then
		ZCXXFLAGS="$ZCXXFLAGS -g --coverage -fprofile-arcs -ftest-coverage"
		ZLDFLAGS="$LDFLAGS -lgcov"
	  fi
    ])

# test offline possibility
AC_DEFUN([OFFLINE_CONFIG],[
  AC_ARG_ENABLE(offline, AC_HELP_STRING([--enable-offline],[Compile zarafa-server as the offline server]),
  [
  	ZCPPFLAGS="$ZCPPFLAGS -DHAVE_OFFLINE_SUPPORT"
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

# Timezones
AC_ARG_WITH(mapi-timezonedir,
  AC_HELP_STRING([--with-mapi-timezonedir=PATH],[path to install MAPI timezone definition files (default /usr/share/zarafa/timezones)]),
  [TIMEZONEDIR=${withval}],[TIMEZONEDIR=/usr/share/zarafa/timezones])
AC_SUBST(TIMEZONEDIR)

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
AC_TYPE_SSIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

# zarafa-stats requires ncurses(w) for --top
# check wide version before non-wide version
AC_DEFUN([LIBNCURSES_WITH_CONFIG],[
	AC_CHECK_PROGS([NCURSES_CONFIG], [ncursesw6-config ncursesw5-config ncurses6-config ncurses5-config ncursesw-config ncurses-config], [])
	AS_IF([test -n "$NCURSES_CONFIG"],
		[
			NCURSES_FLAGS=$("$NCURSES_CONFIG" --cflags)
			NCURSES_LIBS=$("$NCURSES_CONFIG" --libs)
		],
		[
			dnl just pick the lowest common denominator
			NCURSES_FLAGS=""
			NCURSES_LIBS="-lncurses"
		]
	)
	dnl We should be using ncurses.h - or is portability to non-ncurses
	dnl that important?
	AC_CHECK_HEADERS([curses.h])
	LIBS="$LIBS_system"
	CFLAGS="$CFLAGS_system"
	CXXFLAGS="$CXXFLAGS_system"
	AC_SUBST([NCURSES_FLAGS])
	AC_SUBST([NCURSES_LIBS])
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

AC_ARG_WITH([php], [AS_HELP_STRING([--without-php], [Disable building PHP components])])
AM_CONDITIONAL([WITH_PHP], [test "$with_php" != "no"])
AS_IF([test "$with_php" != "no"], [
	# Checks for PHP through php-config and tests the truth of
	# php-config results
	PHP_WITH_PHP_CONFIG
	PHP_CHECK_INCLUDES
])

# Checks for libxml2
AC_DEFUN([LIBXML_WITH_XML2_CONFIG],[
  AC_ARG_WITH(xml2-config, AC_HELP_STRING([--with-xml2-config=PATH],[path to the xml2-config script of libxml2]),
    [XML2_CONFIG=${withval}],[XML2_CONFIG=xml2-config])

  AC_MSG_CHECKING([for xml2-config presence])
  XML2_CONFIG=`which $XML2_CONFIG 2>/dev/null`
  if ! test -x "$XML2_CONFIG"; then
    AC_MSG_ERROR([cannot execute $XML2_CONFIG, install libxml2-dev])
  else
    AC_MSG_RESULT([$XML2_CONFIG])

    XML2_CFLAGS=`$XML2_CONFIG --cflags`
    XML2_LIBS=`$XML2_CONFIG --libs`

    AC_SUBST(XML2_CFLAGS)
    AC_SUBST(XML2_LIBS)

    dnl check correctness of lib
    CXXFLAGS="$CXXFLAGS $XML2_CFLAGS"
    LIBS="$LIBS $XML2_LIBS"
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
	ZCPPFLAGS="$ZCPPFLAGS -DEMBEDDED_MYSQL"
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
    AC_MSG_ERROR(Cannot find mysql_config. Please use --with-mysql-config=PATH or install libmysqlclient-dev)
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

#---------------------------------------------
# pthread setname (4 non-portable variants...)
#---------------------------------------------
LIBS="-pthread $LIBS"
AC_CHECK_HEADERS([pthread_np.h])
define(pthread_np_preamble,[
  #include <pthread.h>
  #if HAVE_PTHREAD_NP_H
  #  include <pthread_np.h>
  #endif
])
# 2-arg setname (e.g. Linux/glibc, QNX, IBM)
AC_MSG_CHECKING([for 2-arg pthread_setname_np])
AC_LINK_IFELSE([AC_LANG_PROGRAM(pthread_np_preamble, [
    pthread_setname_np(pthread_self(), "foo")
])], [
  AC_DEFINE(HAVE_PTHREAD_SETNAME_NP_2, 1, [2-arg pthread_setname_np])
  AC_MSG_RESULT([yes])
], [
  AC_MSG_RESULT([no])

  # 2-arg set_name (e.g. FreeBSD, OpenBSD)
  AC_MSG_CHECKING([for 2-arg pthread_set_name_np])
  AC_LINK_IFELSE([AC_LANG_PROGRAM(pthread_np_preamble, [
      return pthread_set_name_np(pthread_self(), "foo");
  ])], [
    AC_DEFINE(HAVE_PTHREAD_SET_NAME_NP_2, 1, [2-arg pthread_set_name_np])
    AC_MSG_RESULT([yes])
  ], [
    AC_MSG_RESULT([no])

    # 1-arg setname (e.g. Darwin)
    AC_MSG_CHECKING([for 1-arg pthread_setname_np])
    AC_LINK_IFELSE([AC_LANG_PROGRAM(pthread_np_preamble, [
        return pthread_setname_np("foo");
    ])], [
      AC_DEFINE(HAVE_PTHREAD_SETNAME_NP_1, 1, [1-arg pthread_setname_np])
      AC_MSG_RESULT([yes])
    ], [
      AC_MSG_RESULT([no])

      # 3-arg setname (e.g. NetBSD)
      AC_MSG_CHECKING([for 3-arg pthread_setname_np])
      AC_LINK_IFELSE([AC_LANG_PROGRAM(pthread_np_preamble, [
          return pthread_setname_np(pthread_self(), "foo", NULL);
      ])], [
        AC_DEFINE(HAVE_PTHREAD_SETNAME_NP_3, 1, [3-arg pthread_setname_np])
        AC_MSG_RESULT([yes])
      ], [
        AC_MSG_RESULT([no])
      ])
    ])
  ])
])
#---------------------------------------------
# end pthread_setname stuff
#---------------------------------------------

CXXFLAGS="$CXXFLAGS_system $ZCXXFLAGS"
LDFLAGS="$LDFLAGS_system $ZLDFLAGS"
AC_CHECK_FUNCS([pthread_getname_np])

VMIME_CFLAGS=""
VMIME_LIBS="-lvmime"

AC_ARG_WITH(vmime-prefix,
  AC_HELP_STRING([--with-vmime-prefix=PATH],[libvmime prefix, e.g. /usr/local, default /usr]),
  [
  VMIME_PREFIX="$withval"
  VMIME_CFLAGS="$VMIME_CFLAGS -I$VMIME_PREFIX/include"
  VMIME_LIBS="$VMIME_LIBS -L$VMIME_PREFIX/lib -L$VMIME_PREFIX/lib64 -R$VMIME_PREFIX/lib -R$VMIME_PREFIX/lib64"
  ])
AC_SUBST(VMIME_CFLAGS)
AC_SUBST(VMIME_LIBS)

GSOAP_CFLAGS="-DWITH_OPENSSL -DWITH_GZIP"
GSOAP_LIBS="-lgsoapssl++"
AC_MSG_CHECKING([gsoap])
AC_ARG_WITH(gsoap-prefix,
  AC_HELP_STRING([--with-gsoap-prefix=PATH],[gsoap prefix, e.g. /usr/local, default /usr]),
  [
  GSOAP_PREFIX="$withval"
  GSOAP_SOAPCPP2="$GSOAP_PREFIX/bin/soapcpp2"
  GSOAP_CFLAGS="$GSOAP_CFLAGS -I$GSOAP_PREFIX/include"
  # Hardcoding directories is bad… they may contain libraries of mismatching archictecture
  GSOAP_LIBS="$GSOAP_LIBS -L$GSOAP_PREFIX/lib -L$GSOAP_PREFIX/lib64 -R$GSOAP_PREFIX/lib -R$GSOAP_PREFIX/lib64"
  ],
  [
  GSOAP_PREFIX="/usr"
  GSOAP_SOAPCPP2="soapcpp2"
  ])
# Gsoap 2.8.13 introduced the -z1 flag for depreacted old-style C++ service proxies and objects ( https://www.cs.fsu.edu/~engelen/changelog.html )
GSOAP_SOAPCPP2=$(mkdir -p "conftest.$$" && cd "conftest.$$" && "$GSOAP_SOAPCPP2" -z1 </dev/null >/dev/null 2>/dev/null && echo "$GSOAP_SOAPCPP2 -z1 " || echo "$GSOAP_SOAPCPP2")
rm -Rf "conftest.$$"
AC_MSG_RESULT([$GSOAP_SOAPCPP2])
AC_SUBST(GSOAP_CFLAGS)
AC_SUBST(GSOAP_LIBS)
AC_SUBST(GSOAP_SOAPCPP2)

ICAL_CFLAGS=""
ICAL_LIBS="-lical -licalss"
AC_ARG_WITH(ical-prefix,
  AC_HELP_STRING([--with-ical-prefix=PATH],[libical prefix, e.g. /usr/local, default /usr]),
  [
  ICAL_PREFIX="${withval}"
  ICAL_CFLAGS="$ICAL_CFLAGS -I$ICAL_PREFIX/include"
  # Hardcoding directories is bad… they may contain libraries of mismatching archictecture
  ICAL_LIBS="$ICAL_LIBS -L$ICAL_PREFIX/lib -L$ICAL_PREFIX/lib64 -R$ICAL_PREFIX/lib -R$ICAL_PREFIX/lib64"
  ])
AC_SUBST(ICAL_CFLAGS)
AC_SUBST(ICAL_LIBS)

if test "$want_tcmalloc" = "yes"; then
	PKG_CHECK_MODULES([TCMALLOC], [libtcmalloc_minimal])
	CPPFLAGS="$CPPFLAGS $TCMALLOC_CFLAGS"
	AC_CHECK_HEADERS([gperftools/malloc_extension.h google/malloc_extension.h])
	CPPFLAGS="$CPPFLAGS_system"
	AC_DEFINE(HAVE_TCMALLOC, [1], [Google TCMalloc enabled])
fi

# Check for sparsehash availability
if test "$want_sparsehash" = "yes"; then
	AC_CHECK_HEADERS([google/sparse_hash_map], AC_DEFINE(HAVE_SPARSEHASH, [], [Google sparsehash available]), [])
fi

#
# Check for user plugin required libraries
#

AC_CHECK_HEADERS([ldap.h],
	[ LIBS="$LIBS -lldap_r"
	  CXXFLAGS="$CXXFLAGS -pthread"
	  AC_MSG_CHECKING([for -lldap_r without LDAP_DEPRECATED])
	  AC_LINK_IFELSE([ AC_LANG_SOURCE([#include <ldap.h>
int main() {
    LDAP *l = ldap_init("localhost",389);
}]) ],
		[ LDAP_LIBS=-lldap_r
		  LDAP_FLAGS=
		  want_ldap=yes
		  AC_MSG_RESULT([yes]) ],
		[ AC_MSG_RESULT([no])
		  CXXFLAGS="$CXXFLAGS -DLDAP_DEPRECATED=1"
		  AC_MSG_CHECKING([for -lldap_r with LDAP_DEPRECATED])
		  AC_LINK_IFELSE([
			AC_LANG_SOURCE([#include <ldap.h>
int main() {
    LDAP *l = ldap_init("localhost",389);
}]) ],
			[ LDAP_LIBS=-lldap_r
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

LIBS="$LIBS $LDAP_LIBS"
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
	  LIBS="$LIBS -liconv"
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
	  AC_CHECK_LIB([crypt], [crypt], [CRYPT_LIBS="-lcrypt"], AC_MSG_ERROR([required library libcrypt missing or unusable])) ])
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
	LIBS="$LIBS $KRB5_LIBS"

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

	LIBS="$LIBS_system"
	CFLAGS="$CFLAGS_system"
	CXXFLAGS="$CXXFLAGS_system"
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
dnl Set default project flags. On overriding AM_CXXFLAGS, include ${ZCXXFLAGS}
AM_CXXFLAGS="$ZCXXFLAGS"
AM_LDFLAGS="$ZLDFLAGS"
AC_SUBST([ZCPPFLAGS])
AC_SUBST([AM_CXXFLAGS])
AC_SUBST([AM_LDFLAGS])

#
# Output files
#
AC_CONFIG_FILES([Makefile
		mapi4linux/Makefile
		mapi4linux/include/Makefile
		mapi4linux/src/Makefile
		m4lcommon/Makefile
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
		swig/python/zarafa/Makefile
		zarafa-libsync/Makefile

		common/zarafa.pc
		default.sym
		mapi4linux/src/libmapi.sym
		version
		specialbuild
])
dnl non-oss files

AC_OUTPUT

AC_MSG_RESULT([
Install prefix	:	$prefix
Mapiconfig dir	:	$MAPICONFIGDIR
Userscript dir	:	$USERSCRIPTDIR
Quota dir	:	$QUOTATEMPLATEDIR
Search scripts dir	:	$SEARCHSCRIPTSDIR

Build host	:	$host
Compile flags	:	$ZCPPFLAGS $CPPFLAGS $ZCXXFLAGS $CXXFLAGS
Debug flags	:	$DEBUGFLAGS
PHP version	:	$PHP_VERSION
PHP ext dir	:	$PHP_EXTENSION_DIR
PHP config dir	:	$PHP_SYSCONF_DIR
PHP flags	:	$PHP_INCLUDES
PHP libs	:	$PHP_LIBS
gSOAP flags	:	$GSOAP_CFLAGS
gSOAP libs	:	$GSOAP_LIBS
gSOAP soapcpp2	:	$GSOAP_SOAPCPP2
OpenSSL libs	:	$SSL_LIBS
MySQL flags	:	$MYSQL_INCLUDES
MySQL libs	:	$MYSQL_LIBS
VMime flags	:	$VMIME_CFLAGS
VMime libs	:	$VMIME_LIBS
XML2 flags	:	$XML2_CFLAGS
XML2 libs	:	$XML2_LIBS
ICal flags	:	$ICAL_CFLAGS
ICal libs	:	$ICAL_LIBS
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
