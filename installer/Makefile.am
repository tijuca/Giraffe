SUBDIRS = linux userscripts quotamail searchscripts

datadir = ${docdir}
dist_data_DATA = ldap/zarafa.schema zarafa.ldif
BUILT_SOURCES = zarafa.ldif
EXTRA_DIST = ldap/ol-schema-migrate.pl

$(BUILT_SOURCES): ldap/zarafa.schema ldap/ol-schema-migrate.pl
	${top_srcdir}/installer/ldap/ol-schema-migrate.pl -b ${top_srcdir}/installer/ldap/zarafa.schema > ${top_builddir}/installer/zarafa.ldif
