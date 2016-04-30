# -*- Makefile -*-

dist_sbin_SCRIPTS = zarafa-backup-plus

EXTRA_DIST = setup.py \
	zarafa_backup/__init__.py

install-exec-local:
	cd ${srcdir} && ${PYTHON} setup.py install -f \
		$${DESTDIR+--root=${DESTDIR}} \
		--prefix ${prefix} --install-purelib ${pythondir}
	rm -Rf build
