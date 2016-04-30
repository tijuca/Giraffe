# -*- Makefile -*-

dist_man_MANS = ${genpages}
genpages = \
	zarafa.7 \
	zarafa-admin.8 \
	zarafa-archiver.8 zarafa-archiver.cfg.5 \
	zarafa-autorespond.8 \
	zarafa-dagent.8 zarafa-dagent.cfg.5 \
	zarafa-mr-accept.8 zarafa-mr-process.8 \
	zarafa-fsck.1 \
	zarafa-gateway.8 zarafa-gateway.cfg.5 \
	zarafa-ical.8 zarafa-ical.cfg.5 \
	zarafa-ldap.cfg.5 \
	zarafa-mailbox-permissions.8 \
	zarafa-monitor.8 zarafa-monitor.cfg.5 \
	zarafa-passwd.1 \
	zarafa-search.8 zarafa-search.cfg.5 \
	zarafa-server.8 zarafa-server.cfg.5 \
	zarafa-set-oof.1 \
	zarafa-spooler.8 zarafa-spooler.cfg.5 \
	zarafa-backup-plus.8 \
	zarafa-stats.1 \
	zarafa-unix.cfg.5

CLEANFILES = ${genpages} zarafa.0
EXTRA_DIST = manual.xml

${genpages}: zarafa.0

# Since the manual.xml file is in UTF-8, we need to call 'xmlto' in UTF-8 mode as well.
zarafa.0: manual.xml
	@${AM_V_GEN} if [ -z "$(PROG_XMLTO)" ]; then \
		echo Please install xmlto to generate manual pages; \
	else \
		LC_ALL=en_US.UTF-8 $(PROG_XMLTO) man ${srcdir}/manual.xml && \
		touch $@; \
	fi
