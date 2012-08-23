datadir = ${datarootdir}/locale/
EXTRA_DIST = da_DK.po de_DE.po es_CA.po es_ES.po fi_FI.po fr_BE.po fr_FR.po it_IT.po nl_BE.po nl_NL.po pt_BR.po pt_PT.po sv_SE.po ru_RU.po

install-data-local:
	mkdir -p $(DESTDIR)$(datadir)
	for po in $(EXTRA_DIST); do \
		lang=`echo $$po | cut -d_ -f1`; \
		sub="$(DESTDIR)$(datadir)/$$lang/LC_MESSAGES"; \
		mkdir -p $$sub; \
		msgfmt -f -v -o $$sub/zarafa.mo ${top_srcdir}/po/$$po; \
	done
