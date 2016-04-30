# -*- Makefile -*-

dist_sbin_SCRIPTS = zarafa-presence

EXTRA_DIST = setup.py zarafa_presence/__init__.py \
             zarafa_presence/plugin_xmpp.py zarafa_presence/plugin_spreed.py

install-exec-local:
	cd ${srcdir} && ${PYTHON} setup.py install -f \
		$${DESTDIR+--root=${DESTDIR}} \
		--prefix="${prefix}" --install-purelib ${pythondir}
	rm -Rf build
