EXTRA_DIST = setup.py		\
	zarafa/__init__.py \
	zarafa/lockfile.py \
	zarafa/daemon/LICENSE \
	zarafa/daemon/__init__.py \
	zarafa/daemon/daemon.py \
	zarafa/daemon/pidlockfile.py \
	zarafa/daemon/runner.py \
	zarafa/daemon/version/__init__.py \
	zarafa/daemon/version/version_info.py

install-exec-local:
	cd ${srcdir} && ${PYTHON} setup.py install -f \
		$${DESTDIR+--root=${DESTDIR}} \
		--prefix="${prefix}" --install-purelib ${pythondir}
	rm -Rf build
