dist_data_DATA = 
datadir = ${docdir}

BUILT_SOURCES=zarafa.1
allpages=\
	zarafa-dagent.1 zarafa-passwd.1 zarafa-spooler.1 zarafa-admin.1 zarafa-server.1 zarafa-gateway.1 zarafa-monitor.1 \
	zarafa-backup.1	zarafa-restore.1 zarafa-ical.1 zarafa-licensed.1 zarafa-stats.1 zarafa-fsck.1 zarafa-indexer.1 zarafa-report.1 \
	zarafa-ical.cfg.5 zarafa-server.cfg.5 zarafa-ldap.cfg.5 zarafa-ldapms.cfg.5 zarafa-unix.cfg.5 zarafa-spooler.cfg.5 zarafa-gateway.cfg.5 \
	zarafa-monitor.cfg.5 zarafa-licensed.cfg.5 zarafa-dagent.cfg.5 zarafa-indexer.cfg.5 zarafa-archiver.1 zarafa-archiver.cfg.5 \
	zarafa-msr.1 zarafa-msr.cfg.5 zarafa-msr-verify.1 za-aclsync.1 zarafa-backup.cfg.5 za-restore.1

CLEANFILES = zarafa-cfgchecker.1

man_MANS=$(BUILT_SOURCES) $(allpages)

# Since the manual.xml file is in UTF-8, we need to call 'xmlto' in UTF-8 mode as well.
$(BUILT_SOURCES): manual.xml
	@if [ -z $(PROG_XMLTO) ]; then \
		echo Please install xmlto to generate manual pages; \
	else \
		LC_ALL=en_US.UTF-8 $(PROG_XMLTO) man ${top_srcdir}/doc/manual.xml; \
	fi

# force allpages to wait on output from xmlto
$(allpages): $(BUILT_SOURCES)

EXTRA_DIST = $(BUILT_SOURCES) $(allpages) manual.xml
