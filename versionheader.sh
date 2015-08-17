#!/bin/bash

if [ -f revision ]; then
	svnrev="`cat revision | sed s/[^0-9]//g`"
else
	svnrev=$(svnversion -nc `dirname "$0"` | awk -F: '{print $NF}' | sed 's/[^0-9]//g')
	if [ -z "$svnrev" ]; then
	    if [ -f ".git/svn/.metadata" ]; then
		svnrev=$(git svn info | grep Revision | awk '{print $2}')
	    else
		svnrev=0
	    fi
	fi

fi

dot_version=`cat version`
major_version=`sed <version -e 's;\.[\.0-9a-zA-Z]*$;;'`
minor_version=`sed <version -e 's;^\([^.]*\)\.\([^.]*\)\(\.\([^.]*\)\)*;\2;'`
comma_version=`sed <version -e 's;\.[a-zA-Z]*$;;g' -e 's;\.;\,;g'`,$svnrev
specialbuild=`cat specialbuild`

cat << EOF
#define PROJECT_VERSION_SERVER          $comma_version
#define PROJECT_VERSION_SERVER_STR      "$comma_version"
#define PROJECT_VERSION_CLIENT          $comma_version
#define PROJECT_VERSION_CLIENT_STR      "$comma_version"
#define PROJECT_VERSION_EXT_STR         "$comma_version"
#define PROJECT_VERSION_SPOOLER_STR     "$comma_version"
#define PROJECT_VERSION_GATEWAY_STR     "$comma_version"
#define PROJECT_VERSION_CALDAV_STR      "$comma_version"
#define PROJECT_VERSION_DAGENT_STR      "$comma_version"
#define PROJECT_VERSION_PROFADMIN_STR   "$comma_version"
#define PROJECT_VERSION_MONITOR_STR     "$comma_version"
#define PROJECT_VERSION_PASSWD_STR      "$comma_version"
#define PROJECT_VERSION_FBSYNCER_STR    "$comma_version"
#define PROJECT_VERSION_SEARCH_STR      "$comma_version"
#define PROJECT_VERSION_ARCHIVER_STR    "$comma_version"
#define PROJECT_VERSION_DOT_STR         "$dot_version"
#define PROJECT_SPECIALBUILD            "$specialbuild"
#define PROJECT_SVN_REV_STR             "$svnrev"
#define PROJECT_VERSION_MAJOR           $major_version
#define PROJECT_VERSION_MINOR           $minor_version
#define PROJECT_VERSION_REVISION        $svnrev
EOF
