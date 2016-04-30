ACLOCAL_AMFLAGS = -I autoconf


SUBDIRS = . common m4lcommon mapi4linux libfreebusy zarafa-libsync \
          provider libicalmapi inetmapi
if WITH_PHP
SUBDIRS += php-ext
endif
SUBDIRS += ECtools spooler gateway caldav installer po doc swig

#EXTRA_DIST = version php-webclient-ajax
EXTRA_DIST = version versionheader.sh php-webclient-ajax \
	LICENSE.txt CONTRIBUTORS.txt AGPL-3 \
	lddcheck global.am pwdcheck.sh

BUILT_SOURCES = source-moved-check common/include/zarafa/ecversion.h

sbin_SCRIPTS = tools/python-scripts/zarafa-cachestat.py

.PHONY: source-moved-check
source-moved-check:
	@${top_srcdir}/pwdcheck.sh "${abs_top_builddir}"

common/include/zarafa/ecversion.h: versionheader.sh
	sh ${top_srcdir}/versionheader.sh >$@

dist-hook:
	find $(distdir) -type d -name .svn -exec rm -rf {} \; 2>/dev/null || true

install-ajax-webaccess:
	install -d -m 755 $(DESTDIR)/usr/share
	cp -a $(top_srcdir)/php-webclient-ajax $(DESTDIR)/usr/share/zarafa-webaccess
	rm -rf $(DESTDIR)/usr/share/zarafa-webaccess/plugins
	install -d -m 755 $(DESTDIR)/var/lib/zarafa-webaccess/plugins
	install -d -m 755 $(DESTDIR)/var/lib/zarafa-webaccess/tmp
	install -d -m 755 $(DESTDIR)/etc/zarafa/webaccess-ajax
	mv $(DESTDIR)/usr/share/zarafa-webaccess/config.php.dist $(DESTDIR)/etc/zarafa/webaccess-ajax/config.php
	rm -f $(DESTDIR)/usr/share/zarafa-webaccess/debug.php
	for podir in $(DESTDIR)/usr/share/zarafa-webaccess/server/language/*; do \
		if [ `grep msgstr $$podir/LC_MESSAGES/*po | grep -v 'msgstr ""' |wc -l` = 0 ]; then \
			echo "Remove empty language $$podir"; \
			rm -rf $$podir; \
		fi; \
	done; \
	for po in $(DESTDIR)/usr/share/zarafa-webaccess/server/language/*/LC_MESSAGES/*.po; do \
		msgfmt -f -v -o $${po%.po}.mo $$po; \
		rm $$po; \
	done

default.sym: ${top_builddir}/config.status
