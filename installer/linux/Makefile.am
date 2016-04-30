# -*- Makefile -*-

systemunit_DATA = zarafa-dagent.service zarafa-gateway.service \
                  zarafa-ical.service zarafa-monitor.service \
                  zarafa-presence.service zarafa-search.service \
                  zarafa-server.service zarafa-spooler.service
sysusers_DATA = zarafa-sysusers.conf
tmpfiles_DATA = zarafa-tmpfiles.conf

config_files = \
	server.cfg unix.cfg ldap.propmap.cfg presence.cfg \
	spooler.cfg gateway.cfg monitor.cfg dagent.cfg ical.cfg search.cfg \
	autorespond archiver.cfg backup-plus.cfg

rhel_initscript_files = \
	zarafa-server.init.rhel zarafa-spooler.init.rhel \
	zarafa-gateway.init.rhel zarafa-monitor.init.rhel \
	zarafa-ical.init.rhel zarafa-dagent.init.rhel zarafa-search.init.rhel \
	zarafa-presence.init.rhel
suse_initscript_files = \
	zarafa-server.init.suse zarafa-spooler.init.suse \
	zarafa-gateway.init.suse zarafa-monitor.init.suse \
	zarafa-ical.init.suse zarafa-dagent.init.suse zarafa-search.init.suse \
	zarafa-presence.init.suse
sles_initscript_files = $(suse_initscript_files)
cbs4_initscript_files = \
	zarafa-server.init.cbs4 zarafa-spooler.init.cbs4 \
	zarafa-gateway.init.cbs4 zarafa-monitor.init.cbs4 \
	zarafa-ical.init.cbs4 zarafa-dagent.init.cbs4 zarafa-search.init.cbs4 \
	zarafa-presence.init.cbs4
alt_initscript_files = \
	zarafa-server.init.alt zarafa-spooler.init.alt \
	zarafa-gateway.init.alt zarafa-monitor.init.alt \
	zarafa-ical.init.alt zarafa-dagent.init.alt zarafa-search.init.alt \
	zarafa-presence.init.alt
debian_initscript_files = \
	zarafa-server.init.debi zarafa-spooler.init.debi \
	zarafa-gateway.init.debi zarafa-monitor.init.debi \
	zarafa-ical.init.debi zarafa-dagent.init.debi zarafa-search.init.debi \
	zarafa-presence.init.debi

data_configdir = ${docdir}/example-config
dist_data_config_DATA = ${config_files}

sysconf_configdir = ${sysconfdir}/zarafa
dist_sysconf_config_DATA = ${config_files}

sysconf_initdir=${sysconfdir}/init.d
sysconf_init_SCRIPTS = ${@DISTRO@_initscript_files}

rhel_sysconfig_path = ${sysconfdir}/sysconfig
suse_sysconfig_path = ${localstatedir}/adm/fillup-templates
sles_sysconfig_path = ${localstatedir}/adm/fillup-templates
sysconf_sysconfigdir = ${@DISTRO@_sysconfig_path}
sysconf_sysconfig_DATA = sysconfig.txt
alt_sysconfig_path = ${sysconfdir}/sysconfig

dist_doc_DATA = \
	db-convert-4.1-to-4.2 db-convert-4.20-to-4.21 db-convert-4.20-to-innodb.sql db-calc-storesize db-convert-attachments-to-files \
	createuser.dotforward db-convert-4.2x-to-5.00 ssl-certificates.sh db-upgrade-objectsid-to-objectguid.pl ldap-switch-sendas.pl \
	audit-parse.pl zarafa7-upgrade db-remove-orphaned-attachments \
	${top_srcdir}/tools/python-scripts/optimize-imap.py ${top_srcdir}/tools/python-scripts/update-resource-recipients

dist_pkgdata_DATA = ldap.m4

BUILT_SOURCES = ldap.openldap.cfg ldap.active-directory.cfg

ldap.openldap.cfg: ldap.m4
	m4 -DTYPE=OPENLDAP ${top_srcdir}/installer/linux/ldap.m4 > $@

ldap.active-directory.cfg: ldap.m4
	m4 -DTYPE=ADS ${top_srcdir}/installer/linux/ldap.m4 > $@

ldapms.openldap.cfg: ldap.m4
	m4 -DTYPE=OPENLDAP -DMULTISERVER=1 ${top_srcdir}/installer/linux/ldap.m4 > $@

ldapms.active-directory.cfg: ldap.m4
	m4 -DTYPE=ADS -DMULTISERVER=1 ${top_srcdir}/installer/linux/ldap.m4 > $@

dist_sbin_SCRIPTS = zarafa-autorespond zarafa-mr-accept zarafa-mr-process

sysconf_rotatedir = ${sysconfdir}/logrotate.d
dist_sysconf_rotate_DATA = zarafa.logrotate

sysconf_crondir = ${sysconfdir}/cron.daily
dist_sysconf_cron_SCRIPTS = zarafa-client-update.cron

sysconf_bcdir = ${sysconfdir}/bash_completion.d
dist_sysconf_bc_DATA = zarafa-bash-completion.sh

sysconf_certdir = ${sysconfdir}/zarafa/report-ca

config_files += $(BUILT_SOURCES)
EXTRA_DIST = $(BUILT_SOURCES) \
	$(rhel_initscript_files) $(suse_initscript_files) $(cbs4_initscript_files) $(alt_initscript_files) \
	sysconfig.txt

install-data-hook:
	install -d -m 755 ${DESTDIR}/${sysconf_configdir}/license
	install -d -m 755 ${DESTDIR}/${localstatedir}/lib/zarafa/report
	mv -v ${DESTDIR}${sysconf_rotatedir}/zarafa.logrotate ${DESTDIR}${sysconf_rotatedir}/zarafa
	mv -v ${DESTDIR}${sysconf_crondir}/zarafa-client-update.cron ${DESTDIR}${sysconf_crondir}/zarafa-client-update
	if test ${DISTRO} = "sles" -o ${DISTRO} = "suse"; then \
		mv -v ${DESTDIR}${sysconf_sysconfigdir}/sysconfig.txt ${DESTDIR}${sysconf_sysconfigdir}/sysconfig.zarafa; \
	elif test ${DISTRO} = "rhel" -o ${DISTRO} = "alt"; then \
		mv -v ${DESTDIR}${sysconf_sysconfigdir}/sysconfig.txt ${DESTDIR}${sysconf_sysconfigdir}/zarafa; \
	fi
	for f in ${sysconf_init_SCRIPTS}; do \
		newf=`echo "$$f" | sed -e 's/\(zarafa-.*\).init.*/\1/'`; \
		mv -v "${DESTDIR}${sysconf_initdir}/$$f" "${DESTDIR}${sysconf_initdir}/$$newf"; \
	done
	mkdir -p ${DESTDIR}/${docdir}/../zarafa-gateway
	mv -v ${DESTDIR}/${docdir}/optimize-imap.py ${DESTDIR}/${docdir}/../zarafa-gateway
	rm -f ${DESTDIR}/${sysconf_configdir}/example-msr.cfg
