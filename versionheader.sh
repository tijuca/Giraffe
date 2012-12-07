#!/bin/sh

SVNCOPATH=`dirname $0`
svnrev=`svnversion -nc ${SVNCOPATH} | sed -e 's/^[^:]*://;s/[MS]//'`
if [ -z $svnrev ]; then
	svnrev=0
fi

dot_version=`cat version`
major_version=`cat version | sed -e 's;\.[\.0-9a-zA-Z]*$;;'`
minor_version=`cat version | sed -e 's;^\([^.]*\)\.\([^.]*\)\(\.\([^.]*\)\)*;\2;'`
comma_version=`cat version | sed -e 's;\.[a-zA-Z]*$;;g' -e 's;\.;\,;g'`,$svnrev
specialbuild=`cat specialbuild`

echo "#define PROJECT_VERSION_SERVER		$comma_version"
echo "#define PROJECT_VERSION_SERVER_STR	\"$comma_version\""
echo "#define PROJECT_VERSION_CLIENT		$comma_version"
echo "#define PROJECT_VERSION_CLIENT_STR	\"$comma_version\""
echo "#define PROJECT_VERSION_EXT_STR		\"$comma_version\""
echo "#define PROJECT_VERSION_SPOOLER_STR	\"$comma_version\""
echo "#define PROJECT_VERSION_GATEWAY_STR	\"$comma_version\""
echo "#define PROJECT_VERSION_CALDAV_STR	\"$comma_version\""
echo "#define PROJECT_VERSION_DAGENT_STR	\"$comma_version\""
echo "#define PROJECT_VERSION_PROFADMIN_STR	\"$comma_version\""
echo "#define PROJECT_VERSION_MONITOR_STR	\"$comma_version\""
echo "#define PROJECT_VERSION_PASSWD_STR	\"$comma_version\""
echo "#define PROJECT_VERSION_FBSYNCER_STR	\"$comma_version\""
echo "#define PROJECT_VERSION_INDEXER_STR	\"$comma_version\""
echo "#define PROJECT_VERSION_ARCHIVER_STR	\"$comma_version\""
echo "#define PROJECT_VERSION_DOT_STR		\"$dot_version\""
echo "#define PROJECT_SPECIALBUILD			\"$specialbuild\""
echo "#define PROJECT_SVN_REV_STR			\"$svnrev\""
echo "#define PROJECT_VERSION_MAJOR			$major_version"
echo "#define PROJECT_VERSION_MINOR			$minor_version"
echo "#define PROJECT_VERSION_REVISION			$svnrev"
