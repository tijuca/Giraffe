ACLOCAL_AMFLAGS = -I autoconf


SUBDIRS = . common m4lcommon mapi4linux libfreebusy zarafa-libsync \
          $(PROSUBS) provider libicalmapi inetmapi
if WITH_PHP
SUBDIRS += php-ext
endif
SUBDIRS += ECtools spooler gateway caldav installer po doc

if WITH_SWIG
SUBDIRS += swig
else
if WITH_PYTHON
SUBDIRS += swig
endif
endif

if !OSS_ONLY
SUBDIRS += testunit
endif

#EXTRA_DIST = version common/ecversion.h php-webclient-ajax
EXTRA_DIST = version versionheader.sh common/ecversion.h php-webclient-ajax \
	LICENSE.txt CONTRIBUTORS.txt AGPL-3 TRADEMARKS.txt \
	lddcheck global.am pwdcheck.sh

BUILT_SOURCES = source-moved-check common/ecversion.h

.PHONY: source-moved-check
source-moved-check:
	@${top_srcdir}/pwdcheck.sh "${abs_top_builddir}"

common/ecversion.h: versionheader.sh
	sh ${top_srcdir}/versionheader.sh > common/ecversion.h


dist-hook:
	find $(distdir) -type d -name .svn -exec rm -rf {} \; 2>/dev/null || true
if OSS_ONLY
	[ -x $(top_srcdir)/fixup-distro.sh ] && $(top_srcdir)/fixup-distro.sh $(distdir)
endif

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
