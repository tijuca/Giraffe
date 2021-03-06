# -*- Makefile -*-

systemunitdir = ${dcprefix}@systemunitdir@
sysusersdir = ${dcprefix}@sysusersdir@
tmpfilesdir = ${dcprefix}@tmpfilesdir@

dist_systemunit_DATA = kopano-dagent.service kopano-gateway.service \
                  kopano-ical.service kopano-monitor.service \
                  kopano-presence.service kopano-search.service \
                  kopano-server.service kopano-spooler.service \
                  kopano-spamd.service kopano-statsd.service
if WITH_PYTHON3
dist_systemunit_DATA += kopano-grapi.service
endif

dist_sysusers_DATA = kopano-sysusers.conf
dist_tmpfiles_DATA = kopano-tmpfiles.conf
dist_pkgdata_DATA = ldap.active-directory.cfg ldap.openldap.cfg ldap.propmap.cfg

config_files = \
	admin.cfg server.cfg unix.cfg presence.cfg \
	spooler.cfg gateway.cfg monitor.cfg dagent.cfg ical.cfg search.cfg \
	autorespond archiver.cfg backup.cfg migration-pst.cfg \
	autorespond.cfg spamd.cfg statsd.cfg
if WITH_PYTHON3
config_files += grapi.cfg
endif

data_configdir = ${docdir}/example-config
dist_data_config_DATA = ${config_files} ldap.cfg

dist_doc_DATA = \
	db-calc-storesize db-convert-attachments-to-files \
	createuser.dotforward ssl-certificates.sh ldap-switch-sendas.pl \
	audit-parse.pl db-remove-orphaned-attachments \
	${top_srcdir}/tools/python-scripts/update-resource-recipients
kgwdocdir = ${docdir}/../kopano-gateway
dist_kgwdoc_DATA = ${top_srcdir}/tools/python-scripts/optimize-imap.py

sysconf_apparmordir = ${docdir}/example-config/apparmor.d
dist_sysconf_apparmor_DATA = \
	kopano-coredump.aa kopano-client.aa \
	usr.sbin.kopano-dagent \
	usr.sbin.kopano-search \
	usr.sbin.kopano-server

BUILT_SOURCES = ldap.openldap.cfg ldap.active-directory.cfg

ldap.openldap.cfg: ldap.m4
	m4 -DTYPE=OPENLDAP ${top_srcdir}/installer/linux/ldap.m4 > $@

ldap.active-directory.cfg: ldap.m4
	m4 -DTYPE=ADS ${top_srcdir}/installer/linux/ldap.m4 > $@

dist_sbin_SCRIPTS = kopano-autorespond
if WITH_PYTHON3
dist_sbin_SCRIPTS += kopano-grapi
endif

sysconf_rotatedir = ${sysconfdir}/logrotate.d
dist_sysconf_rotate_DATA = kopano.logrotate

sysconf_bcdir = ${datadir}/bash-completion/completions
dist_sysconf_bc_DATA = kopano-bash-completion.sh

EXTRA_DIST = ${BUILT_SOURCES} ldap.m4 \
	kopano-server.init.rhel kopano-spooler.init.rhel \
	kopano-gateway.init.rhel kopano-monitor.init.rhel \
	kopano-ical.init.rhel kopano-dagent.init.rhel kopano-search.init.rhel \
	kopano-presence.init.rhel kopano-spamd.init.rhel

kopano-statsd.service: kopano-statsd.service.in ${top_builddir}/config.status
	${AM_V_GEN}${SED} -e 's|@''libexecdir''@|${libexecdir}|g' <$< >$@

install-data-hook:
	install -d -m 755 ${DESTDIR}/${sysconfdir}/kopano/license
	mkdir -p ${DESTDIR}${localstatedir}/lib/kopano/empty
	mv -v ${DESTDIR}${sysconf_rotatedir}/kopano.logrotate ${DESTDIR}${sysconf_rotatedir}/kopano

uninstall-local:
	rm -f "${DESTDIR}${sysconf_rotatedir}/kopano"
