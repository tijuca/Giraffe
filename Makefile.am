ACLOCAL_AMFLAGS = -I autoconf

if ! OSS_ONLY
PROSUBS = liblicense licensed
endif

SUBDIRS = common libfreebusy mapi4linux zarafa-libsync $(PROSUBS) provider libicalmapi inetmapi php-ext ECtools spooler gateway caldav installer po doc

if WITH_SWIG
SUBDIRS += swig
else
if WITH_PERL
SUBDIRS += swig
else
if WITH_PYTHON
SUBDIRS += swig
endif
endif
endif

EXTRA_DIST = version versionheader.sh common/ecversion.h php-webclient-ajax webapp/deploy

BUILT_SOURCES = common/ecversion.h

common/ecversion.h: versionheader.sh
	sh ${top_srcdir}/versionheader.sh > common/ecversion.h

webapp/deploy: webapp/build.xml
	if [ -z "$(PROG_ANT)" ]; then \
		echo ant is required for the \'make dist\' build step; \
		exit 0; \
	fi
# we force a clean, since if it's not clean, something will loop and eat up gigabytes of diskspace
	(cd webapp && ant clean && ant && ant deploy)

if ! OSS_ONLY
CLEANFILES=common/ecversion.h
endif

dist-hook: webapp/deploy
	find $(distdir) -type d -name .svn -exec rm -rf {} \; 2>/dev/null || true
if OSS_ONLY
	[ -x $(top_srcdir)/fixup-distro.sh ] && $(top_srcdir)/fixup-distro.sh $(distdir)
	rm -rf $(distdir)/webapp
endif

install-ajax-webaccess:
	install -d -m 755 $(DESTDIR)/usr/share
	cp -a $(top_srcdir)/php-webclient-ajax $(DESTDIR)/usr/share/zarafa-webaccess
	rmdir $(DESTDIR)/usr/share/zarafa-webaccess/plugins
	ln -sf /var/lib/zarafa-webaccess/plugins $(DESTDIR)/usr/share/zarafa-webaccess/plugins
	install -d -m 755 $(DESTDIR)/var/lib/zarafa-webaccess/plugins
	install -d -m 755 $(DESTDIR)/var/lib/zarafa-webaccess/tmp
	install -d -m 755 $(DESTDIR)/etc/zarafa/webaccess-ajax
	mv $(DESTDIR)/usr/share/zarafa-webaccess/config.php.dist $(DESTDIR)/etc/zarafa/webaccess-ajax/config.php
	ln -sf /etc/zarafa/webaccess-ajax/config.php $(DESTDIR)/usr/share/zarafa-webaccess/config.php
	rm -f $(DESTDIR)/usr/share/zarafa-webaccess/debug.php
	for po in $(DESTDIR)/usr/share/zarafa-webaccess/server/language/*/LC_MESSAGES/*.po; do \
		msgfmt -f -v -o $${po%.po}.mo $$po; \
		rm $$po; \
	done

install-webapp:
	if [ ! -d $(top_srcdir)/webapp/deploy ]; then \
		exit 0; \
	fi
	install -d -m 755 $(DESTDIR)/usr/share
	cp -a $(top_srcdir)/webapp/deploy $(DESTDIR)/usr/share/zarafa-webapp
	ln -sf /var/lib/zarafa-webapp/plugins $(DESTDIR)/usr/share/zarafa-webapp/plugins
	install -d -m 755 $(DESTDIR)/var/lib/zarafa-webapp/plugins
	install -d -m 755 $(DESTDIR)/var/lib/zarafa-webapp/tmp
	install -d -m 755 $(DESTDIR)/etc/zarafa/webapp
	mv $(DESTDIR)/usr/share/zarafa-webapp/config.php.dist $(DESTDIR)/etc/zarafa/webapp/config.php
	ln -sf /etc/zarafa/webapp/config.php $(DESTDIR)/usr/share/zarafa-webapp/config.php
